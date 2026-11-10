#pragma once
#include "gfx/text.hpp"
// Original vector exports from the user-edited WBunny Figma component library.
// PNGs are build-time rasterizations; the runtime uses their alpha as an icon mask.
namespace text {
inline const char* design_icon_asset(int cp) {
    switch(cp) {
    case ic::HOME:return "home";
    case ic::SEARCH:return "search";
    case ic::SMART_DISPLAY:return "bilibili";
    case ic::SETTINGS:return "settings";
    case ic::ARROW_BACK:return "back";
    case ic::PLAY:return "play";
    case ic::PAUSE:return "pause";
    case ic::FOLDER:return "folder";
    case ic::FAVORITE:case ic::FAVORITE_BORDER:return "favorite";
    case ic::HISTORY:return "history";
    case ic::STAR:return "star";
    case ic::REFRESH:return "refresh";
    case ic::PERSON:return "person";
    case ic::CLOSE:return "close";
    case ic::CHECK:return "check";
    case ic::CHEVRON_RIGHT:return "chevron";
    case ic::CHEVRON_LEFT:return "chevron-left";
    case ic::VOLUME_UP:case ic::VOLUME_DOWN:return "volume";
    case ic::REPLAY_10:case ic::FAST_REWIND:return "rewind";
    case ic::FORWARD_10:case ic::FAST_FORWARD:return "forward";
    case ic::TV:case ic::LIVE_TV:return "tv";
    case ic::GAMEPAD:return "gamepad";
    case ic::SPEED:return "speed";
    case ic::SCHEDULE:case ic::WATCH_LATER:return "sleep";
    case ic::SUBTITLES:return "subtitles";
    case ic::LOCK:return "lock";
    case ic::INFO:return "info";
    case ic::ERROR:case ic::ERROR_OUTLINE:return "error";
    case ic::WIFI_OFF:return "offline";
    case ic::ADD:return "add";
    case ic::EDIT:return "edit";
    case ic::RADIO:return "radio";
    case ic::HD:return "hd";
    case ic::FULLSCREEN:return "expand";
    case ic::PHOTO:case ic::IMAGE:return "photo";
    case ic::DELETE:case ic::REMOVE:return "delete";
    case ic::HEADPHONES:case ic::AUDIOTRACK:return "headphones";
    case ic::TRENDING:return "trending";
    case ic::PLAY_CIRCLE:return "play-circle";
    case ic::LINK:return "link";
    case ic::TRANSLATE:return "translate";
    case ic::PALETTE:return "palette";
    case ic::TUNE:return "user-setting";
    case ic::LOGOUT:return "logout";
    case ic::VOLUME_OFF:return "sound";
    case ic::PEOPLE:return "user-setting";
    case ic::DESCRIPTION:return "file";
    case ic::BOOK:return "book";
    case ic::MENU:return "menu";
    default:return nullptr;
    }
}
}
