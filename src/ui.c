#include "ui.h"
#include "windrose.h"
#include "i18n.h"
#include "weather_net.h"
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#define W13_VERSION "v0.1"

#define W13_MIN_W 320
#define W13_MIN_H 178
#define W13_DEFAULT_W 600
#define W13_DEFAULT_H 190
#define W13_CONFIG_FILE "weather13.conf"
#define W13_CONFIG_BUF_SIZE 160

static int text_len(const char *s)
{
    int n = 0;
    while (s && s[n])
        ++n;
    return n;
}

static int value_in_range(long value, long min_value, long max_value)
{
    return value >= min_value && value <= max_value;
}

static int key_equals(const char *line, const char *key)
{
    while (*key) {
        if (*line++ != *key++)
            return 0;
    }
    return *line == '=';
}

static long parse_long_value(const char *s)
{
    long value = 0;
    int neg = 0;

    if (!s)
        return 0;
    if (*s == '-') {
        neg = 1;
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        ++s;
    }
    return neg ? -value : value;
}

static void config_defaults(W13WindowConfig *cfg)
{
    cfg->left = 20;
    cfg->top = 20;
    cfg->width = W13_DEFAULT_W;
    cfg->height = W13_DEFAULT_H;
    cfg->location[0] = 'M';
    cfg->location[1] = 'u';
    cfg->location[2] = 'e';
    cfg->location[3] = 'n';
    cfg->location[4] = 's';
    cfg->location[5] = 't';
    cfg->location[6] = 'e';
    cfg->location[7] = 'r';
    cfg->location[8] = 0;
}


static void copy_config_text(char *dst, int dst_size, const char *src)
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

static void config_set_text(W13WindowConfig *cfg, const char *line)
{
    const char *value = line;

    while (*value && *value != '=')
        ++value;
    if (*value == '=')
        ++value;
    if (key_equals(line, "LOCATION"))
        copy_config_text(cfg->location, sizeof(cfg->location), value);
}

static void config_set_value(W13WindowConfig *cfg, const char *line)
{
    const char *value = line;
    long n;

    while (*value && *value != '=')
        ++value;
    if (*value == '=')
        ++value;
    n = parse_long_value(value);

    if (key_equals(line, "LEFT") && value_in_range(n, 0, 639))
        cfg->left = (WORD)n;
    else if (key_equals(line, "TOP") && value_in_range(n, 0, 255))
        cfg->top = (WORD)n;
    else if (key_equals(line, "WIDTH") && value_in_range(n, W13_MIN_W, 640))
        cfg->width = (WORD)n;
    else if (key_equals(line, "HEIGHT") && value_in_range(n, W13_MIN_H, 256))
        cfg->height = (WORD)n;
}

