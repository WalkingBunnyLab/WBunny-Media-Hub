// Real navigation/store/updater behavior for the standalone Bilibili milestone.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "app/app.hpp"
#include "app/personal.hpp"
#include "app/updater.hpp"
#include "core/app_identity.hpp"
#include "core/i18n.hpp"
#include "core/store.hpp"
#include "core/util.hpp"
#include "services/bilibili_account.hpp"
#include "player/player.hpp"

int main() {
    char folder[] = "/tmp/wbunny-identity-XXXXXX";
    assert(mkdtemp(folder));
    const std::string settings = std::string(folder) + "/" + identity::SETTINGS_FILE;
    store::load(settings);
    store::set_str("language", "fr");
    i18n::init("content");
    assert(i18n::LANGUAGE_COUNT == 3);
    assert(i18n::chosen() == i18n::system().code);
    for (const char* code : {"zh", "en", "ja"}) {
        i18n::choose(code);
        assert(i18n::chosen() == code && std::string(i18n::current().code) == code);
    }
    i18n::choose("en");
    namespace ba = bilibili::account;
    store::set_user("console-user-a");
    ba::save({"SESSDATA=fake", "fake-csrf", "123", "test"});
    assert(ba::session().uid == "123");
    const auto second = store::add_profile("Second", 1, false);
    assert(!second.empty() && store::switch_profile(second));
    assert(!ba::session().ok());
    assert(store::switch_profile("main") && ba::session().uid == "123");
    store::set_user("console-user-b"); assert(!ba::session().ok());
    store::set_user("console-user-a"); assert(ba::session().uid == "123");
    bilibili::Video v; v.bvid = "BV1GJ411x7h7"; v.title = "test"; v.author = "author"; v.ok = true;
    bilibili::Part p{12345678901LL, 1, "part"};
    ba::local_favorite(v); assert(ba::local_library(false).size() == 1);
    ba::local_favorite(v); assert(ba::local_library(false).empty());
    auto source = ba::playback(v, p, 480); assert(source.remember_position && source.start == 0);
    for (const auto& h : source.headers) assert(h.first != "Cookie");
    source.on_progress(0, false); assert(ba::local_library(true).empty());
    source.on_progress(15, false); assert(ba::local_library(true).size() == 1);
    store::Resume resume; resume.service = source.service; resume.id = source.id; resume.position = 30; resume.duration = 300;
    store::resume_save(resume); assert(ba::playback(v, p, 480).start == 30);
    assert(store::switch_profile(second)); source.on_stop(40, false); assert(ba::local_library(true).empty());
    assert(store::switch_profile("main")); ba::logout(); assert(!ba::session().ok());
    assert(personal::SECTION_COUNT == 5);
    assert(app::section_visible(app::SEC_HOME));
    assert(app::section_visible(app::SEC_BILIBILI));
    assert(app::section_visible(app::SEC_BILIBILI_LIVE));
    assert(personal::section_of_service("bilibili_live") == app::SEC_BILIBILI_LIVE);
    personal::set_start_id("bilibili_live");
    assert(personal::start_section() == app::SEC_BILIBILI_LIVE);
    personal::set_hidden(app::SEC_BILIBILI_LIVE, true);
    assert(!app::section_visible(app::SEC_BILIBILI_LIVE));
    assert(app::section_visible(app::SEC_BILIBILI));
    assert(personal::start_section() == app::SEC_HOME);
    personal::set_hidden(app::SEC_BILIBILI_LIVE, false);
    assert(personal::start_section() == app::SEC_BILIBILI_LIVE);
    for (auto section : {app::SEC_JELLYFIN, app::SEC_NAVIDROME, app::SEC_TWITCH,
                         app::SEC_RADIO, app::SEC_PODCASTS, app::SEC_MEDIA}) {
        assert(!app::section_allowed(section));
        assert(!app::section_visible(section));
    }
    personal::set_start_id("bilibili");
    assert(personal::start_section() == app::SEC_BILIBILI);
    personal::set_start_id("youtube");
    assert(personal::start_section() == app::SEC_BILIBILI);
    store::set_str("hidden_sections", "youtube");
    assert(!app::section_visible(app::SEC_BILIBILI));
    personal::set_hidden(app::SEC_BILIBILI, false);
    assert(app::section_visible(app::SEC_BILIBILI));
    assert(store::get_str("hidden_sections").empty());

    store::set_bool("update_auto", true);
    updater::init();
    assert(!updater::supported() && !updater::automatic());
    assert(!updater::supporter_available());
    updater::check();
    updater::tick();
    assert(updater::state() == updater::IDLE && updater::release().url.empty());
    assert(updater::set_developer(true) && updater::developer());
    assert(updater::set_developer(false) && !updater::developer());
    assert(updater::dev_server().empty());
    store::save_now();
    assert(util::file_exists(settings));
    assert(!util::file_exists(std::string(folder) + "/coffeeflix.json"));
    puts("PASS Bilibili-only sections, legacy preference aliases, isolated settings, and offline update mode");
}
