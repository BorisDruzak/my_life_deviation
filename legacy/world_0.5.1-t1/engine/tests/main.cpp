#include "test.hpp"
#include <algorithm>
int main(int argc, char * * argv) {
    auto & tests = test_registry();
    std::sort(tests.begin(), tests.end(),[](const auto & a, const auto & b) {
        return a.name < b.name;
    });
    if (argc == 2 && std::string(argv[1]) == "--list") {
        for (const auto & t : tests) std::cout << t.name << '\n';
        return 0;
    }
    int pass = 0, fail = 0;
    for (const auto & t : tests) {
        if (argc == 3 && std::string(argv[1]) == "--case" && t.name != argv[2]) continue;
        try {
            t.fn();
            ++ pass;
            std::cout << "PASS " << t.name << '\n';
        } catch (const std::exception & e) {
            ++ fail;
            std::cerr << "FAIL " << t.name << ": " << e.what() << '\n';
        }
    }
    std::cout << "RESULT passed=" << pass << " failed=" << fail << '\n';
    return(fail || ! pass) ? 1 : 0;
}