void W13_LoadConfig(W13WindowConfig *cfg)
{
    BPTR fh;
    char buf[W13_CONFIG_BUF_SIZE];
    char line[48];
    LONG len;
    LONG i;
    int pos = 0;

    config_defaults(cfg);
    fh = Open((STRPTR)W13_CONFIG_FILE, MODE_OLDFILE);
    if (!fh)
        return;
    len = Read(fh, buf, sizeof(buf));
    Close(fh);
    if (len <= 0)
        return;

    for (i = 0; i < len; ++i) {
        char c = buf[i];
        if (c == '\r')
            continue;
        if (c == '\n') {
            line[pos] = 0;
            if (pos > 0 && line[0] != '#') {
                config_set_value(cfg, line);
                config_set_text(cfg, line);
            }
            pos = 0;
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }
    if (pos > 0) {
        line[pos] = 0;
        if (line[0] != '#') {
            config_set_value(cfg, line);
            config_set_text(cfg, line);
        }
    }

    if (cfg->width < W13_MIN_W || cfg->width > 640)
        cfg->width = W13_DEFAULT_W;
    if (cfg->height < W13_MIN_H || cfg->height > 256)
        cfg->height = W13_DEFAULT_H;
    if (cfg->left < 0 || cfg->left > 639 || cfg->left + W13_MIN_W > 640)
        cfg->left = 0;
    if (cfg->top < 0 || cfg->top > 255 || cfg->top + W13_MIN_H > 256)
        cfg->top = 0;
}

static void config_write_num(BPTR fh, const char *key, long value)
{
    char line[48];
    char rev[12];
    int p = 0;
    int r = 0;
    int neg = 0;
    unsigned long v;

    while (*key && p < (int)sizeof(line) - 1)
        line[p++] = *key++;
    if (p < (int)sizeof(line) - 1)
        line[p++] = '=';
    if (value < 0) {
        neg = 1;
        v = (unsigned long)(-value);
    } else {
        v = (unsigned long)value;
    }
    if (neg && p < (int)sizeof(line) - 1)
        line[p++] = '-';
    if (v == 0)
        rev[r++] = '0';
    while (v && r < (int)sizeof(rev)) {
        rev[r++] = (char)('0' + (v % 10UL));
        v /= 10UL;
    }
    while (r > 0 && p < (int)sizeof(line) - 2)
        line[p++] = rev[--r];
    line[p++] = '\n';
    Write(fh, line, p);
}


static void config_write_text(BPTR fh, const char *key, const char *value)
{
    char line[80];
    int p = 0;
    int i = 0;

    while (*key && p < (int)sizeof(line) - 1)
        line[p++] = *key++;
    if (p < (int)sizeof(line) - 1)
        line[p++] = '=';
    while (value && value[i] && p < (int)sizeof(line) - 2)
        line[p++] = value[i++];
    line[p++] = '\n';
    Write(fh, line, p);
}

void W13_SaveConfig(const W13WindowConfig *cfg)
{
    BPTR fh;

    if (!cfg)
        return;
    fh = Open((STRPTR)W13_CONFIG_FILE, MODE_NEWFILE);
    if (!fh)
        return;
    Write(fh, "# Weather13 window configuration\n", 33);
    config_write_num(fh, "LEFT", cfg->left);
    config_write_num(fh, "TOP", cfg->top);
    config_write_num(fh, "WIDTH", cfg->width);
    config_write_num(fh, "HEIGHT", cfg->height);
    config_write_text(fh, "LOCATION", cfg->location);
    Close(fh);
}

static void draw_text(struct RastPort *rp, WORD x, WORD y, const char *s)
{
    Move(rp, x, y);
    Text(rp, (STRPTR)s, text_len(s));
}

static void cat_num(char *dst, int dst_size, int value)
{
    char rev[12];
    int r = 0;
    int p = text_len(dst);
    int neg = 0;
    unsigned int v;

    if (value < 0) {
        neg = 1;
        v = (unsigned int)(-value);
    } else {
        v = (unsigned int)value;
    }
    if (neg && p < dst_size - 1)
        dst[p++] = '-';
    if (v == 0)
        rev[r++] = '0';
    while (v && r < (int)sizeof(rev)) {
        rev[r++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (r > 0 && p < dst_size - 1)
        dst[p++] = rev[--r];
    dst[p] = 0;
}

static void cat_text(char *dst, int dst_size, const char *src)
{
    int p = text_len(dst);
    int i = 0;
    while (src && src[i] && p < dst_size - 1)
        dst[p++] = src[i++];
    dst[p] = 0;
}

static void temp_text(char *dst, int dst_size, int tenths)
{
    int whole;
    int frac;
    dst[0] = 0;
    if (tenths < 0) {
        cat_text(dst, dst_size, "-");
        tenths = -tenths;
    }
    whole = tenths / 10;
    frac = tenths % 10;
    cat_num(dst, dst_size, whole);
    cat_text(dst, dst_size, ".");
    cat_num(dst, dst_size, frac);
    cat_text(dst, dst_size, " C");
}

static UBYTE icon_pen(const Weather13Icon *icon, int x, int y)
{
    ULONG plane_size;
    ULONG row_index;
    UWORD bit;
    UBYTE pen = 0;
    UWORD word;
    UWORD p;

    if (!icon || x < 0 || y < 0 || x >= icon->width || y >= icon->height)
        return 0;
    row_index = (ULONG)y * icon->row_words + (ULONG)(x >> 4);
    bit = (UWORD)(0x8000U >> (x & 15));
    if (!(icon->mask[row_index] & bit))
        return 0;
    plane_size = (ULONG)icon->height * icon->row_words;
    for (p = 0; p < icon->depth; ++p) {
        word = icon->planes[(ULONG)p * plane_size + row_index];
        if (word & bit)
            pen |= (UBYTE)(1U << p);
    }
    return pen;
}

static void draw_icon(struct RastPort *rp, const Weather13Icon *icon, WORD x, WORD y, UBYTE max_pen)
{
    int ix;
    int iy;
    UBYTE pen;

    if (!icon)
        return;
    for (iy = 0; iy < icon->height; ++iy) {
        for (ix = 0; ix < icon->width; ++ix) {
            pen = icon_pen(icon, ix, iy);
            if (pen) {
                if (pen > max_pen)
                    pen = max_pen;
                SetAPen(rp, pen);
                WritePixel(rp, x + ix, y + iy);
            }
        }
    }
}

static void draw_box(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2, UBYTE pen)
{
    SetAPen(rp, pen);
    Move(rp, x1, y1);
    Draw(rp, x2, y1);
    Draw(rp, x2, y2);
    Draw(rp, x1, y2);
    Draw(rp, x1, y1);
}

static void draw_forecast(W13App *app, WORD x, WORD y, WORD w)
{
    struct RastPort *rp = app->win->RPort;
    WORD colw = w / 3;
    WORD text_x;
    int i;
    char line[40];
    const Weather13Icon *icon;
    UBYTE max_pen = (UBYTE)((1U << app->asset_depth) - 1U);
    UBYTE text_pen;

    for (i = 0; i < 3; ++i) {
        WORD cx = (WORD)(x + i * colw);
        if (i == 0)
            text_pen = 1;
        else if (i == 1)
            text_pen = 3;
        else
            text_pen = 2;
        SetAPen(rp, text_pen);
        if (i == 0)
            draw_text(rp, cx + 4, y + 12, W13_Text(W13_TX_TODAY));
        else if (i == 1)
            draw_text(rp, cx + 4, y + 12, W13_Text(W13_TX_TOMORROW));
        else
            draw_text(rp, cx + 4, y + 12, W13_Text(W13_TX_DAY_AFTER));
        icon = Weather13_GetIcon((UWORD)app->data.forecast[i].weather_code, 24, app->screen_depth);
        draw_icon(rp, icon, cx + 6, y + 18, max_pen);
        text_x = (WORD)(cx + 36);
        line[0] = 0;
        cat_text(line, sizeof(line), W13_Text(W13_TX_DAY));
        cat_text(line, sizeof(line), " ");
        cat_num(line, sizeof(line), app->data.forecast[i].temp_day);
        cat_text(line, sizeof(line), " C");
        SetAPen(rp, text_pen);
        draw_text(rp, text_x, y + 24, line);
        line[0] = 0;
        cat_text(line, sizeof(line), W13_Text(W13_TX_NIGHT));
        cat_text(line, sizeof(line), " ");
        cat_num(line, sizeof(line), app->data.forecast[i].temp_night);
        cat_text(line, sizeof(line), " C");
        SetAPen(rp, text_pen);
        draw_text(rp, text_x, y + 36, line);
        line[0] = 0;
        cat_text(line, sizeof(line), W13_Text(W13_TX_RAIN));
        cat_text(line, sizeof(line), " ");
        cat_num(line, sizeof(line), app->data.forecast[i].precip_probability);
        cat_text(line, sizeof(line), "%");
        SetAPen(rp, text_pen);
        draw_text(rp, text_x, y + 48, line);
        line[0] = 0;
        cat_text(line, sizeof(line), W13_Text(W13_TX_WIND));
        cat_text(line, sizeof(line), " ");
        cat_num(line, sizeof(line), app->data.forecast[i].wind_speed_max);
        cat_text(line, sizeof(line), " ");
        cat_text(line, sizeof(line), W13_WindDirName(app->data.forecast[i].wind_dir_deg));
        SetAPen(rp, text_pen);
        draw_text(rp, text_x, y + 60, line);
    }
}

static void draw_temp_large(struct RastPort *rp, WORD x, WORD y, const char *s)
{
    SetDrMd(rp, JAM1);
    SetAPen(rp, 1);
    draw_text(rp, x, y, s);
    draw_text(rp, x + 1, y, s);
}


static struct IntuiText w13_info_text = { 0, 1, JAM2, 2, 1, 0, (UBYTE *)"Info", 0 };
static struct IntuiText w13_quit_text = { 0, 1, JAM2, 2, 1, 0, (UBYTE *)"Quit", 0 };
static struct IntuiText w13_location_text = { 0, 1, JAM2, 2, 1, 0, (UBYTE *)"Location...", 0 };

static struct MenuItem w13_project_items[2];
static struct MenuItem w13_settings_items[1];
static struct Menu w13_menus[2];

static void init_menu_item(struct MenuItem *item, struct MenuItem *next, WORD top, WORD width, struct IntuiText *text)
{
    memset(item, 0, sizeof(*item));
    item->NextItem = next;
    item->LeftEdge = 0;
    item->TopEdge = top;
    item->Width = width;
    item->Height = 11;
    item->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
    item->ItemFill = (APTR)text;
}

static void setup_menus(W13App *app)
{
    if (!app || !app->win)
        return;
    init_menu_item(&w13_project_items[0], &w13_project_items[1], 0, 64, &w13_info_text);
    init_menu_item(&w13_project_items[1], 0, 11, 64, &w13_quit_text);
    init_menu_item(&w13_settings_items[0], 0, 0, 92, &w13_location_text);

    memset(w13_menus, 0, sizeof(w13_menus));
    w13_menus[0].NextMenu = &w13_menus[1];
    w13_menus[0].LeftEdge = 0;
    w13_menus[0].TopEdge = 0;
    w13_menus[0].Width = 72;
    w13_menus[0].Height = 10;
    w13_menus[0].Flags = MENUENABLED;
    w13_menus[0].MenuName = (UBYTE *)"Project";
    w13_menus[0].FirstItem = &w13_project_items[0];

    w13_menus[1].LeftEdge = 72;
    w13_menus[1].TopEdge = 0;
    w13_menus[1].Width = 80;
    w13_menus[1].Height = 10;
    w13_menus[1].Flags = MENUENABLED;
    w13_menus[1].MenuName = (UBYTE *)"Settings";
    w13_menus[1].FirstItem = &w13_settings_items[0];

    SetMenuStrip(app->win, &w13_menus[0]);
}

int W13_Open(W13App *app)
{
    struct NewWindow nw;
    WORD width = app->config.width;
    WORD height = app->config.height;

    memset(&nw, 0, sizeof(nw));

    if (width < W13_MIN_W || width > 640)
        width = W13_DEFAULT_W;
    if (height < W13_MIN_H || height > 256)
        height = W13_DEFAULT_H;
    if (app->config.left < 0 || app->config.left + W13_MIN_W > 640)
        app->config.left = 0;
    if (app->config.top < 0 || app->config.top + W13_MIN_H > 256)
        app->config.top = 0;

    nw.LeftEdge = app->config.left;
    nw.TopEdge = app->config.top;
    nw.Width = width;
    nw.Height = height;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY | IDCMP_NEWSIZE | IDCMP_MENUPICK | IDCMP_INTUITICKS;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
               WFLG_SIZEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.FirstGadget = 0;
    nw.CheckMark = 0;
    nw.Title = (UBYTE *)"Weather13";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = W13_MIN_W;
    nw.MinHeight = W13_MIN_H;
    nw.MaxWidth = 640;
    nw.MaxHeight = 256;
    nw.Type = WBENCHSCREEN;

    app->win = OpenWindow(&nw);
    if (!app->win) {
        nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY | IDCMP_MENUPICK | IDCMP_INTUITICKS;
        nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                   WFLG_SMART_REFRESH | WFLG_ACTIVATE;
        nw.LeftEdge = 0;
        nw.TopEdge = 0;
        nw.Width = width;
        nw.Height = height;
        app->win = OpenWindow(&nw);
    }
    if (!app->win) {
        nw.LeftEdge = 0;
        nw.TopEdge = 0;
        nw.Width = W13_MIN_W;
        nw.Height = W13_MIN_H;
        app->win = OpenWindow(&nw);
    }
    if (!app->win)
        return 0;
    app->config.left = app->win->LeftEdge;
    app->config.top = app->win->TopEdge;
    app->config.width = app->win->Width;
    app->config.height = app->win->Height;

    app->screen_depth = app->win->WScreen->BitMap.Depth;
    if (app->screen_depth >= 4)
        app->asset_depth = 4;
    else if (app->screen_depth == 3)
        app->asset_depth = 3;
    else
        app->asset_depth = 2;
    setup_menus(app);
    return 1;
}

void W13_Close(W13App *app)
{
    if (app && app->win) {
        app->config.left = app->win->LeftEdge;
        app->config.top = app->win->TopEdge;
        app->config.width = app->win->Width;
        app->config.height = app->win->Height;
        ClearMenuStrip(app->win);
        CloseWindow(app->win);
        app->win = 0;
    }
}

static int inside(WORD x, WORD y, WORD l, WORD t, WORD r, WORD b)
{
    return x >= l && x <= r && y >= t && y <= b;
}

static void draw_button(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2, const char *label)
{
    SetAPen(rp, 1);
    Move(rp, x1, y1);
    Draw(rp, x2, y1);
    Draw(rp, x2, y2);
    Draw(rp, x1, y2);
    Draw(rp, x1, y1);
    draw_text(rp, (WORD)(x1 + 8), (WORD)(y1 + 12), label);
}

void W13_ShowInfo(W13App *app)
{
    struct NewWindow nw;
    struct Window *win;
    ULONG mask;
    int done = 0;
    WORD left = 20;
    WORD top = 18;

    if (app && app->win) {
        left = app->win->LeftEdge + 18;
        top = app->win->TopEdge + 16;
    }
    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = left;
    nw.TopEdge = top;
    nw.Width = 320;
    nw.Height = 112;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.Title = (UBYTE *)"Info";
    nw.Type = WBENCHSCREEN;
    win = OpenWindow(&nw);
    if (!win)
        return;
    SetDrMd(win->RPort, JAM1);
    SetBPen(win->RPort, 0);
    SetAPen(win->RPort, 0);
    RectFill(win->RPort, 2, 10, win->Width - 3, win->Height - 3);
    SetAPen(win->RPort, 1);
    draw_text(win->RPort, 12, 24, "Weather13 for Kick1.3");
    draw_text(win->RPort, 12, 36, "Version: " W13_VERSION);
    draw_text(win->RPort, 12, 48, "by Marcel Jaehne");
    draw_text(win->RPort, 12, 60, "(c) 2026");
    draw_text(win->RPort, 12, 72, "If you want to buy me a coffe,");
    draw_text(win->RPort, 12, 84, "send me a buck: paypal.me/mytubefree");
    draw_button(win->RPort, 128, 92, 190, 108, "OK");
    mask = 1UL << win->UserPort->mp_SigBit;
    while (!done) {
        struct IntuiMessage *msg;
        Wait(mask);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != 0) {
            ULONG cls = msg->Class;
            WORD mx = msg->MouseX;
            WORD my = msg->MouseY;
            UWORD code = msg->Code;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW)
                done = 1;
            else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN && inside(mx, my, 128, 92, 190, 108))
                done = 1;
            else if (cls == IDCMP_RAWKEY && ((code & 0x7f) == 0x44 || (code & 0x7f) == 0x45))
                done = 1;
        }
    }
    CloseWindow(win);
}

