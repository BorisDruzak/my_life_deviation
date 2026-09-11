#include "test.hpp"
#include "../src/policy/persistent_map.hpp"
#include <map>
#include <random>
#include <vector>

TEST(PI_T1_persistent_index_matches_ordered_map_and_preserves_forks) {
    npc::policy::detail::PersistentMap<int> index;
    std::map<std::string,int> oracle;
    std::mt19937 rng(20260910);
    std::vector<std::pair<decltype(index),std::map<std::string,int>>> snapshots;
    for (int i=0;i<8000;++i) {
        auto key=std::to_string(rng()%1200);
        if (rng()%3==0) { index.erase(key); oracle.erase(key); }
        else { index.set(key,i); oracle[key]=i; }
        CHECK(index.size()==oracle.size());
        CHECK(index.height() <= 2*std::ceil(std::log2(static_cast<double>(index.size()+1)))+1);
        if (i%1000==0) snapshots.emplace_back(index,oracle);
    }
    snapshots.emplace_back(index,oracle);
    for (const auto& [saved,expected] : snapshots) {
        std::map<std::string,int> got;
        saved.visit(saved.size(),[&](const auto& key,int value){got[key]=value;});
        CHECK(got==expected);
        std::vector<std::string> reversed;
        saved.visit(4,[&](const auto& key,int){reversed.push_back(key);},true);
        auto it=expected.rbegin();
        for (const auto& key:reversed) { CHECK(it!=expected.rend()); CHECK(key==it->first); ++it; }
    }
}
