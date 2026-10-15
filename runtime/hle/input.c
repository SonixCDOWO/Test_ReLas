// Controller input: SDL keyboard/gamepad -> nn::hid Npad state.
//
// Both go through the player's bindings (runtime/hle/bindings.c, defaults there: keyboard WASD left stick, arrows
// d-pad, Space (or Enter) = A, Q (or Backspace) = B, Tab = X, R = Y, Z / C = L / R, 1 / 3 = ZL / ZR, = / - = Plus /
// Minus, numpad 8/4/5/6 right stick; controller by position, Switch A = east).
// Mouse: while the game aims a Poke Ball (it started the motion sensor) the window holds it (relative mode): moving it
// turns the virtual gyro, left click = A (throw). Otherwise it is free and is a finger on the touch screen (left
// button held; Play with Eevee pets this way in handheld mode). Right click = B either way. The menu (Esc) or leaving
// the window lets it go.
#include <SDL3/SDL.h>
#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../nvn/nvn.h"
#include "../nvn/renderer.h"
#include "../settings.h"
#include "../input_bind.h"

enum {
    BTN_A = 0, BTN_B, BTN_X, BTN_Y, BTN_LSTICK, BTN_RSTICK, BTN_L, BTN_R, BTN_ZL, BTN_ZR, BTN_PLUS, BTN_MINUS,
    BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_DOWN
};

_Static_assert(BTN_DOWN == BIND_DOWN && BTN_ZL == BIND_ZL, "bindings follow the button bits");

static _Atomic uint64_t key_buttons, pad_buttons, mouse_buttons;
static _Atomic int32_t key_lx, key_ly, key_rx, key_ry, pad_lx, pad_ly, pad_rx, pad_ry;
static uint8_t keys_down[SDL_SCANCODE_COUNT];
static SDL_Gamepad *pad;
static _Atomic int64_t sampling;

// Host keys / buttons held by a test script (LARECOMP_HOST_INPUT, input_frame): they go through the bindings
static uint8_t test_keys[SDL_SCANCODE_COUNT];
static _Atomic uint32_t test_pad;
static _Atomic int32_t test_lx, test_ly;  // (a resting stick's drift: "lstick" in LARECOMP_HOST_INPUT)
static bool host_test;  // (then a real controller is left out: the player's own pad must not spoil the test)

static int held_key(int sc) { return sc > 0 && sc < SDL_SCANCODE_COUNT && (keys_down[sc] || test_keys[sc]); }

// Through the bindings (runtime/hle/bindings.c: g_bind_key, and the fixed second keys), read on every key event, so
// a rebind applies at once.
static void recompute_keyboard(void) {
    uint64_t b = 0;
    for (int i = 0; i < BIND_PAD_COUNT; i++)
        if (held_key(g_bind_key[i]) || held_key(g_bind_key2[i])) b |= 1ull << i;
    key_buttons = b;
    key_lx = (held_key(g_bind_key[BIND_LS_RIGHT]) - held_key(g_bind_key[BIND_LS_LEFT])) * 32767;
    key_ly = (held_key(g_bind_key[BIND_LS_UP]) - held_key(g_bind_key[BIND_LS_DOWN])) * 32767;
    key_rx = (held_key(g_bind_key[BIND_RS_RIGHT]) - held_key(g_bind_key[BIND_RS_LEFT])) * 32767;
    key_ry = (held_key(g_bind_key[BIND_RS_UP]) - held_key(g_bind_key[BIND_RS_DOWN])) * 32767;
}

// Controller buttons (and the triggers as buttons) -> Switch buttons through g_bind_pad; the sticks stay the sticks.
static void recompute_pad(void) {
    uint32_t host = test_pad;
    // Scripted runs ignore the real controller: its state is polled here, not only through events, so the event
    // filter alone (input_user_blocked) let the player's controller drive a test instance (non-deterministic runs).
    static int user_off = -1;
    if (user_off < 0) {
        const char *v = getenv("LARECOMP_NO_USER_INPUT");
        user_off = v ? atoi(v) != 0 : getenv("LARECOMP_INPUT") != NULL;
    }
    if (pad && !host_test && !user_off) {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
            if (SDL_GetGamepadButton(pad, (SDL_GamepadButton)i)) host |= 1u << i;
        if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000) host |= 1u << PADB_LTRIGGER;
        if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) host |= 1u << PADB_RTRIGGER;
    }
    uint64_t b = 0;
    for (int i = 0; i < BIND_PAD_COUNT; i++)
        if (g_bind_pad[i] >= 0 && g_bind_pad[i] < PADB_HOST_COUNT && (host >> g_bind_pad[i] & 1)) b |= 1ull << i;
    pad_buttons = b;
    if (!pad || host_test || user_off) { pad_lx = test_lx, pad_ly = test_ly, pad_rx = pad_ry = 0; return; }
    pad_lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    pad_ly = -SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    pad_rx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
    pad_ry = -SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
}

// The mouse: held by the game window (relative mode) or free
static SDL_Window *mouse_win;
static _Atomic int gyro_aiming;  // the game has the motion sensor started (aiming): the mouse turns the gyro
static _Atomic int64_t mouse_acc_x, mouse_acc_y;  // motion not yet applied to the virtual gyro, in 1/1000 pixel
static _Atomic int touch_down;
static void mouse_release(void) {
    if (mouse_win) SDL_SetWindowRelativeMouseMode(mouse_win, false);
    mouse_win = NULL;
    mouse_buttons = 0;
    touch_down = 0;
}

// Touch screen: the free mouse over the game image, in the Switch's 1280x720 screen. g_present_rect: where the
// image is drawn in the window, in pixels (runtime/nvn/vulkan/device.c).
double g_present_rect[4] = {0, 0, 1280, 720};
static _Atomic int32_t touch_x = 640, touch_y = 360;
static void touch_at(SDL_Window *w, float mx, float my) {
    float d = w ? SDL_GetWindowPixelDensity(w) : 1;
    if (d <= 0) d = 1;
    double r0 = g_present_rect[0], r1 = g_present_rect[1], r2 = g_present_rect[2], r3 = g_present_rect[3];
    if (r2 <= 0 || r3 <= 0) return;
    double x = (mx * d - r0) / r2 * 1280, y = (my * d - r1) / r3 * 720;
    touch_x = (int32_t)(x < 0 ? 0 : x > 1279 ? 1279 : x);
    touch_y = (int32_t)(y < 0 ? 0 : y > 719 ? 719 : y);
}