void W13_ShowLocation(W13App *app)
{
    struct NewWindow nw;
    struct Window *win;
    struct Gadget loc_gadget;
    struct StringInfo loc_info;
    static char loc_buf[32];
    static char loc_undo[32];
    ULONG mask;
    int done = 0;
    WORD left = 24;
    WORD top = 24;
    if (!app)
        return;
    copy_config_text(loc_buf, sizeof(loc_buf), app->config.location[0] ? app->config.location : app->data.location);
    loc_undo[0] = 0;
    memset(&loc_info, 0, sizeof(loc_info));
    loc_info.Buffer = (UBYTE *)loc_buf;
    loc_info.UndoBuffer = (UBYTE *)loc_undo;
    loc_info.MaxChars = sizeof(loc_buf);
    memset(&loc_gadget, 0, sizeof(loc_gadget));
    loc_gadget.LeftEdge = 68;
    loc_gadget.TopEdge = 28;
    loc_gadget.Width = 180;
    loc_gadget.Height = 12;
    loc_gadget.Flags = GADGHCOMP;
    loc_gadget.Activation = RELVERIFY;
    loc_gadget.GadgetType = STRGADGET;
    loc_gadget.SpecialInfo = (APTR)&loc_info;
    loc_gadget.GadgetID = 1;

    if (app->win) {
        left = app->win->LeftEdge + 24;
        top = app->win->TopEdge + 24;
    }
    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = left;
    nw.TopEdge = top;
    nw.Width = 300;
    nw.Height = 92;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_GADGETUP | IDCMP_RAWKEY;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.FirstGadget = &loc_gadget;
    nw.Title = (UBYTE *)"Location";
    nw.Type = WBENCHSCREEN;
    win = OpenWindow(&nw);
    if (!win)
        return;
    SetDrMd(win->RPort, JAM1);
    SetBPen(win->RPort, 0);
    SetAPen(win->RPort, 0);
    RectFill(win->RPort, 2, 10, win->Width - 3, win->Height - 3);
    SetAPen(win->RPort, 1);
    draw_text(win->RPort, 12, 38, "Ort:");
    draw_box(win->RPort, 66, 26, 250, 42, 1);
    draw_text(win->RPort, 12, 56, "Open-Meteo search: /v1/search?name=");
    draw_button(win->RPort, 32, 66, 96, 82, "Search");
    draw_button(win->RPort, 116, 66, 170, 82, "Set");
    draw_button(win->RPort, 190, 66, 256, 82, "Cancel");
    ActivateGadget(&loc_gadget, win, 0);
    mask = 1UL << win->UserPort->mp_SigBit;
    while (!done) {
        struct IntuiMessage *msg;
        Wait(mask);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != 0) {
            ULONG cls = msg->Class;
            WORD mx = msg->MouseX;
            WORD my = msg->MouseY;
            UWORD code = msg->Code;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                if (inside(mx, my, 32, 66, 96, 82)) {
                    if (loc_buf[0]) {
                        copy_config_text(app->config.location, sizeof(app->config.location), loc_buf);
                        W13_SetStatus(app, "Searching...");
                        W13_FetchWeatherForLocation(loc_buf, &app->data, app->status, sizeof(app->status));
                        W13_SetStatus(app, app->status[0] ? app->status : "Search failed");
                    }
                } else if (inside(mx, my, 116, 66, 170, 82)) {
                    if (loc_buf[0]) {
                        copy_config_text(app->config.location, sizeof(app->config.location), loc_buf);
                        copy_config_text(app->data.location, sizeof(app->data.location), loc_buf);
                        if (app->win)
                            W13_DrawAll(app);
                    }
                    done = 1;
                } else if (inside(mx, my, 190, 66, 256, 82)) {
                    done = 1;
                }
            } else if (cls == IDCMP_RAWKEY) {
                UWORD raw = code & 0x7f;
                if (raw == 0x44) {
                    if (loc_buf[0]) {
                        copy_config_text(app->config.location, sizeof(app->config.location), loc_buf);
                        copy_config_text(app->data.location, sizeof(app->data.location), loc_buf);
                        if (app->win)
                            W13_DrawAll(app);
                    }
                    done = 1;
                } else if (raw == 0x45) {
                    done = 1;
                }
            }
        }
    }
    CloseWindow(win);
}

