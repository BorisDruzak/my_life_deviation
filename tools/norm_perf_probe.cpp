#include "life/archive.hpp"
#include "life/world.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using namespace life;

constexpr std::uint64_t default_capture_seconds=600;
constexpr std::size_t default_capture_limit=256;
constexpr std::size_t benchmark_repeats=64;
#ifdef _WIN32
constexpr double cpu_ticks_per_second=10'000'000.;
constexpr const char* cpu_timer_name="GetProcessTimes kernel+user CPU";
std::uint64_t process_cpu_ticks() {
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
        throw std::runtime_error("GetProcessTimes failed");
    auto ticks=[](FILETIME value){return (std::uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;};
    return ticks(kernel)+ticks(user);
}
#else
constexpr double cpu_ticks_per_second=CLOCKS_PER_SEC;
constexpr const char* cpu_timer_name="std::clock process CPU";
std::uint64_t process_cpu_ticks() {
    const auto value=std::clock();
    if(value==std::clock_t(-1))throw std::runtime_error("std::clock failed");
    return std::uint64_t(value);
}
#endif

struct ForecastSample {
    Id actor{};
    Tick at{};
    PersonalView view;
    PlanOption option;
};

struct SourceCounts {
    std::size_t evidence{},explicit_priors{},principle_transforms{},principle_qualifications{};
    std::size_t total() const {
        return evidence+explicit_priors+principle_transforms+principle_qualifications;
    }
};

struct ActorSize {
    Id actor{};
    std::size_t mind_bytes{},norm_memory_bytes{},norm_records{},records_over_hot_capacity{};
    std::size_t hot_prediction_cache_entries{};
    SourceCounts sources;
    std::uint64_t deep_evaluations{};
};

struct StateSize {
    Tick now{};
    std::string hash;
    std::size_t serialized_state_bytes{};
    std::vector<ActorSize> actors;
};

std::uint64_t unsigned_number(std::string_view text,const char* label) {
    std::uint64_t value=0;
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{}||end!=text.data()+text.size())
        throw std::invalid_argument(std::string("invalid ")+label+": "+std::string(text));
    return value;
}

SourceCounts source_counts(const NormMemory& memory) {
    SourceCounts result;
    for(const auto& record:memory.records()) {
        result.evidence+=record.sources.size();
        result.explicit_priors+=std::count(record.prior_source_known.begin(),
                                           record.prior_source_known.end(),true);
        result.explicit_priors+=record.severity_prior_source_known?1:0;
        result.explicit_priors+=record.personal.has_explicit_prior?1:0;
        result.principle_transforms+=record.personal.transforms.size();
        const auto& qualification=record.personal.qualification_source;
        if(qualification.delivery||qualification.known_root||qualification.revision||
           qualification.speaker||qualification.independently_grounded)
            ++result.principle_qualifications;
    }
    return result;
}

StateSize state_size(const World& world) {
    StateSize result;
    result.now=world.state().now;
    result.hash=world.hash();
    Writer state_writer;
    state_writer(world.state());
    result.serialized_state_bytes=state_writer.data.size();
    result.actors.reserve(world.state().actors.size());
    for(const auto& actor:world.state().actors) {
        Writer mind_writer,norm_writer;
        mind_writer(actor.mind);
        norm_writer(actor.mind.norm_memory);
        const auto& memory=actor.mind.norm_memory;
        ActorSize row;
        row.actor=actor.id;
        row.mind_bytes=mind_writer.data.size();
        row.norm_memory_bytes=norm_writer.data.size();
        row.norm_records=memory.size();
        row.records_over_hot_capacity=memory.size()>memory.profile().hot_record_limit?
            memory.size()-memory.profile().hot_record_limit:0;
        row.hot_prediction_cache_entries=memory.hot_cache_size();
        row.sources=source_counts(memory);
        row.deep_evaluations=actor.cog.norm.deep_evaluations;
        result.actors.push_back(row);
    }
    return result;
}