// Forget held keys (the menu took focus) and let the mouse go; the gamepad is re-read on its next event.
void input_reset(void) {
    memset(keys_down, 0, sizeof keys_down);
    recompute_keyboard();
    mouse_release();
}

volatile int g_debug_frame_request;  // F9: dump the next frame's passes (build/pass_*.png) + draw log

// Events a test pushes (LARECOMP_HOST_INPUT type: / sdlkey: / sdlpad:) carry this device id: they go the whole way, menu first.
#define TEST_DEVICE 0x7E57u
static bool test_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: return e->key.which == TEST_DEVICE;
    case SDL_EVENT_TEXT_INPUT: return e->text.windowID == TEST_DEVICE;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP: return e->gbutton.which == TEST_DEVICE;
    default: return false;
    }
}

// Scripted test runs ignore the user's own input (a key or stick touched while a test drives Link spoils it):
// LARECOMP_NO_USER_INPUT=1, on by default whenever LARECOMP_INPUT / LARECOMP_DOORJUMP_TEST / LARECOMP_PLACE is set
// (or LARECOMP_HOST_INPUT / LARECOMP_BIND_TEST; LARECOMP_NO_USER_INPUT=0 lets it through again).
bool input_user_blocked(const SDL_Event *e) {
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("LARECOMP_NO_USER_INPUT");
        on = v ? atoi(v) != 0 : (getenv("LARECOMP_INPUT") || getenv("LARECOMP_DOORJUMP_TEST") || getenv("LARECOMP_PLACE") ||
                                 getenv("LARECOMP_HOST_INPUT") || getenv("LARECOMP_BIND_TEST"));
        if (on) LOG("[input] scripted run: keyboard, mouse and controllers ignored\n");
    }
    if (!on) return false;
    if (test_event(e)) return false;  // (LARECOMP_HOST_INPUT type: / sdlkey: / sdlpad: - the test's own presses)
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: case SDL_EVENT_TEXT_INPUT: case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_MOUSE_MOTION: case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_GAMEPAD_ADDED: case SDL_EVENT_GAMEPAD_AXIS_MOTION: case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: case SDL_EVENT_JOYSTICK_ADDED: case SDL_EVENT_JOYSTICK_AXIS_MOTION:
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN: case SDL_EVENT_JOYSTICK_BUTTON_UP:
        return true;
    default:
        return false;
    }
}

// Controller family for the button prompts (runtime/nvn/texpatch.c): 0 Nintendo / other / none, 1 Xbox, 2 PlayStation.
// LARECOMP_PAD_TYPE=xbox|playstation|nintendo pretends one is attached (tests).
static _Atomic int pad_family;
static void note_family(void) {
    int f = 0;
    if (pad) switch (SDL_GetGamepadType(pad)) {
        case SDL_GAMEPAD_TYPE_XBOX360: case SDL_GAMEPAD_TYPE_XBOXONE: f = 1; break;
        case SDL_GAMEPAD_TYPE_PS3: case SDL_GAMEPAD_TYPE_PS4: case SDL_GAMEPAD_TYPE_PS5: f = 2; break;
        default: break;
    }
    if (f != pad_family) LOG("[input] button prompts: %s controller\n", f == 1 ? "Xbox" : f == 2 ? "PlayStation" : "Nintendo / other");
    pad_family = f;
}
int input_pad_family(void) {
    static int env = -2;
    if (env == -2) {
        const char *e = getenv("LARECOMP_PAD_TYPE");
        env = !e ? -1 : !strcmp(e, "xbox") ? 1 : !strcmp(e, "playstation") || !strcmp(e, "ps") ? 2 : 0;
    }
    return env >= 0 ? env : pad_family;
}

void input_handle_event(const SDL_Event *e) {
    static bool scanned;  // pads present at startup: their ADDED event may have gone to the start screen's loop
    if (!scanned) {
        scanned = true;
        int n = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&n);
        if (n > 0 && !pad) { pad = SDL_OpenGamepad(ids[0]); LOG("[input] gamepad: %s\n", SDL_GetGamepadName(pad)); note_family(); }
        SDL_free(ids);
    }
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP:
        if (e->type == SDL_EVENT_KEY_DOWN && e->key.scancode == SDL_SCANCODE_F9 && !e->key.repeat) g_debug_frame_request = 1;
        if (e->key.scancode < SDL_SCANCODE_COUNT) keys_down[e->key.scancode] = e->type == SDL_EVENT_KEY_DOWN;
        recompute_keyboard();
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: case SDL_EVENT_MOUSE_MOTION: {
        bool motion = e->type == SDL_EVENT_MOUSE_MOTION;
        SDL_Window *w = SDL_GetWindowFromID(motion ? e->motion.windowID : e->button.windowID);
        // aiming: the window holds the mouse (the gyro); else it is free (the touch screen)
        if (gyro_aiming && !mouse_win && w) {
            mouse_win = w;
            SDL_SetWindowRelativeMouseMode(w, true);
            touch_down = 0;
        } else if (!gyro_aiming && mouse_win) {
            mouse_release();
        }
        if (motion) {
            if (mouse_win) {
                mouse_acc_x += (int64_t)(e->motion.xrel * 1000);
                mouse_acc_y += (int64_t)(e->motion.yrel * 1000);
            } else {
                touch_at(w, e->motion.x, e->motion.y);
            }
            break;
        }
        bool down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (e->button.button == SDL_BUTTON_RIGHT) {
            mouse_buttons = down ? mouse_buttons | 1ull << BTN_B : mouse_buttons & ~(1ull << BTN_B);
        } else if (e->button.button == SDL_BUTTON_LEFT) {
            if (mouse_win) mouse_buttons = down ? mouse_buttons | 1ull << BTN_A : mouse_buttons & ~(1ull << BTN_A);
            else { touch_at(w, e->button.x, e->button.y); touch_down = down; }
        }
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        mouse_release();
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!pad) { pad = SDL_OpenGamepad(e->gdevice.which); LOG("[input] gamepad: %s\n", SDL_GetGamepadName(pad)); note_family(); }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (pad && SDL_GetGamepadID(pad) == e->gdevice.which) { SDL_CloseGamepad(pad); pad = NULL; note_family(); }
        recompute_pad();
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP:
        recompute_pad();
        break;
    }
}