int W13_HandleMenu(W13App *app, UWORD code)
{
    UWORD menu = MENUNUM(code);
    UWORD item = ITEMNUM(code);

    if (code == MENUNULL)
        return 0;
    if (menu == 0 && item == 0) {
        W13_ShowInfo(app);
    } else if (menu == 0 && item == 1) {
        return 1;
    } else if (menu == 1 && item == 0) {
        W13_ShowLocation(app);
    }
    return 0;
}


void W13_SetStatus(W13App *app, const char *status)
{
    int i = 0;
    if (!app)
        return;
    while (status && status[i] && i < (int)sizeof(app->status) - 1) {
        app->status[i] = status[i];
        ++i;
    }
    app->status[i] = 0;
    if (app->win)
        W13_DrawAll(app);
}


static WORD wind_area_x(W13App *app)
{
    WORD w;
    WORD wind_x;
    if (!app || !app->win)
        return 300;
    w = app->win->Width;
    if (w < W13_MIN_W)
        w = W13_MIN_W;
    wind_x = (WORD)(w - 174);
    if (wind_x < 300)
        wind_x = 300;
    return wind_x;
}

void W13_DrawWindRoseOnly(W13App *app)
{
    struct RastPort *rp;
    WORD wind_x;
    WORD cx;
    WORD cy = 65;
    WORD radius = 26;

    if (!app || !app->win)
        return;
    rp = app->win->RPort;
    wind_x = wind_area_x(app);
    cx = (WORD)(wind_x + 111);
    SetAPen(rp, 0);
    RectFill(rp, cx - radius - 14, cy - radius - 8, cx + radius + 16, cy + radius + 14);
    SetBPen(rp, 0);
    SetDrMd(rp, JAM1);
    W13_DrawWindRose(rp, cx, cy, radius, app->data.wind_dir_deg, app->data.wind_speed,
        app->wind_jitter_x, app->wind_jitter_y, 1);
    {
        char line[32];
        line[0] = 0;
        cat_num(line, sizeof(line), app->data.wind_speed);
        cat_text(line, sizeof(line), " km/h ");
        cat_text(line, sizeof(line), W13_WindDirName(app->data.wind_dir_deg));
        SetAPen(rp, 1);
        draw_text(rp, wind_x - 5, 53, line);
    }
}

