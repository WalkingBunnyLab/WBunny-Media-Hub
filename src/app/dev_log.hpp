#pragma once
// Compatibility API for local diagnostics; no log network transport in this app.
namespace dev_log {
void tick();
void shutdown();
void flush_now();
}