// Scripted input for testing: LARECOMP_INPUT="frame:BUTTON:frames,..." (BUTTON = A,B,X,Y,L,R,ZL,ZR,PLUS,
// MINUS,LEFT,UP,RIGHT,DOWN, or STICK_UP/DOWN/LEFT/RIGHT, RSTICK_UP/DOWN/LEFT/RIGHT, STICK_<degrees>: full tilt at
// that angle, 0 = right, 90 = up).
static _Atomic uint64_t script_buttons;
static _Atomic int32_t script_lx, script_ly, script_rx, script_ry;

// Tests of the bindings, through them like a real player:
// LARECOMP_HOST_INPUT="frame:key:<SDL key name>:frames,frame:pad:<button>:frames,..." holds a host key / controller
//   button (names as in larecomp.ini's bind_* lines: "Z", "Keypad 0", "south", "ltrigger"...);
//   "frame:lstick:<x>/<y>:frames" holds the controller's left stick there (raw SDL units, y up: a drifting stick).
// "frame:type:<text>" types <text> as real key events (down, text, up per character), through the overlay menu / name
//   entry first like a player's typing; "{Escape}" (any SDL key name) presses that key. No ',' or ':' in <text>.
// "frame:sdlpad:<button>:frames" / "frame:sdlkey:<key>:frames" hold a controller button / key as real events (down
//   at frame, up `frames` later): they reach menus and the on-screen keyboard first, as a player's would.
// "frame:shot:<name>" saves the window (game + menus) to build/<name>.png.
// LARECOMP_BIND_TEST="frame:pad|key:<input>:<host>,..." starts a rebind capture of <input> (bind_* name: a, zl,
//   ls_up...) at that frame (the Controls tab shows it waiting) and presses <host> 90 frames later ("esc" cancels,
//   "wait" lets it time out); "frame:reset:pad|key" resets to the defaults.
static void push_key(SDL_Scancode sc, SDL_Keymod mod, bool down) {
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.which = TEST_DEVICE, e.key.scancode = sc, e.key.mod = mod, e.key.down = down;
    e.key.key = SDL_GetKeyFromScancode(sc, mod, false);
    SDL_PushEvent(&e);
}
static void test_push(uint64_t frame, unsigned long start, unsigned long len, const char *dev, const char *arg) {
    SDL_Event e;
    memset(&e, 0, sizeof e);
    if (!strcmp(dev, "shot")) {
        extern char g_window_shot_request[256];  // runtime/hle/hookshot.c
        if (frame == start) snprintf(g_window_shot_request, sizeof g_window_shot_request, "build/%s.png", arg);
        return;
    }
    if (!strcmp(dev, "sdlkey")) {
        if (frame == start || frame == start + len) push_key(SDL_GetScancodeFromName(arg), SDL_KMOD_NONE, frame == start);
        return;
    }
    if (!strcmp(dev, "sdlpad")) {
        int b = bind_pad_from_name(arg);
        if (b < 0 || b >= SDL_GAMEPAD_BUTTON_COUNT || (frame != start && frame != start + len)) return;
        e.type = frame == start ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.which = TEST_DEVICE, e.gbutton.button = (Uint8)b, e.gbutton.down = frame == start;
        SDL_PushEvent(&e);
        return;
    }
    if (frame != start) return;
    LOG("[input] test: typing \"%s\"\n", arg);
    static char ascii[128][2];  // (a text event keeps a pointer to its text: these live on)
    for (const char *p = arg; *p; p++) {
        if (*p == '{' && strchr(p, '}')) {  // a named key
            char nm[32];
            size_t n = (size_t)(strchr(p, '}') - p - 1);
            snprintf(nm, sizeof nm, "%.*s", (int)(n < sizeof nm ? n : sizeof nm - 1), p + 1);
            SDL_Scancode sc = SDL_GetScancodeFromName(nm);
            push_key(sc, SDL_KMOD_NONE, true), push_key(sc, SDL_KMOD_NONE, false);
            p += n + 1;
            continue;
        }
        SDL_Keymod mod = SDL_KMOD_NONE;
        SDL_Scancode sc = SDL_GetScancodeFromKey((SDL_Keycode)tolower((unsigned char)*p), &mod);
        if (isupper((unsigned char)*p)) mod |= SDL_KMOD_LSHIFT;
        push_key(sc, mod, true);
        int c = *p & 0x7f;
        ascii[c][0] = (char)c;
        memset(&e, 0, sizeof e);
        e.type = SDL_EVENT_TEXT_INPUT, e.text.windowID = TEST_DEVICE, e.text.text = ascii[c];
        SDL_PushEvent(&e);
        push_key(sc, mod, false);
    }
}

