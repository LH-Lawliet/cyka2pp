#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace cyka::test {

inline int g_failed = 0;
inline int g_passed = 0;

inline void check(bool ok, std::string_view expr, std::string_view file, int line) {
    if (ok) {
        ++g_passed;
        return;
    }
    ++g_failed;
    std::cerr << file << ':' << line << ": CHECK failed: " << expr << '\n';
}

} // namespace cyka::test

#define CYKA_CHECK(expr) ::cyka::test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

void test_ratings();
void test_cli();
void test_vision();
void test_tags();
void test_golden();
void test_forfeit();
void test_corpus();
void test_ttd_trace();
void test_steam_id();
void test_ent_decode();
