/**
 * Launcher integration for retro-go-firmware
 *
 * Provides retro_loop() — the ROM browser UI loop.
 * Simplified for our retro-go version (some newer launcher APIs not available).
 */
#include "rg_system.h"
#include "shared.h"

#include "applications.h"
#include "bookmarks.h"
#include "browser.h"
#include "gui.h"

/* ── app pointer (defined in main.c) ── */
extern rg_app_t *app;

/* ── Static state ── */
static int64_t next_repeat = 0;
static int repeats = 0;
static int prev_joystick = 0;
static int change_tab = 0;
static int browse_last = -1;
static bool redraw_pending = true;

/* ── Simple callbacks ── */

static rg_gui_event_t show_preview_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) {
        gui.show_preview = !gui.show_preview;
        rg_settings_set_number(NS_GLOBAL, "ShowPreview", gui.show_preview);
        return RG_DIALOG_REDRAW;
    }
    strcpy(option->value, gui.show_preview ? _("Show") : _("Hide"));
    return RG_DIALOG_VOID;
}

static rg_gui_event_t scroll_mode_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    const char *modes[SCROLL_MODE_COUNT] = {_("Center"), _("Paging")};
    const int max = SCROLL_MODE_COUNT - 1;
    if (event == RG_DIALOG_PREV && --gui.scroll_mode < 0) gui.scroll_mode = max;
    if (event == RG_DIALOG_NEXT && ++gui.scroll_mode > max) gui.scroll_mode = 0;
    gui.scroll_mode %= SCROLL_MODE_COUNT;
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) return RG_DIALOG_REDRAW;
    strcpy(option->value, modes[gui.scroll_mode]);
    return RG_DIALOG_VOID;
}

static rg_gui_event_t start_screen_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    const char *modes[START_SCREEN_COUNT] = {_("Auto"), _("Carousel"), _("Browser")};
    const int max = START_SCREEN_COUNT - 1;
    if (event == RG_DIALOG_PREV && --gui.start_screen < 0) gui.start_screen = max;
    if (event == RG_DIALOG_NEXT && ++gui.start_screen > max) gui.start_screen = 0;
    gui.start_screen %= START_SCREEN_COUNT;
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) return RG_DIALOG_REDRAW;
    strcpy(option->value, modes[gui.start_screen]);
    return RG_DIALOG_VOID;
}

static rg_gui_event_t toggle_tab_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    tab_t *tab = gui.tabs[option->arg];
    if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT || event == RG_DIALOG_ENTER)
        tab->enabled = !tab->enabled;
    strcpy(option->value, tab->enabled ? _("Show") : _("Hide"));
    return RG_DIALOG_VOID;
}

static rg_gui_event_t toggle_tabs_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_ENTER) {
        rg_gui_option_t options[32];
        rg_gui_option_t *opt = options;
        for (size_t i = 0; i < gui.tabs_count && i < 16; ++i)
            *opt++ = (rg_gui_option_t){ (int)i, gui.tabs[i]->name, "...", 1, &toggle_tab_cb };
        *opt++ = (rg_gui_option_t)RG_DIALOG_END;
        rg_gui_dialog(option->label, options, 0);
    }
    return RG_DIALOG_VOID;
}

/* ── Options / About menus ── */

void options_handler(rg_gui_option_t *dest)
{
    const rg_gui_option_t options[] = {
        {0, _("Preview"),      "-", RG_DIALOG_FLAG_NORMAL, &show_preview_cb},
        {0, _("Scroll mode"),  "-", RG_DIALOG_FLAG_NORMAL, &scroll_mode_cb},
        {0, _("Start screen"), "-", RG_DIALOG_FLAG_NORMAL, &start_screen_cb},
        {0, _("Hide tabs"),    "-", RG_DIALOG_FLAG_NORMAL, &toggle_tabs_cb},
        RG_DIALOG_END,
    };
    memcpy(dest, options, sizeof(options));
}

void about_handler(rg_gui_option_t *dest)
{
    *dest++ = (rg_gui_option_t){0, _("Build CRC cache"), NULL, RG_DIALOG_FLAG_NORMAL, NULL};
    *dest++ = (rg_gui_option_t)RG_DIALOG_END;
}

/* ── Event handler ── */

void event_handler(int event, void *arg)
{
    if (event == RG_EVENT_REDRAW)
        gui_redraw();
}

