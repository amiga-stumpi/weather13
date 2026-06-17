#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <string.h>
#include "ui.h"
#include "i18n.h"
#include "weather_net.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static int open_libraries(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase)
        return 0;
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!GfxBase)
        return 0;
    return 1;
}

static void close_libraries(void)
{
    if (GfxBase) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = 0;
    }
    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = 0;
    }
}

static void copy_text(char *dst, int dst_size, const char *src)
{
    int i = 0;
    if (dst_size <= 0)
        return;
    while (src && src[i] && i < dst_size - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static int is_quit_key(UWORD code)
{
    code &= 0x7f;
    return code == 0x10; /* Q on Amiga raw keyboard */
}

static int is_update_key(UWORD code)
{
    code &= 0x7f;
    return code == 0x16; /* U on Amiga raw keyboard */
}


static ULONG update_interval_ticks(const W13App *app)
{
    WORD minutes = 30;
    if (app)
        minutes = app->config.update_interval;
    if (minutes < 5)
        minutes = 5;
    else if (minutes > 120)
        minutes = 120;
    return (ULONG)minutes * 60UL * 10UL;
}

static void refresh_weather(W13App *app, int startup_cache_loaded)
{
    if (!app)
        return;
    W13_SetStatus(app, "Updating...");
    if (!W13_FetchWeatherForLocation(app->config.location[0] ? app->config.location : app->data.location, &app->data, app->status, sizeof(app->status))) {
        if (startup_cache_loaded < 0)
            W13_SetStatus(app, app->status[0] ? app->status : "Update failed");
        else if (startup_cache_loaded)
            W13_SetStatus(app, "Offline; using cache");
        else
            W13_SetStatus(app, "Offline; demo data");
    } else {
        W13_SetStatus(app, app->status);
    }
    app->update_ticks = 0;
    W13_DrawAll(app);
}

static void update_wind_jitter(W13App *app)
{
    static const WORD offsets[][2] = {
        { 0, 0 }, { 2, 1 }, { 4, 2 }, { 6, 3 },
        { 4, 1 }, { 2, 0 }, { 0, 0 }, { -2, -1 },
        { -4, -2 }, { -6, -3 }, { -4, -1 }, { -2, 0 }
    };
    UWORD count = (UWORD)(sizeof(offsets) / sizeof(offsets[0]));

    if (!app)
        return;
    app->anim_phase = (UWORD)((app->anim_phase + 1) % count);
    app->wind_jitter_x = offsets[app->anim_phase][0];
    app->wind_jitter_y = offsets[app->anim_phase][1];
    app->anim_ticks = 0;
    app->anim_next = (UWORD)(20 + ((app->anim_phase * 7) % 31));
}

int main(void)
{
    W13App app;
    ULONG sigmask;
    int done = 0;
    int cache_loaded = 0;

    memset(&app, 0, sizeof(app));
    if (!open_libraries()) {
        close_libraries();
        return 20;
    }
    W13_InitLanguage();
    W13_LoadConfig(&app.config);
    W13_FillDummyWeather(&app.data);
    copy_text(app.status, sizeof(app.status), "Demo data");
    if (W13_LoadWeatherCache(&app.data)) {
        copy_text(app.status, sizeof(app.status), "Cached data");
        cache_loaded = 1;
    }
    if (app.config.location[0])
        copy_text(app.data.location, sizeof(app.data.location), app.config.location);
    if (!W13_Open(&app)) {
        close_libraries();
        return 20;
    }
    app.anim_next = 30;
    W13_DrawAll(&app);
    refresh_weather(&app, cache_loaded);
    sigmask = 1UL << app.win->UserPort->mp_SigBit;

    while (!done) {
        struct IntuiMessage *msg;
        Wait(sigmask);
        while ((msg = (struct IntuiMessage *)GetMsg(app.win->UserPort)) != 0) {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (cls == IDCMP_NEWSIZE) {
                app.config.left = app.win->LeftEdge;
                app.config.top = app.win->TopEdge;
                app.config.width = app.win->Width;
                app.config.height = app.win->Height;
                W13_DrawAll(&app);
            } else if (cls == IDCMP_MENUPICK) {
                if (W13_HandleMenu(&app, code))
                    done = 1;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(app.win);
                W13_DrawAll(&app);
                EndRefresh(app.win, TRUE);
            } else if (cls == IDCMP_INTUITICKS) {
                ++app.anim_ticks;
                ++app.update_ticks;
                if (app.anim_ticks >= app.anim_next) {
                    update_wind_jitter(&app);
                    W13_DrawWindRoseOnly(&app);
                }
                if (app.update_ticks >= update_interval_ticks(&app))
                    refresh_weather(&app, -1);
            } else if (cls == IDCMP_RAWKEY) {
                if (is_quit_key(code)) {
                    done = 1;
                } else if (is_update_key(code)) {
                    refresh_weather(&app, -1);
                }
            }
        }
    }

    W13_SaveWeatherCache(&app.data);
    W13_Close(&app);
    W13_SaveConfig(&app.config);
    close_libraries();
    return 0;
}
