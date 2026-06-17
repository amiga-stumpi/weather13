#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <string.h>
#include "ui.h"
#include "i18n.h"

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

int main(void)
{
    W13App app;
    ULONG sigmask;
    int done = 0;

    memset(&app, 0, sizeof(app));
    if (!open_libraries()) {
        close_libraries();
        return 20;
    }
    W13_InitLanguage();
    W13_LoadConfig(&app.config);
    W13_FillDummyWeather(&app.data);
    if (app.config.location[0])
        copy_text(app.data.location, sizeof(app.data.location), app.config.location);
    if (!W13_Open(&app)) {
        close_libraries();
        return 20;
    }
    W13_DrawAll(&app);
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
            } else if (cls == IDCMP_RAWKEY) {
                if (is_quit_key(code)) {
                    done = 1;
                } else if (is_update_key(code)) {
                    W13_FillDummyWeather(&app.data);
                    W13_DrawAll(&app);
                }
            }
        }
    }

    W13_Close(&app);
    W13_SaveConfig(&app.config);
    close_libraries();
    return 0;
}