static void host_test_frame(uint64_t frame) {
    static const char *hspec, *bspec;
    static int init;
    if (!init) { init = 1; hspec = getenv("LARECOMP_HOST_INPUT"); bspec = getenv("LARECOMP_BIND_TEST"); host_test = hspec; }
    char buf[2048];
    if (hspec) {
        uint8_t keys[SDL_SCANCODE_COUNT] = {0};
        uint32_t padm = 0;
        int32_t sx = 0, sy = 0;
        snprintf(buf, sizeof buf, "%s", hspec);
        char *save = NULL;
        for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            unsigned long start = 0, len = 10;
            char dev[8] = "", name[400] = "";
            if (sscanf(tok, "%lu:%7[a-z]:%399[^:]:%lu", &start, dev, name, &len) < 3) continue;
            if (!strcmp(dev, "type") || !strcmp(dev, "sdlpad") || !strcmp(dev, "sdlkey") || !strcmp(dev, "shot")) {
                test_push(frame, start, len, dev, name);
                continue;
            }
            if (frame < start || frame >= start + len) continue;
            if (!strcmp(dev, "key")) { SDL_Scancode sc = SDL_GetScancodeFromName(name); if (sc) keys[sc] = 1; }
            else if (!strcmp(dev, "lstick")) sscanf(name, "%d/%d", &sx, &sy);
            else { int h = bind_pad_from_name(name); if (h >= 0) padm |= 1u << h; }
        }
        bool kch = memcmp(keys, test_keys, sizeof keys) != 0, pch = padm != test_pad || sx != test_lx || sy != test_ly;
        memcpy(test_keys, keys, sizeof keys);
        test_pad = padm;
        test_lx = sx, test_ly = sy;
        if (kch) recompute_keyboard();
        if (pch) recompute_pad();
    }
    if (bspec) {
        snprintf(buf, sizeof buf, "%s", bspec);
        char *save = NULL;
        for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            unsigned long start = 0;
            char dev[8] = "", in[16] = "", host[40] = "";
            if (sscanf(tok, "%lu:%7[a-z]:%15[a-z_]:%39[^:]", &start, dev, in, host) < 3) continue;
            if (!strcmp(dev, "reset")) { if (frame == start) bind_reset(!strcmp(in, "key") ? BIND_DEV_KEY : BIND_DEV_PAD); continue; }
            int d = !strcmp(dev, "key") ? BIND_DEV_KEY : BIND_DEV_PAD;
            if (frame == start) {
                LOG("[input] test: capture %s %s\n", dev, in);
                bind_capture_begin(d, bind_slot_from_name(in));
            }
            if (frame == start + 90 && strcmp(host, "wait")) {
                SDL_Event e;
                memset(&e, 0, sizeof e);
                if (!strcmp(host, "esc") || d == BIND_DEV_KEY) {
                    e.type = SDL_EVENT_KEY_DOWN;
                    e.key.scancode = !strcmp(host, "esc") ? SDL_SCANCODE_ESCAPE : SDL_GetScancodeFromName(host);
                } else {
                    int h = bind_pad_from_name(host);
                    if (h >= PADB_LTRIGGER) {
                        e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
                        e.gaxis.axis = h == PADB_LTRIGGER ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
                        e.gaxis.value = 32767;
                    } else {
                        e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                        e.gbutton.button = (Uint8)h;
                    }
                }
                bind_capture_event(&e);
            }
        }
    }
}

static uint64_t g_pad_obj;
volatile int g_inject_plus;  // frames to hold Plus for the game (movie.c: A/B skip the opening on rando seeds)
RT_ON_RESET(input_reboot_reset) { g_pad_obj = 0; g_inject_plus = 0; }  // (reboot: the controller object was guest memory)
void input_frame(uint64_t frame) {
    if (g_pad_obj && getenv("LARECOMP_DUMP_PAD") && frame % 600 == 0) {
        const uint8_t *m = (const uint8_t *)(uintptr_t)(g_pad_obj - 0x98);
        LOG("[pad] mgr %llx 4e0=%u 4e4=%u 4e8=%u 548=%#x 54c..=%02x %02x %02x 56c=%02x\n", g_pad_obj - 0x98,
            *(uint32_t *)(m + 0x4e0), *(uint32_t *)(m + 0x4e4), *(uint32_t *)(m + 0x4e8), *(uint32_t *)(m + 0x548),
            m[0x54c], m[0x54d], m[0x54e], m[0x56c]);
        for (int i = 0; i < 9; i++) {
            const uint8_t *o = m + 0x98 + 0x48 * i;
            uint64_t player = *(uint64_t *)(m + 0x10 * i);
            LOG("[pad]  ctl%d id %#x style %#x btn %#x conn %d %d | player %llx [8]=%u\n", i, *(uint32_t *)(o + 8),
                *(uint32_t *)(o + 0xc), *(uint32_t *)(o + 0x14), o[0x44], o[0x45], player,
                player ? *(uint32_t *)(uintptr_t)(player + 8) : 0);
        }
    }
    extern void watch_arm(void);
    watch_arm();
    host_test_frame(frame);
    const char *spec = getenv("LARECOMP_INPUT");
    if (!spec) return;
    static const char *const names[] = {"A", "B", "X", "Y", "LSTICK", "RSTICK", "L", "R", "ZL", "ZR", "PLUS",
                                        "MINUS", "LEFT", "UP", "RIGHT", "DOWN"};
    uint64_t b = 0;
    int32_t lx = 0, ly = 0, rx = 0, ry = 0;
    static char buf[16384];  // (long scripted sessions: the perf walk is ~4 KB)
    snprintf(buf, sizeof buf, "%s", spec);
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        unsigned long start = 0, len = 10;
        char name[32] = "";
        if (sscanf(tok, "%lu:%31[A-Z_0-9]:%lu", &start, name, &len) < 2) continue;
        if (frame < start || frame >= start + len) continue;
        if (!strcmp(name, "STICK_UP")) ly = 32767;
        else if (!strcmp(name, "STICK_DOWN")) ly = -32767;
        else if (!strcmp(name, "STICK_LEFT")) lx = -32767;
        else if (!strcmp(name, "STICK_RIGHT")) lx = 32767;
        else if (!strcmp(name, "RSTICK_UP")) ry = 32767;
        else if (!strcmp(name, "RSTICK_DOWN")) ry = -32767;
        else if (!strcmp(name, "RSTICK_LEFT")) rx = -32767;
        else if (!strcmp(name, "RSTICK_RIGHT")) rx = 32767;
        else if (!strncmp(name, "STICK_", 6) && name[6] >= '0' && name[6] <= '9') {
            double r = atoi(name + 6) * 3.14159265358979 / 180;
            lx = (int32_t)lround(cos(r) * 32767), ly = (int32_t)lround(sin(r) * 32767);
        }
        for (int i = 0; i < 16; i++) if (!strcmp(name, names[i])) b |= 1ull << i;
    }
    {  // test hooks steering the stick (runtime/hle/doorjump.c LARECOMP_DOORJUMP_TEST)
        extern int32_t g_test_lx, g_test_ly;
        extern uint64_t g_test_stick_until;
        if (frame < g_test_stick_until) { lx = g_test_lx; ly = g_test_ly; }
        extern uint64_t g_test_buttons, g_test_buttons_until;  // (LARECOMP_PLACE action script)
        if (frame < g_test_buttons_until) b |= g_test_buttons;
    }
    script_buttons = b;
    script_lx = lx;
    script_ly = ly;
    script_rx = rx;
    script_ry = ry;
}