void W13_DrawAll(W13App *app)
{
    struct RastPort *rp;
    WORD w;
    WORD h;
    WORD forecast_y;
    WORD wind_x;
    char line[64];
    const Weather13Icon *icon;
    UBYTE max_pen;

    if (!app || !app->win)
        return;
    rp = app->win->RPort;
    w = app->win->Width;
    h = app->win->Height;
    if (w < W13_MIN_W)
        w = W13_MIN_W;
    if (h < W13_MIN_H)
        h = W13_MIN_H;
    forecast_y = (WORD)(h - 82);
    if (forecast_y < 106)
        forecast_y = 106;
    wind_x = (WORD)(w - 174);
    if (wind_x < 300)
        wind_x = 300;
    max_pen = (UBYTE)((1U << app->asset_depth) - 1U);

    SetAPen(rp, 0);
    RectFill(rp, 2, 10, w - 3, h - 3);
    SetBPen(rp, 0);
    SetDrMd(rp, JAM1);
    SetAPen(rp, 1);

    draw_text(rp, 8, 22, "Weather13");
    line[0] = 0;
    cat_text(line, sizeof(line), W13_Text(W13_TX_UPDATE));
    cat_text(line, sizeof(line), " ");
    cat_text(line, sizeof(line), app->data.updated);
    draw_text(rp, w - 142, 22, line);

    draw_box(rp, 6, 26, w - 8, forecast_y - 6, 1);
    draw_box(rp, 6, forecast_y, w - 8, h - 22, 1);

    icon = Weather13_GetIcon((UWORD)app->data.weather_code, 48, app->screen_depth);
    draw_icon(rp, icon, 20, 42, max_pen);
    draw_text(rp, 82, 58, W13_ConditionText(app->data.weather_code));
    line[0] = 0;
    cat_text(line, sizeof(line), W13_Text(W13_TX_HUMIDITY));
    cat_text(line, sizeof(line), " ");
    cat_num(line, sizeof(line), app->data.humidity);
    cat_text(line, sizeof(line), "%");
    draw_text(rp, 82, 74, line);
    line[0] = 0;
    cat_text(line, sizeof(line), W13_Text(W13_TX_PRESSURE));
    cat_text(line, sizeof(line), " ");
    cat_num(line, sizeof(line), app->data.pressure);
    cat_text(line, sizeof(line), " hPa");
    draw_text(rp, 82, 90, line);

    temp_text(line, sizeof(line), app->data.temp_tenths);
    cat_text(line, sizeof(line), " (");
    {
        char feels[20];
        temp_text(feels, sizeof(feels), app->data.apparent_temp_tenths);
        cat_text(line, sizeof(line), feels);
    }
    cat_text(line, sizeof(line), ")");
    draw_temp_large(rp, (WORD)(w / 2 - 58), 58, line);
    draw_text(rp, (WORD)(w / 2 - 34), 78, app->data.location);

    draw_text(rp, wind_x + 20, 41, W13_Text(W13_TX_WIND));
    line[0] = 0;
    cat_num(line, sizeof(line), app->data.wind_speed);
    cat_text(line, sizeof(line), " km/h ");
    cat_text(line, sizeof(line), W13_WindDirName(app->data.wind_dir_deg));
    draw_text(rp, wind_x - 5, 53, line);
    W13_DrawWindRoseOnly(app);

    draw_forecast(app, 8, forecast_y - 2, (WORD)(w - 18));

    line[0] = 0;
    cat_text(line, sizeof(line), "U ");
    cat_text(line, sizeof(line), W13_Text(W13_TX_UPDATE));
    cat_text(line, sizeof(line), "    Q ");
    cat_text(line, sizeof(line), W13_Text(W13_TX_END));
    if (app->status[0]) {
        cat_text(line, sizeof(line), "    ");
        cat_text(line, sizeof(line), app->status);
    }
    draw_text(rp, 8, h - 8, line);
}
