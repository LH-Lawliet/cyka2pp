#include "test_harness.hpp"

int main() {
    test_ratings();
    test_cli();
    test_vision();
    test_tags();
    test_golden();
    test_forfeit();
    test_steam_id();
    std::cout << "passed=" << cyka::test::g_passed << " failed=" << cyka::test::g_failed << '\n';
    return cyka::test::g_failed == 0 ? 0 : 1;
}