extern bool menu_input_captured(void);  // runtime/ui/menu.mm: overlay menu or name entry up (it has the keys / pad)

// Buttons held right now (keyboard, gamepad, test script), independent of whether the game polls (movies).
uint64_t input_buttons_now(void) { return (menu_input_captured() ? 0 : key_buttons | pad_buttons | mouse_buttons) | script_buttons; }

static int32_t clamp_axis(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }

// NpadFullKeyState / NpadHandheldState / NpadJoy*State share this layout (0x28 bytes):
// {int64 sampling, u64 buttons, AnalogStick L {x,y}, AnalogStick R {x,y}, u32 attributes}
static uint32_t npad_id_used(uint32_t *style);  // (below: which controller the player has)

static void fill_state(Cpu *c) {
    uint8_t *s = (uint8_t *)(uintptr_t)c->x[0];
    uint32_t id = *(uint32_t *)(uintptr_t)c->x[1];
    memset(s, 0, 0x28);
    *(int64_t *)s = ++sampling;
    static int nlog;
    if (nlog < 6 && (sampling % 600) == 1) { nlog++; LOG("[input] GetNpadState id %#x lr %llx\n", id, c->x[30]); }
    if (id != npad_id_used(NULL)) return;  // only the one controller (npad_id_used) is connected
    extern void warp_game_tick(Cpu *c);
    warp_game_tick(c);  // game main thread, once per frame: apply a queued warp
    { extern void give_item_tick(Cpu *c, uint64_t frame); extern uint64_t renderer_present_count(void);
      give_item_tick(c, renderer_present_count());
      extern void chamber_game_tick(Cpu *c, uint64_t frame);
      chamber_game_tick(c, renderer_present_count());
      extern void digs_game_tick(Cpu *c, uint64_t frame);
      digs_game_tick(c, renderer_present_count());
      extern void qol_tick(void);
      qol_tick();
      extern void pets_game_tick(Cpu *c);
      pets_game_tick(c); }
    extern uint64_t g_watch_lo, g_watch_hi;
    if (!g_pad_obj) g_pad_obj = c->x[1] - 8;
    if (!g_watch_lo && getenv("LARECOMP_WATCH_PAD")) {
        unsigned lo = 0x98 + 0x10, hi = 0x98 + 0x48;  // offsets from the controller manager
        sscanf(getenv("LARECOMP_WATCH_PAD"), "%x,%x", &lo, &hi);
        g_watch_lo = c->x[1] - 8 - 0x98 + lo;
        g_watch_hi = c->x[1] - 8 - 0x98 + hi;
        LOG("[input] watching controller object %llx\n", c->x[1] - 8);
    }
    bool menu = menu_input_captured();  // the overlay menu / name entry has the controls (typing there isn't for the game)
    uint64_t b = menu ? script_buttons : key_buttons | pad_buttons | mouse_buttons | script_buttons;
    {   // injected Plus: held for ~10 presented frames (the game polls several times per frame)
        extern uint64_t renderer_present_count(void);
        static uint64_t until;
        if (g_inject_plus > 0) { until = renderer_present_count() + 10; g_inject_plus = 0; }
        if (renderer_present_count() < until) b |= 1ull << BTN_PLUS;
    }
    int32_t lx = clamp_axis((menu ? 0 : key_lx + pad_lx) + script_lx), ly = clamp_axis((menu ? 0 : key_ly + pad_ly) + script_ly);
    int32_t rx = clamp_axis((menu ? 0 : key_rx + pad_rx) + script_rx), ry = clamp_axis((menu ? 0 : key_ry + pad_ry) + script_ry);
    // d-pad / arrow keys walk (setting) while the stick is at rest. "At rest" has a dead zone: a controller's stick
    // never reads exactly 0 (a DualSense rests at a few hundred), which used to turn the d-pad off altogether.
    if (!menu && g_settings.dpad_move && (int64_t)lx * lx + (int64_t)ly * ly < 8000ll * 8000) {
        uint64_t d = key_buttons | pad_buttons | script_buttons;
        int dx = (int)((d >> BTN_RIGHT) & 1) - (int)((d >> BTN_LEFT) & 1);
        int dy = (int)((d >> BTN_UP) & 1) - (int)((d >> BTN_DOWN) & 1);
        int32_t m = dx && dy ? 23170 : 32767;  // diagonals: same speed (32767 / sqrt 2)
        if (dx || dy) lx = dx * m, ly = dy * m;
    }
    {   // door jumps stop Link for a moment before moving him (runtime/hle/doorjump.c): stick + d-pad released
        extern int32_t g_stick_lx, g_stick_ly;  // (what the player holds, before that: doorjump.c reads it)
        g_stick_lx = lx, g_stick_ly = ly;
        extern uint64_t g_stick_freeze_until;
        extern uint64_t renderer_present_count(void);
        if (renderer_present_count() < g_stick_freeze_until) {
            lx = ly = 0;
            b &= ~(0xfull << 12);
        }
    }
    // stick directions also appear as buttons (bits 16-23)
    if (lx < -16000) b |= 1ull << 16;
    if (ly > 16000) b |= 1ull << 17;
    if (lx > 16000) b |= 1ull << 18;
    if (ly < -16000) b |= 1ull << 19;
    if (rx < -16000) b |= 1ull << 20;
    if (ry > 16000) b |= 1ull << 21;
    if (rx > 16000) b |= 1ull << 22;
    if (ry < -16000) b |= 1ull << 23;
    {   // the pause menu's companion slot (runtime/hle/pets.c) takes the d-pad / sticks / A while the cursor is on it
        extern uint64_t pets_menu_input(uint64_t b, int32_t st[4]);
        int32_t st[4] = {lx, ly, rx, ry};
        b = pets_menu_input(b, st);
        lx = st[0], ly = st[1], rx = st[2], ry = st[3];
    }
    *(uint64_t *)(s + 8) = b;
    static uint64_t last_b;
    if (b != last_b && getenv("LARECOMP_INPUT")) LOG("[input] id %#x buttons %#llx (sampling %lld, lr %llx)\n", id, (unsigned long long)b, (long long)sampling, c->x[30]);
    last_b = b;
    *(int32_t *)(s + 16) = lx;
    *(int32_t *)(s + 20) = ly;
    { extern void input_note_rstick(int32_t ry); input_note_rstick(ry); }
    *(int32_t *)(s + 24) = rx;
    *(int32_t *)(s + 28) = ry;
    *(uint32_t *)(s + 32) = 1;  // IsConnected
}

