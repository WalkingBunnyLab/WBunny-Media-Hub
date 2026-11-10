// WBunny Media Hub versions are kept in local Git and installed manually.
// No upstream release/supporter server or network update transport is active.
#include "app/updater.hpp"
#include "core/build_variant.hpp"
#include "core/store.hpp"
#include "core/version.hpp"

namespace updater {
namespace {
const std::string empty;
const std::string reason = "Local build: install WBunny Media Hub versions manually.";
const Release none;
const std::vector<Release> no_builds;
}
const char* version() { return APP_VERSION; }
bool supported() { return false; }
const std::string& unsupported_reason() { return reason; }
void init() {}
void tick() {}
void stop() {}
State state() { return IDLE; }
const Release& release() { return none; }
const std::string& error() { return empty; }
float progress() { return 0; }
int64_t last_checked() { return 0; }
void check() {}
void download() {}
void cancel() {}
bool cancelling() { return false; }
bool automatic() { return false; }
void set_automatic(bool) {}
void install_now() {}
bool has_previous() { return false; }
std::string previous_version() { return ""; }
void switch_to_previous() {}
bool dev_unlocked() { return true; }
bool set_dev_unlocked(bool) { return true; }
bool developer() { return store::get_bool("developer_mode", false); }
bool set_developer(bool on) { store::set_bool("developer_mode", on); return true; }
const std::string& dev_server() { return empty; }
const char* current_build_name() { return APP_BUILD_NAME; }
std::string selected_build() { return APP_BUILD_ID; }
const std::vector<Release>& developer_builds() { return no_builds; }
bool select_build(const std::string&) { return false; }
bool supporter_available() { return false; }
bool supporter_on() { return false; }
std::string supporter_email() { return ""; }
bool set_supporter_email(const std::string&) { return false; }
const std::string& supporter_message() { return empty; }
void finish_on_exit() {}
}  // namespace updater
