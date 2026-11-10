// Read-only login availability probe. Never prints QR keys, URLs or cookies.
#include "services/bilibili_account.hpp"
#include "core/http.hpp"
#include <cstdio>
int main() {
    http::init("");
    auto l = bilibili::account::generate(nullptr);
    if (!l.error.empty()) { printf("FAIL QR generation: %s\n", l.error.c_str()); http::shutdown(); return 1; }
    auto p = bilibili::account::poll(l.key, nullptr);
    bool ok = p.error.empty() && p.state == 86101;
    puts(ok ? "PASS live QR generation and waiting-for-scan poll (no account signed in)" : "FAIL initial QR poll");
    http::shutdown(); return ok ? 0 : 1;
}