HLE_FUNC("_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj", hid_state_fullkey) { fill_state(c); }
HLE_FUNC("_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj", hid_state_dual) { fill_state(c); }
HLE_FUNC("_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyLeftStateERKj", hid_state_left) { fill_state(c); }
HLE_FUNC("_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj", hid_state_handheld) { fill_state(c); }
HLE_FUNC("_ZN2nn3hid12GetNpadStateEPNS0_17NpadJoyRightStateERKj", hid_state_right) { fill_state(c); }

// The one controller the PC gamepad / keyboard appears as. Let's Go accepts no Pro Controller (FullKey): only a
// single Joy-Con, the Poke Ball Plus and the console in handheld mode - handheld maps every button and both sticks.
// LARECOMP_NPAD=handheld (default) | fullkey | dual | left | right.
static uint32_t npad_id_used(uint32_t *style) {
    static uint32_t id = ~0u, st;
    if (id == ~0u) {
        const char *e = getenv("LARECOMP_NPAD");
        if (!e || !strcmp(e, "handheld")) id = 0x20, st = 1u << 1;
        else if (!strcmp(e, "fullkey")) id = 0, st = 1u << 0;
        else if (!strcmp(e, "dual")) id = 0, st = 1u << 2;
        else if (!strcmp(e, "left")) id = 0, st = 1u << 3;
        else if (!strcmp(e, "right")) id = 0, st = 1u << 4;
        else id = 0x20, st = 1u << 1;
        LOG("[input] controller: npad id %#x, style %#x\n", id, st);
    }
    if (style) *style = st;
    return id;
}

HLE_FUNC("_ZN2nn3hid15GetNpadStyleSetERKj", hid_style_set) {
    uint32_t id = *(uint32_t *)(uintptr_t)c->x[0], style;
    static int nlog;
    if (nlog++ < 4) LOG("[input] GetNpadStyleSet id %#x\n", id);
    c->x[0] = id == npad_id_used(&style) ? style : 0;
}

// BindNpadStyleSetUpdateEvent(id, SystemEventType*, clear mode): signaled once so the game sees a
// controller connected.
#include "../kernel.h"
HLE_FUNC("_ZN2nn3hid27BindNpadStyleSetUpdateEventERKjPNS_2os15SystemEventTypeENS3_14EventClearModeE", hid_bind_style_event) {
    uint32_t w, r;
    KEvent *ev = kernel_create_event_pair(&w, &r);
    pthread_mutex_lock(&g_klock);
    kernel_signal(ev);
    pthread_mutex_unlock(&g_klock);
    uint64_t clear_mode = c->x[2];
    c->x[0] = c->x[1];
    c->x[1] = r;
    c->x[2] = 0;
    c->x[3] = clear_mode;
    guest_call(c, guest_sym("_ZN2nn2os33AttachReadableHandleToSystemEventEPNS0_15SystemEventTypeEjbNS0_14EventClearModeE"));
    LOG("[input] style set update event bound\n");
}

// Touch screen: TouchScreenState<16> {int64 sampling; int32 count; pad; TouchState[16]}, TouchState (0x28) {int64
// delta time; u32 attributes (bit 0 Start, bit 1 End); u32 finger id; u32 x, y; u32 diameter x, y; u32 rotation;
// pad}. One finger: the free mouse (touch_at), or LARECOMP_TOUCH="<from>-<to>[@x,y]" (frames: a back-and-forth
// petting stroke around x,y - default the middle of the screen - for tests).
static bool touch_now(int32_t *x, int32_t *y) {
    static int from = -1, to, cx = 640, cy = 330;
    if (from < 0) {
        from = 0;
        const char *e = getenv("LARECOMP_TOUCH");
        if (e) sscanf(e, "%d-%d@%d,%d", &from, &to, &cx, &cy);
    }
    extern uint64_t renderer_present_count(void);
    uint64_t f = renderer_present_count();
    if (from > 0 && f >= (uint64_t)from && f <= (uint64_t)to) {
        double t = (double)(f - from);
        *x = (int32_t)(cx + 120 * sin(t * 0.25));
        *y = (int32_t)(cy + 20 * sin(t * 0.5));
        return true;
    }
    *x = touch_x, *y = touch_y;
    return touch_down && !menu_input_captured();
}
HLE_FUNC("_ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi", hid_touch_states) {
    uint8_t *s = (uint8_t *)(uintptr_t)c->x[0];
    int count = (int)c->x[1];
    static int64_t sampling;
    static bool was_down;
    static int nlog;
    if (count <= 0) { c->x[0] = 0; return; }
    int32_t x, y;
    bool down = touch_now(&x, &y);
    memset(s, 0, 0x290);
    *(int64_t *)s = ++sampling;
    if (down || was_down) {  // (the release is one more sample, flagged End)
        uint8_t *t = s + 0x10;
        *(int32_t *)(s + 8) = 1;
        *(int64_t *)t = 16666667;
        *(uint32_t *)(t + 8) = down && !was_down ? 1 : !down ? 2 : 0;
        *(uint32_t *)(t + 0x10) = (uint32_t)x;
        *(uint32_t *)(t + 0x14) = (uint32_t)y;
        *(uint32_t *)(t + 0x18) = *(uint32_t *)(t + 0x1c) = 15;
        if (nlog < 12 && down != was_down) {
            nlog++;
            LOG("[touch] %s at %d,%d (lr %llx)\n", down ? "down" : "up", x, y, (unsigned long long)c->x[30]);
        }
    }
    was_down = down;
    c->x[0] = 1;
}
// Gestures: none yet (logged: does the game use them?).
HLE_FUNC("_ZN2nn3hid16GetGestureStatesEPNS0_12GestureStateEi", hid_gesture_states) {
    static int nlog;
    if (nlog++ < 3) LOG("[touch] GetGestureStates (lr %llx)\n", (unsigned long long)c->x[30]);
    c->x[0] = 0;
}