std::string samples_hash(const std::vector<ForecastSample>& samples) {
    Writer writer;
    writer(std::uint64_t(samples.size()));
    for(const auto& sample:samples)writer(sample.actor,sample.at,sample.view,sample.option);
    return hex_sha256(writer.data);
}

double percentile_microseconds(std::vector<std::chrono::nanoseconds> values,double fraction) {
    if(values.empty())return 0;
    std::sort(values.begin(),values.end());
    const auto rank=std::size_t(std::ceil(fraction*double(values.size())));
    const auto index=std::min(values.size()-1,rank?rank-1:0);
    return double(values[index].count())/1000.;
}

void write_source_counts(std::ostream& out,const SourceCounts& sources) {
    out<<"{\"cold_source_entries\":"<<sources.total()
       <<",\"evidence_contributions\":"<<sources.evidence
       <<",\"explicit_prior_sources\":"<<sources.explicit_priors
       <<",\"principle_transform_sources\":"<<sources.principle_transforms
       <<",\"principle_qualification_sources\":"<<sources.principle_qualifications<<'}';
}

void write_state_size(std::ostream& out,const StateSize& state) {
    out<<"{\"now_ms\":"<<state.now<<",\"hash\":\""<<state.hash
       <<"\",\"serialized_state_bytes\":"<<state.serialized_state_bytes
       <<",\"actors\":[";
    for(std::size_t i=0;i<state.actors.size();++i) {
        if(i)out<<',';
        const auto& actor=state.actors[i];
        out<<"{\"actor\":"<<actor.actor
           <<",\"serialized_mind_bytes\":"<<actor.mind_bytes
           <<",\"serialized_norm_memory_bytes\":"<<actor.norm_memory_bytes
           <<",\"norm_records\":"<<actor.norm_records
           <<",\"records_over_hot_capacity\":"<<actor.records_over_hot_capacity
           <<",\"hot_prediction_cache_entries\":"<<actor.hot_prediction_cache_entries
           <<",\"sources\":";
        write_source_counts(out,actor.sources);
        out<<",\"deep_evaluations\":"<<actor.deep_evaluations<<'}';
    }
    out<<"]}";
}

std::ofstream output_file(const std::filesystem::path& path) {
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("cannot write output: "+path.string());
    out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<std::setprecision(17);
    return out;
}

} // namespace

