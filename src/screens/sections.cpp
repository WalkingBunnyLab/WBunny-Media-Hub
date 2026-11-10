#include "core/store.hpp"
#include "screens/screens.hpp"

namespace screens {

std::unique_ptr<app::Screen> make_section_root(app::Section s) {
    // YouTube channel allowlists cannot authorize Bilibili videos.
    if (store::kids_profile() && (s == app::SEC_HOME || s == app::SEC_BILIBILI)) return make_bilibili();
    switch (s) {
        case app::SEC_SEARCH: return make_search();
        case app::SEC_BILIBILI: return make_bilibili();
        case app::SEC_BILIBILI_LIVE: return make_bilibili_live();
        case app::SEC_SETTINGS: return make_settings();
        default: return make_home();
    }
}

}  // namespace screens