// Vibration devices: accepted but inert (the SDK's vibration node graph stays recompiled).
HLE_FUNC("_ZN2nn3hid25InitializeVibrationDeviceERKNS0_21VibrationDeviceHandleE", hid_init_vibration) { (void)c; }
// Six-axis (motion) sensor. Let's Go aims Poke Ball throws with the gyro in handheld mode; with no sensor behind it
// the SDK read garbage and the aim went into the sky. The rest pose is a controller lying level (identity orientation,
// gravity -z), which keeps the aim centred. The mouse turns this virtual controller (yaw / pitch, input_handle_event
// accumulates the motion), with every field consistent: orientation, angle, angular velocity and gravity.
// SixAxisSensorState (0x60): +0 delta time ns, +8 sampling number, +0x10 acceleration (G), +0x1c angular velocity
// (turns / s), +0x28 angle (turns), +0x34 direction (3x3: x, y, z axes), +0x58 attributes (bit 0 IsConnected).
// LARECOMP_GYRO_TEST=x|y|z sweeps the yaw axis ±15 degrees (2 s period) to see which way the aim follows.
static double gyro_yaw, gyro_pitch;               // radians
static _Atomic uint64_t gyro_start_ns;            // when the game last started the sensor (aiming begins)
static int gyro_yaw_axis = 2, gyro_pitch_axis = 0;
static void mat_rot(double m[3][3], int axis, double a) {
    int i = (axis + 1) % 3, j = (axis + 2) % 3;
    memset(m, 0, 9 * sizeof(double));
    m[axis][axis] = 1;
    m[i][i] = cos(a); m[i][j] = -sin(a);
    m[j][i] = sin(a); m[j][j] = cos(a);
}
static void mat_mul(double o[3][3], double a[3][3], double b[3][3]) {
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) o[r][k] = a[r][0] * b[0][k] + a[r][1] * b[1][k] + a[r][2] * b[2][k];
}
static void six_axis_fill(uint8_t *s, int64_t sampling) {
    static int test = -1, test_fixed;
    if (test < 0) {
        const char *e = getenv("LARECOMP_GYRO_TEST");
        test = e && *e >= 'x' && *e <= 'z' ? 1 + (*e - 'x') : 0;
        if (test && (e[1] == '+' || e[1] == '-')) test_fixed = e[1] == '+' ? 1 : -1;
        const char *ax = getenv("LARECOMP_GYRO_AXES");  // "<yaw><pitch>", e.g. "zx" (the default)
        if (ax && ax[0] >= 'x' && ax[0] <= 'z' && ax[1] >= 'x' && ax[1] <= 'z') gyro_yaw_axis = ax[0] - 'x', gyro_pitch_axis = ax[1] - 'x';
    }
    static uint64_t last_ns;
    uint64_t now = SDL_GetTicksNS();
    double dt = last_ns ? (now - last_ns) / 1e9 : 1 / 60.0;
    if (dt < 1e-4) dt = 1e-4;
    if (dt > 0.1) dt = 0.1;
    last_ns = now;
    double py = gyro_yaw, pp = gyro_pitch;
    int yaw_axis = gyro_yaw_axis;
    if (test) {
        yaw_axis = test - 1;
        // x+ / z-...: 1 s into the aim, a turn of 10 degrees at 20 degrees/s (a sudden jump is ignored as noise)
        double ts = gyro_start_ns ? (now - gyro_start_ns) / 1e9 - 1 : 0, ramp = ts < 0 ? 0 : ts > 0.5 ? 0.5 : ts;
        gyro_yaw = test_fixed ? test_fixed * 20 * ramp * M_PI / 180 : 15 * M_PI / 180 * sin(now / 1e9 * M_PI);
        gyro_pitch = 0;
    } else {
        double k = g_settings.mouse_sens * M_PI / 180 / 1000;  // degrees per pixel
        gyro_yaw -= atomic_exchange(&mouse_acc_x, 0) * k;
        gyro_pitch -= atomic_exchange(&mouse_acc_y, 0) * k;
        double lim = 80 * M_PI / 180;
        if (gyro_pitch > lim) gyro_pitch = lim;
        if (gyro_pitch < -lim) gyro_pitch = -lim;
    }
    double ry[3][3], rp[3][3], r[3][3];
    mat_rot(ry, yaw_axis, gyro_yaw);
    mat_rot(rp, gyro_pitch_axis, gyro_pitch);
    mat_mul(r, ry, rp);
    memset(s, 0, 0x60);
    *(int64_t *)(s + 0x00) = (int64_t)(dt * 1e9);
    *(int64_t *)(s + 0x08) = sampling;
    float *acc = (float *)(s + 0x10), *gyr = (float *)(s + 0x1c), *ang = (float *)(s + 0x28), *dir = (float *)(s + 0x34);
    for (int i = 0; i < 3; i++) acc[i] = (float)-r[2][i];  // gravity (world -z) in the controller's frame: R^T (0,0,-1)
    gyr[yaw_axis] += (float)((gyro_yaw - py) / dt / (2 * M_PI));
    gyr[gyro_pitch_axis] += (float)((gyro_pitch - pp) / dt / (2 * M_PI));
    ang[yaw_axis] += (float)(gyro_yaw / (2 * M_PI));
    ang[gyro_pitch_axis] += (float)(gyro_pitch / (2 * M_PI));
    for (int a = 0; a < 3; a++)  // direction: the controller's x, y, z axes in world coordinates (columns of R)
        for (int i = 0; i < 3; i++) dir[a * 3 + i] = (float)r[i][a];
    *(uint32_t *)(s + 0x58) = 1;  // IsConnected
}
static int64_t six_axis_sampling;
HLE_FUNC("_ZN2nn3hid23GetSixAxisSensorHandlesEPNS0_19SixAxisSensorHandleEiRKjNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", hid_six_axis_handles) {
    uint32_t *out = (uint32_t *)(uintptr_t)c->x[0];
    int n = (int)c->x[1] < 2 ? (int)c->x[1] : 2;
    uint32_t id = *(uint32_t *)(uintptr_t)c->x[2];
    for (int i = 0; i < n; i++) out[i] = (id & 0xFF) << 8 | (uint32_t)i | 0x04000000u;  // (opaque to the game)
    c->x[0] = (uint64_t)n;
}
HLE_FUNC("_ZN2nn3hid18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE", hid_six_axis_start) {
    // aiming begins (the game measures the turn from here): the virtual controller starts level again, and the
    // mouse becomes the gyro at its next move (input_handle_event)
    gyro_aiming = 1;
    gyro_start_ns = SDL_GetTicksNS();
    gyro_yaw = gyro_pitch = 0;
    atomic_store(&mouse_acc_x, 0);
    atomic_store(&mouse_acc_y, 0);
    extern uint64_t renderer_present_count(void);
    LOG("[input] six-axis sensor started (frame %llu)\n", (unsigned long long)renderer_present_count());
}
HLE_FUNC("_ZN2nn3hid17StopSixAxisSensorERKNS0_19SixAxisSensorHandleE", hid_six_axis_stop) {
    gyro_aiming = 0;
    extern uint64_t renderer_present_count(void);
    if (getenv("LARECOMP_GYRO_TEST")) LOG("[input] six-axis sensor stopped (frame %llu)\n", (unsigned long long)renderer_present_count());
}
static void six_axis_log(Cpu *c) {  // (LARECOMP_GYRO_TEST: when the game reads it, and from where)
    static int on = -1;
    if (on < 0) on = getenv("LARECOMP_GYRO_TEST") != NULL;
    extern uint64_t renderer_present_count(void);
    static uint64_t last, n;
    uint64_t f = renderer_present_count();
    n++;
    if (on && (n <= 8 || f / 30 != last / 30))
        LOG("[gyro] read #%llu at frame %llu (lr %llx)\n", (unsigned long long)n, (unsigned long long)f, (unsigned long long)c->x[30]);
    last = f;
}
HLE_FUNC("_ZN2nn3hid21GetSixAxisSensorStateEPNS0_18SixAxisSensorStateERKNS0_19SixAxisSensorHandleE", hid_six_axis_state) {
    six_axis_log(c);
    six_axis_fill((uint8_t *)(uintptr_t)c->x[0], ++six_axis_sampling);
}
HLE_FUNC("_ZN2nn3hid22GetSixAxisSensorStatesEPNS0_18SixAxisSensorStateEiRKNS0_19SixAxisSensorHandleE", hid_six_axis_states) {
    six_axis_log(c);
    int n = (int)c->x[1];
    if (n > 0) six_axis_fill((uint8_t *)(uintptr_t)c->x[0], ++six_axis_sampling);  // the latest sample only
    c->x[0] = n > 0 ? 1 : 0;
}
HLE_FUNC("_ZN2nn3hid21IsSixAxisSensorAtRestERKNS0_19SixAxisSensorHandleE", hid_six_axis_at_rest) {
    static int n;
    if (getenv("LARECOMP_GYRO_TEST") && n++ < 8) LOG("[gyro] IsSixAxisSensorAtRest (lr %llx)\n", (unsigned long long)c->x[30]);
    c->x[0] = 0;  // never at rest: at rest, the game stopped reading the sensor (a hand-held console never is)
}
HLE_FUNC("_ZN2nn3hid25EnableSixAxisSensorFusionERKNS0_19SixAxisSensorHandleEb", hid_six_axis_fusion) { (void)c; }
HLE_FUNC("_ZN2nn3hid28IsSixAxisSensorFusionEnabledERKNS0_19SixAxisSensorHandleE", hid_six_axis_fusion_on) { c->x[0] = 1; }
HLE_FUNC("_ZN2nn3hid32SetSixAxisSensorFusionParametersERKNS0_19SixAxisSensorHandleEff", hid_six_axis_set_params) { (void)c; }
HLE_FUNC("_ZN2nn3hid32GetSixAxisSensorFusionParametersEPfS1_RKNS0_19SixAxisSensorHandleE", hid_six_axis_get_params) {
    if (c->x[0]) *(float *)(uintptr_t)c->x[0] = 0.03f;
    if (c->x[1]) *(float *)(uintptr_t)c->x[1] = 0.4f;
}
HLE_FUNC("_ZN2nn3hid25SetGyroscopeZeroDriftModeERKNS0_19SixAxisSensorHandleERKNS0_22GyroscopeZeroDriftModeE", hid_gyro_drift_set) { (void)c; }
HLE_FUNC("_ZN2nn3hid25GetGyroscopeZeroDriftModeERKNS0_19SixAxisSensorHandleE", hid_gyro_drift_get) { c->x[0] = 1; }

HLE_FUNC("_ZN2nn3hid25GetVibrationDeviceHandlesEPNS0_21VibrationDeviceHandleEiRKjNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", hid_vibration_handles) {
    uint32_t *out = (uint32_t *)(uintptr_t)c->x[0];
    int n = (int)c->x[1] < 2 ? (int)c->x[1] : 2;
    uint32_t id = *(uint32_t *)(uintptr_t)c->x[2];
    for (int i = 0; i < n; i++) out[i] = (id & 0xFF) << 8 | (uint32_t)i | 0x03000000u;  // type, player, position
    c->x[0] = (uint64_t)n;
}
HLE_FUNC("_ZN2nn3hid22GetVibrationDeviceInfoEPNS0_19VibrationDeviceInfoERKNS0_21VibrationDeviceHandleE", hid_vibration_info) {
    uint32_t *info = (uint32_t *)(uintptr_t)c->x[0];
    uint32_t h = *(uint32_t *)(uintptr_t)c->x[1];
    info[0] = 1;        // VibrationDeviceType_LinearResonantActuator
    info[1] = h & 1;    // position: left / right
}