int main(int argc,char** argv) {
    try {
        std::locale::global(std::locale::classic());
        std::filesystem::path save_path,out_path;
        std::uint64_t capture_seconds=default_capture_seconds;
        std::size_t capture_limit=default_capture_limit;
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if(key=="--help") {
                std::cout<<"norm_perf_probe --save PATH --out JSON "
                            "[--seconds 600..3600] [--samples 256|512]\n";
                return 0;
            }
            if(i+1>=argc)throw std::invalid_argument("missing value for "+key);
            const std::string value=argv[++i];
            if(key=="--save") {
                if(!save_path.empty())throw std::invalid_argument("duplicated --save");
                save_path=value;
            } else if(key=="--out") {
                if(!out_path.empty())throw std::invalid_argument("duplicated --out");
                out_path=value;
            } else if(key=="--seconds") {
                capture_seconds=unsigned_number(value,"seconds");
            } else if(key=="--samples") {
                capture_limit=std::size_t(unsigned_number(value,"samples"));
            } else {
                throw std::invalid_argument("unknown argument: "+key);
            }
        }
        if(save_path.empty()||out_path.empty())
            throw std::invalid_argument("norm_perf_probe requires --save PATH --out JSON");
        if(capture_seconds<600||capture_seconds>3600)
            throw std::invalid_argument("seconds must be 600..3600");
        if(capture_limit!=256&&capture_limit!=512)
            throw std::invalid_argument("samples must be 256 or 512");

        auto world=World::load(save_path.string());
        const auto loaded=state_size(world);
        const Tick capture_started=world.state().now;
        std::vector<ForecastSample> samples;
        samples.reserve(capture_limit);
        world.set_thought_logger([&](const Thought& thought) {
            if(thought.kind!=ThoughtKind::Forecast||samples.size()>=capture_limit)return;
            if(!thought.actor||thought.actor>world.state().actors.size())
                throw std::logic_error("forecast thought has invalid actor");
            const auto& actor=world.state().actors[thought.actor-1];
            if(actor.cog.cursor>=actor.cog.options.size())
                throw std::logic_error("forecast thought has no current paid option");
            const auto& option=actor.cog.options[actor.cog.cursor];
            if(option.method!=thought.method||option.object!=thought.debug_object)
                throw std::logic_error("forecast thought and paid option differ");
            samples.push_back({thought.actor,thought.time,actor.cog.snapshot,option});
        });

        const Tick requested_span=Tick(capture_seconds*1000);
        if(capture_started>std::numeric_limits<Tick>::max()-requested_span)
            throw std::invalid_argument("capture horizon exceeds simulation time range");
        const Tick requested_end=capture_started+requested_span;
        while(world.state().now<requested_end&&samples.size()<capture_limit) {
            const auto remaining=requested_end-world.state().now;
            world.run_ms(std::uint64_t(std::min<Tick>(remaining,10'000)));
        }
        world.set_thought_logger({});
        world.validate();
        const auto before_benchmark=state_size(world);
        const auto input_hash_before=samples_hash(samples);
        const auto world_hash_before=world.hash();

        std::size_t prepared_norms=0,prepared_snapshots=0,max_prepared_norms=0;
        for(const auto& sample:samples) {
            const auto count=std::size_t(sample.view.norms_view.count);
            prepared_norms+=count;
            prepared_snapshots+=count?1:0;
            max_prepared_norms=std::max(max_prepared_norms,count);
        }

        bool scores_finite=true;
        double wall_score_checksum=0,cpu_score_checksum=0;
        std::vector<std::chrono::nanoseconds> wall_times;
        wall_times.reserve(samples.size()*benchmark_repeats);
        if(!samples.empty()) {
            const auto warmup=Planner::forecast(samples.front().view,samples.front().option);
            if(!std::isfinite(warmup.score)||!std::isfinite(warmup.norm_raw))
                throw std::runtime_error("non-finite warm-up forecast");
            for(std::size_t repeat=0;repeat<benchmark_repeats;++repeat) {
                for(const auto& sample:samples) {
                    const auto started=std::chrono::steady_clock::now();
                    const auto decision=Planner::forecast(sample.view,sample.option);
                    const auto finished=std::chrono::steady_clock::now();
                    wall_times.push_back(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(finished-started));
                    scores_finite=scores_finite&&std::isfinite(decision.score)&&
                        std::isfinite(decision.norm_raw);
                    wall_score_checksum+=decision.score+decision.norm_raw;
                }
            }
        }

        const auto cpu_started=process_cpu_ticks();
        for(std::size_t repeat=0;repeat<benchmark_repeats;++repeat) {
            for(const auto& sample:samples) {
                const auto decision=Planner::forecast(sample.view,sample.option);
                scores_finite=scores_finite&&std::isfinite(decision.score)&&
                    std::isfinite(decision.norm_raw);
                cpu_score_checksum+=decision.score+decision.norm_raw;
            }
        }
        const auto cpu_finished=process_cpu_ticks();
        if(cpu_finished<cpu_started)
            throw std::runtime_error("process CPU timer moved backwards");

        const auto input_hash_after=samples_hash(samples);
        const auto world_hash_after=world.hash();
        if(!scores_finite)throw std::runtime_error("non-finite forecast result");
        if(wall_score_checksum!=cpu_score_checksum)
            throw std::runtime_error("repeated pure forecasts produced different checksums");
        if(input_hash_before!=input_hash_after||world_hash_before!=world_hash_after)
            throw std::runtime_error("pure forecast benchmark mutated its input");

        const auto measured_calls=samples.size()*benchmark_repeats;
        const double cpu_seconds=samples.empty()?0:
            double(cpu_finished-cpu_started)/cpu_ticks_per_second;
        const double cpu_microseconds=cpu_seconds*1'000'000.;
        const double wall_p50=percentile_microseconds(wall_times,.50);
        const double wall_p95=percentile_microseconds(wall_times,.95);
        const auto capture_elapsed=world.state().now-capture_started;

        auto out=output_file(out_path);
        out<<"{\"schema\":\"norm-perf-probe-0.1\",\"save_format\":\""
           <<world.state().format<<"\",\"mode\":\""
           <<norm_mode_name(world.state().norm_profile.mode)<<"\",\"capture\":{"
           <<"\"requested_seconds\":"<<capture_seconds
           <<",\"simulated_ms\":"<<capture_elapsed
           <<",\"limit\":"<<capture_limit
           <<",\"captured_paid_forecasts\":"<<samples.size()
           <<",\"stopped_at_capture_limit\":"
           <<(samples.size()==capture_limit?"true":"false")
           <<",\"prepared_norms_total\":"<<prepared_norms
           <<",\"snapshots_with_prepared_norms\":"<<prepared_snapshots
           <<",\"max_prepared_norms_per_snapshot\":"<<max_prepared_norms<<"},"
           <<"\"loaded_state\":";
        write_state_size(out,loaded);
        out<<",\"pre_benchmark_state\":";
        write_state_size(out,before_benchmark);
        out<<",\"benchmark\":{\"available\":"<<(samples.empty()?"false":"true")
           <<",\"warmup_calls\":"<<(samples.empty()?0:1)
           <<",\"repeats_per_capture\":"<<benchmark_repeats
           <<",\"wall_measured_calls\":"<<measured_calls
           <<",\"wall_p50_microseconds\":"<<wall_p50
           <<",\"wall_p95_microseconds\":"<<wall_p95
           <<",\"wall_timer\":\"std::chrono::steady_clock\""
           <<",\"wall_scope\":\"Planner::forecast call and return construction; validation and result destruction excluded\""
           <<",\"cpu_measured_calls\":"<<measured_calls
           <<",\"cpu_aggregate_seconds\":"<<cpu_seconds
           <<",\"cpu_aggregate_microseconds\":"<<cpu_microseconds
           <<",\"cpu_microseconds_per_call\":"
           <<(measured_calls?cpu_microseconds/double(measured_calls):0)
           <<",\"cpu_timer\":\""<<cpu_timer_name<<"\""
           <<",\"cpu_clock_ticks_per_second\":"<<cpu_ticks_per_second
           <<",\"cpu_clock_unit_microseconds\":"
           <<1'000'000./cpu_ticks_per_second
           <<",\"cpu_scope\":\"separate aggregate forecast pass without per-call wall timer\""
           <<",\"all_scores_finite\":true"
           <<",\"wall_score_checksum\":"<<wall_score_checksum
           <<",\"cpu_score_checksum\":"<<cpu_score_checksum<<"},"
           <<"\"immutability\":{\"captured_inputs_hash_before\":\""<<input_hash_before
           <<"\",\"captured_inputs_hash_after\":\""<<input_hash_after
           <<"\",\"captured_inputs_unchanged\":true,\"world_hash_before\":\""
           <<world_hash_before<<"\",\"world_hash_after\":\""<<world_hash_after
           <<"\",\"world_unchanged\":true},"
           <<"\"measurement_notes\":{"
           <<"\"cold_source_definition\":\"serialized provenance entries in NormRecord sources, explicit priors, principle transforms and qualifications; prediction cache is derived and excluded from serialization\","
           <<"\"records_over_hot_capacity_definition\":\"max(norm_records-hot_record_limit,0); no physical cold-record partition is inferred\","
           <<"\"legacy_zero_capture_allowed\":true,"
           <<"\"simulation_horizon_is_capture_only\":true}}\n";
        out.close();
        std::cout<<"captured="<<samples.size()<<" wall_p50_us="<<wall_p50
                 <<" wall_p95_us="<<wall_p95<<" cpu_us="<<cpu_microseconds<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"ERROR: "<<error.what()<<'\n';
        return 1;
    }
}
