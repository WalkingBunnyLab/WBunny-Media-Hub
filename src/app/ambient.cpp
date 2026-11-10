#include "app/ambient.hpp"
#include "app/photo_frame.hpp"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "audio/mixer.hpp"
#include "core/i18n.hpp"
#include "core/store.hpp"
#include "core/util.hpp"
#include "gfx/anim.hpp"
#include "gfx/gfx.hpp"
#include "gfx/images.hpp"
#include "gfx/text.hpp"
#include "platform/platform.hpp"
#include "player/player.hpp"
#include "screens/widgets.hpp"
#include "ui/ui.hpp"

namespace ambient {

namespace {

using namespace ui;

// Hold the supplied launch artwork, then fade into the application.
constexpr float OUT = 1.3f, OUT_LEN = 0.6f;

float g_intro_t = 0;
bool g_intro_done = false;
bool g_intro_sound = false;

bool g_saver_on = false;
bool g_forced = false;  // started by hand (Settings > Start the photo frame now)
float g_saver = 0;      // 0..1 visibility
float g_saver_t = 0;    // seconds shown, drives the drift

void swallow(Input& in) {
    in.eat_all();
    in.pointer_moved = false;
    suspend_nav();
}

// Load packaged branding synchronously so the first frame matches the console splash.
const images::Image* branding(const char* name) {
    const std::string key = std::string("wbunny:branding:") + name;
    if (!images::has(key, images::NO_EVICT)) {
        std::string data;
        if (util::read_file(platform::content_dir() + "/ui/" + name, data))
            if (auto* surface = images::decode(data)) images::put(key, surface, images::NO_EVICT);
    }
    return images::get(key, 0, 0, images::NO_EVICT);
}

void logo(float cx, float cy, float r, float) {
    const auto* image = branding("logo.png");
    if (image && image->ready)
        gfx::image_contain(image->tex, image->w, image->h, Rect(cx-r, cy-r, r*2, r*2));
}

void draw_intro() {
    const float alpha = 1 - anim::smoothstep((g_intro_t - OUT) / OUT_LEN);
    gfx::push_alpha(alpha);
    const auto* image = branding("startup.png");
    if (image && image->ready)
        gfx::image_contain(image->tex, image->w, image->h, Rect(0, 0, W, H));
    gfx::pop_alpha();
}

const char* const WEEKDAYS[] = {N_("Sunday"), N_("Monday"), N_("Tuesday"), N_("Wednesday"),
                                N_("Thursday"), N_("Friday"), N_("Saturday")};

std::string date_line() {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    // Each language puts the parts in its own order.
    std::string s = tr("{weekday}, {month} {day}");
    s = util::replace_all(s, "{weekday}", tr(WEEKDAYS[lt.tm_wday]));
    s = util::replace_all(s, "{month}", i18n::month_name(lt.tm_mon));
    return util::replace_all(s, "{day}", std::to_string(lt.tm_mday));
}

// Slow Lissajous drift so nothing sits still long enough to burn in.
// `range` 0..1 of the room there is (a card that is plainly a card can move less than bare text does).
void drift(float w, float h, float& x, float& y, float range = 1.0f) {
    float s = g_saver_t;
    float mx = std::max(0.0f, (W - 120 - w) * 0.5f) * range, my = std::max(0.0f, (H - 100 - h) * 0.5f) * range;
    x = (W - w) * 0.5f + std::sin(s * 0.043f + 0.6f) * mx;
    y = (H - h) * 0.5f + std::sin(s * 0.031f) * my;
}

void draw_saver() {
    const Theme& t = theme();
    float a = anim::smoothstep(g_saver);
    gfx::push_alpha(a);
    gfx::fill_rect(Rect(0, 0, W, H), gfx::rgb(0x050407));
    // The photo frame, when that is the screensaver chosen and there are pictures to show.
    if (photo_frame::wanted() || g_forced) {
        if (photo_frame::draw(g_saver_t, ui::dt())) {
            gfx::pop_alpha();
            return;
        }
        if (g_forced && g_saver_t > 3.0f) {  // asked for it, and there was nothing: back to what was on, with a word
            g_forced = false;
            g_saver_on = false;
            ui::toast(tr("No photos found. Put some in the Photos folder."), ic::IMAGE);
            gfx::pop_alpha();
            return;
        }
        if (g_saver_t < 3.0f && photo_frame::wanted()) {  // looking: the dark, for a moment
            gfx::pop_alpha();
            return;
        }
    }

    float s = g_saver_t;
    bool music = player::active() && !player::has_video();
    player::Source src;
    std::string art;
    if (music) {
        src = player::source();
        art = player::artwork_key();
        if (art.empty()) art = src.artwork;
    }

    // Blurred artwork (or accent glows) washing slowly across the screen.
    const images::Image* blur = art.empty() ? nullptr : images::get(art, 0, 0, images::BLUR);
    if (blur && blur->ready) {
        float z = 1.25f + 0.08f * std::sin(s * 0.05f);
        Rect r(-W * (z - 1) * 0.5f + std::sin(s * 0.03f) * 60, -H * (z - 1) * 0.5f, W * z, H * z);
        gfx::image_cover(blur->tex, blur->w, blur->h, r, 0, Color(255, 255, 255, (uint8_t)(90 * images::fade(blur, 1.0f))));
    }
    gfx::glow(Rect(W * 0.5f - 700 + std::sin(s * 0.07f) * 300, -300 + std::cos(s * 0.05f) * 120, 1000, 900), t.accent.alpha(0.10f));
    gfx::glow(Rect(W * 0.5f - 200 + std::cos(s * 0.06f) * 320, H - 560 + std::sin(s * 0.08f) * 100, 1000, 900), t.accent2.alpha(0.08f));

    std::string clock = util::clock_hhmm();
    if (music) {
        // Now-playing card: artwork, title, artist and a live spectrum.
        float cw = 760, ch = 250, x, y;
        // The card has an edge, so its slow drift (it keeps the picture from burning in) reads as a card that is moving, not as
        // pieces of the screen that are loose.
        const float pad = 32;
        const bool bar = !player::live() && player::duration() > 0;
        const float panel_h = ch + pad * 2 + (bar ? 28 : 0);
        drift(cw + pad * 2, panel_h, x, y, 0.4f);
        x += pad;
        y += pad;
        const Rect panel(x - pad, y - pad, cw + pad * 2, panel_h);
        gfx::shadow(panel.offset(0, 14), 50, Color(0, 0, 0, 170));
        gfx::fill_rrect(panel, 34, Color(18, 15, 24, 214));
        gfx::stroke_rrect(panel, 34, 1.5f, Color(255, 255, 255, 46));
        Rect ar(x, y, ch, ch);
        const images::Image* img = art.empty() ? nullptr : images::get(art, 500, 500);
        gfx::shadow(ar.offset(0, 12), 40, Color(0, 0, 0, 180));
        if (img && img->ready) gfx::image_cover(img->tex, img->w, img->h, ar, 20);
        else {
            gfx::fill_rrect_vgrad(ar, 20, t.accent, t.accent2);
            text::icon(player::live() ? ic::RADIO : ic::MUSIC, 96, ar.cx(), ar.cy(), gfx::WHITE);
        }
        float tx = ar.r() + 36, tw = cw - ch - 36;
        std::string sub = player::stream_title();
        if (sub.empty()) sub = src.subtitle;
        text::draw(font::label, tx, y + 8, clock, t.text2);
        text::draw_fit(font::display, tx, y + 44, tw, src.title, t.text);
        if (!sub.empty()) text::draw_fit(font::title, tx, y + 104, tw, sub, t.text2);

        float lv[16];
        audio::levels(lv);
        float bw = tw / 16.0f;
        for (int i = 0; i < 16; i++) {
            float h = 6 + lv[i] * 64;
            Color c = gfx::lerp(t.accent, t.accent2, i / 15.0f).alpha(player::state() == player::PLAYING ? 0.9f : 0.35f);
            gfx::fill_rrect(Rect(tx + i * bw, y + ch - h, bw - 6, h), 3, c);
        }
        if (!player::live() && player::duration() > 0) {
            float p = (float)(player::position() / player::duration());
            gfx::fill_rrect(Rect(tx, y + ch + 22, tw, 4), 2, Color(255, 255, 255, 40));
            gfx::fill_rrect_hgrad(Rect(tx, y + ch + 22, tw * p, 4), 2, t.accent, t.accent2);
        }
    } else {
        // Big clock with the date.
        text::Font big = text::font(text::BOLD, 150);
        float cw = std::max(text::measure(big, clock), 420.0f), ch = 250, x, y;
        drift(cw, ch, x, y);
        float cx = x + cw * 0.5f;
        text::draw(big, cx, y, clock, t.text.alpha(0.92f), text::CENTER);
        text::draw(font::headline, cx, y + text::line_height(big) + 2, date_line(), t.text2, text::CENTER);
        float lx = cx - 70;
        logo(lx - 4, y - 36, 16, 0);
        text::draw(text::font(text::SEMIBOLD, 22), lx + 20, y - 50, "WBunny Media Hub", t.text3);
    }
    gfx::pop_alpha();
}

}  // namespace

void begin(bool video_foreground) {
    Input& in = input();
    float dt = ui::dt();
    // Scripted desktop runs skip the intro unless they ask for it.
    if (!g_intro_done && platform::scripted() && !getenv("COFFEEFLIX_INTRO")) g_intro_done = true;
    if (!g_intro_done) {
        if (!g_intro_sound) {
            audio::play(audio::SFX_OPEN, 0.7f);
            g_intro_sound = true;
        }
        g_intro_t += dt;
        if (in.any() && g_intro_t > 0.15f && g_intro_t < OUT) g_intro_t = OUT;  // any button skips
        if (g_intro_t >= OUT + OUT_LEN) g_intro_done = true;
        swallow(in);
        return;
    }

    // Read once a second: a background save holds the settings for a moment.
    static int after = 300;
    static float read_ago = 1e9f;
    read_ago += dt;
    if (read_ago >= 1.0f) {
        after = getenv("COFFEEFLIX_SAVER") ? 3 : (int)store::get_int("screensaver", 300);   // (the tests don't wait minutes)
        read_ago = 0;
    }
    bool want = (after > 0 && in.idle_time > after && !video_foreground && !screens::prompt_active()) || (g_forced && !video_foreground);
    if (g_saver_on && g_saver_t > 0.5f && in.any()) want = false;  // any input wakes (not the press that started it by hand)
    if (g_saver_on && !g_forced && in.idle_time < 0.5) want = false;
    if (!want) g_forced = false;
    if (want && !g_saver_on) {
        g_saver_t = 0;
        photo_frame::begin();
    }
    if (!want && g_saver_on) photo_frame::end();
    g_saver_on = want;
    g_saver = anim::clamp01(g_saver + (want ? dt / 1.6f : -dt / 0.3f));
    if (g_saver_on) g_saver_t += dt;
    // Swallow input while it shows or fades so the wake-up press does nothing else.
    if (g_saver > 0.0f) swallow(in);
}

void draw() {
    if (!g_intro_done) draw_intro();
    else if (g_saver > 0.0f) draw_saver();
}

bool intro_running() { return !g_intro_done; }

void show_now() {
    if (g_intro_done) g_forced = true;
}

}  // namespace ambient