/* ── Main launcher loop ── */

void retro_loop(void)
{
    tab_t *tab = NULL;
    int64_t next_idle_event = 0;
    int joystick;

    gui_init(app->isColdBoot);
    applications_init();
    bookmarks_init();

    if (!gui_get_current_tab())
        gui.selected_tab = 0;
    tab = gui_set_current_tab(gui.selected_tab);

    while (true) {
        if (gui.http_lock) {
            rg_gui_draw_message(_("HTTP Server Busy..."));
            redraw_pending = true;
            rg_task_delay(100);
            continue;
        }

        prev_joystick = gui.joystick;
        joystick = 0;

        if ((gui.joystick = rg_input_read_gamepad())) {
            if (prev_joystick != gui.joystick) {
                joystick = gui.joystick;
                repeats = 0;
                next_repeat = rg_system_timer() + 400000;
            } else if ((rg_system_timer() - next_repeat) >= 0) {
                joystick = gui.joystick;
                repeats++;
                next_repeat = rg_system_timer() + 400000 / (repeats + 1);
            }
        }

        if (joystick & (RG_KEY_MENU|RG_KEY_OPTION)) {
            if (joystick == RG_KEY_MENU)
                rg_gui_about_menu();
            else
                rg_gui_options_menu();

            gui_update_theme();
            gui_save_config();
            rg_settings_commit();
            redraw_pending = true;
        }

        int64_t start_time = rg_system_timer();

        if (!tab->enabled && !change_tab)
            change_tab = 1;

        if (change_tab || gui.browse != browse_last) {
            if (change_tab) {
                gui_event(TAB_LEAVE, tab);
                tab = gui_set_current_tab(gui.selected_tab + change_tab);
                for (int tabs = gui.tabs_count; !tab->enabled && --tabs > 0;)
                    tab = gui_set_current_tab(gui.selected_tab + change_tab);
                change_tab = 0;
            }

            if (gui.browse) {
                if (!tab->initialized) {
                    gui_redraw();
                    gui_init_tab(tab);
                }
                gui_event(TAB_ENTER, tab);
            }

            browse_last = gui.browse;
            redraw_pending = true;
        }

        if (gui.browse) {
            if (joystick == RG_KEY_SELECT)      change_tab = -1;
            else if (joystick == RG_KEY_START)   change_tab = 1;
            else if (joystick == RG_KEY_UP)      { gui_scroll_list(tab, SCROLL_LINE, -1); redraw_pending = true; }
            else if (joystick == RG_KEY_DOWN)    { gui_scroll_list(tab, SCROLL_LINE, 1);  redraw_pending = true; }
            else if (joystick == RG_KEY_LEFT)    { gui_scroll_list(tab, SCROLL_PAGE, -1); redraw_pending = true; }
            else if (joystick == RG_KEY_RIGHT)   { gui_scroll_list(tab, SCROLL_PAGE, 1);  redraw_pending = true; }
            else if (joystick == RG_KEY_A)       { gui_event(TAB_ACTION, tab); redraw_pending = true; }
            else if (joystick == RG_KEY_B) {
                if (tab->navpath) gui_event(TAB_BACK, tab);
                else gui.browse = false;
                redraw_pending = true;
            }
        } else {
            if (joystick & (RG_KEY_UP|RG_KEY_LEFT|RG_KEY_SELECT))  change_tab = -1;
            else if (joystick & (RG_KEY_DOWN|RG_KEY_RIGHT|RG_KEY_START))  change_tab = 1;
            else if (joystick == RG_KEY_A)  gui.browse = true;
        }

        if (redraw_pending) {
            redraw_pending = false;
            gui_redraw();
        }

        rg_system_tick(rg_system_timer() - start_time);

        if ((gui.joystick|joystick) & RG_KEY_ANY) {
            gui.idle_counter = 0;
            next_idle_event = rg_system_timer() + 100000;
        } else if (rg_system_timer() >= next_idle_event) {
            gui.idle_counter++;
            gui.joystick = 0;
            prev_joystick = 0;
            gui_event(TAB_IDLE, tab);
            next_idle_event = rg_system_timer() + 100000;
            if (gui.idle_counter % 10 == 1)
                redraw_pending = true;
        } else if (gui.idle_counter) {
            rg_task_delay(10);
        }
    }
}
