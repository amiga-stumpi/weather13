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
#define W13_CONFIG_BUF_SIZE 192
#define W13_DEFAULT_UPDATE_MINUTES 30
#define W13_MIN_UPDATE_MINUTES 5
#define W13_MAX_UPDATE_MINUTES 120
#define W13_DEFAULT_LANGUAGE W13_LANG_ENGLISH

static int text_len(const char *s)
{
    int n = 0;
    while (s && s[n])
        ++n;
    return n;
}

static int lower_char(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static int text_equals_ci(const char *a, const char *b)
{
    int i = 0;
    if (!a || !b)
        return 0;
    while (a[i] && b[i]) {
        if (lower_char((UBYTE)a[i]) != lower_char((UBYTE)b[i]))
            return 0;
        ++i;
    }
    return a[i] == 0 && b[i] == 0;
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
    cfg->update_interval = W13_DEFAULT_UPDATE_MINUTES;
    cfg->language = W13_DEFAULT_LANGUAGE;
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

static void config_set_language(W13WindowConfig *cfg, const char *line)
{
    const char *value = line;

    while (*value && *value != '=')
        ++value;
    if (*value == '=')
        ++value;
    if (!key_equals(line, "LANGUAGE"))
        return;
    if (text_equals_ci(value, "GERMAN") || text_equals_ci(value, "DEUTSCH"))
        cfg->language = W13_LANG_GERMAN;
    else if (text_equals_ci(value, "POLISH") || text_equals_ci(value, "POLSKI"))
        cfg->language = W13_LANG_POLISH;
    else
        cfg->language = W13_LANG_ENGLISH;
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
    else if (key_equals(line, "UPDATE_INTERVAL") && value_in_range(n, W13_MIN_UPDATE_MINUTES, W13_MAX_UPDATE_MINUTES))
        cfg->update_interval = (WORD)n;
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
                config_set_language(cfg, line);
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
            config_set_language(cfg, line);
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
    if (cfg->update_interval < W13_MIN_UPDATE_MINUTES || cfg->update_interval > W13_MAX_UPDATE_MINUTES)
        cfg->update_interval = W13_DEFAULT_UPDATE_MINUTES;
    if (cfg->language < W13_LANG_ENGLISH || cfg->language > W13_LANG_POLISH)
        cfg->language = W13_DEFAULT_LANGUAGE;
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



static const char *config_language_name(WORD language)
{
    if (language == W13_LANG_GERMAN)
        return "GERMAN";
    if (language == W13_LANG_POLISH)
        return "POLISH";
    return "ENGLISH";
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
    config_write_num(fh, "UPDATE_INTERVAL", cfg->update_interval);
    config_write_text(fh, "LANGUAGE", config_language_name(cfg->language));
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


static struct IntuiText w13_info_text = { 0, 1, JAM2, 2, 1, 0, 0, 0 };
static struct IntuiText w13_quit_text = { 0, 1, JAM2, 2, 1, 0, 0, 0 };
static struct IntuiText w13_location_text = { 0, 1, JAM2, 2, 1, 0, 0, 0 };
static struct IntuiText w13_update_interval_text = { 0, 1, JAM2, 2, 1, 0, 0, 0 };
static struct IntuiText w13_language_text = { 0, 1, JAM2, 2, 1, 0, 0, 0 };

static struct MenuItem w13_project_items[2];
static struct MenuItem w13_settings_items[3];
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
    w13_info_text.IText = (UBYTE *)W13_Text(W13_TX_MENU_INFO);
    w13_quit_text.IText = (UBYTE *)W13_Text(W13_TX_MENU_QUIT);
    w13_location_text.IText = (UBYTE *)W13_Text(W13_TX_MENU_LOCATION);
    w13_update_interval_text.IText = (UBYTE *)W13_Text(W13_TX_MENU_UPDATE_INTERVAL);
    w13_language_text.IText = (UBYTE *)W13_Text(W13_TX_MENU_LANGUAGE);
    init_menu_item(&w13_project_items[0], &w13_project_items[1], 0, 72, &w13_info_text);
    init_menu_item(&w13_project_items[1], 0, 11, 72, &w13_quit_text);
    init_menu_item(&w13_settings_items[0], &w13_settings_items[1], 0, 152, &w13_location_text);
    init_menu_item(&w13_settings_items[1], &w13_settings_items[2], 11, 152, &w13_update_interval_text);
    init_menu_item(&w13_settings_items[2], 0, 22, 152, &w13_language_text);

    memset(w13_menus, 0, sizeof(w13_menus));
    w13_menus[0].NextMenu = &w13_menus[1];
    w13_menus[0].LeftEdge = 0;
    w13_menus[0].TopEdge = 0;
    w13_menus[0].Width = 72;
    w13_menus[0].Height = 10;
    w13_menus[0].Flags = MENUENABLED;
    w13_menus[0].MenuName = (UBYTE *)W13_Text(W13_TX_MENU_PROJECT);
    w13_menus[0].FirstItem = &w13_project_items[0];

    w13_menus[1].LeftEdge = 72;
    w13_menus[1].TopEdge = 0;
    w13_menus[1].Width = 96;
    w13_menus[1].Height = 10;
    w13_menus[1].Flags = MENUENABLED;
    w13_menus[1].MenuName = (UBYTE *)W13_Text(W13_TX_MENU_SETTINGS);
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

static void draw_location_result(struct Window *win, const char *line1, const char *line2)
{
    if (!win)
        return;
    SetAPen(win->RPort, 0);
    RectFill(win->RPort, 10, 46, win->Width - 12, 72);
    SetAPen(win->RPort, 1);
    if (line1 && line1[0])
        draw_text(win->RPort, 12, 56, line1);
    if (line2 && line2[0])
        draw_text(win->RPort, 12, 68, line2);
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
    nw.Title = (UBYTE *)W13_Text(W13_TX_TITLE_INFO);
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
    draw_button(win->RPort, 128, 92, 190, 108, W13_Text(W13_TX_BTN_OK));
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


static void location_run_search(W13App *app, struct Window *win, const char *loc_buf,
    char found_names[][32], char *result1, int result1_size, char *result2, int result2_size)
{
    int count;

    if (!app || !win || !loc_buf || !loc_buf[0])
        return;
    W13_SetStatus(app, W13_Text(W13_TX_MSG_SEARCHING));
    count = W13_SearchLocations(loc_buf, found_names, 3, app->status, sizeof(app->status));
    if (count <= 0) {
        draw_location_result(win, W13_Text(W13_TX_MSG_LOCATION_NOT_FOUND), 0);
        W13_SetStatus(app, app->status[0] ? app->status : W13_Text(W13_TX_MSG_SEARCH_FAILED));
    } else {
        int match = -1;
        int i;
        for (i = 0; i < count; ++i) {
            if (text_equals_ci(loc_buf, found_names[i])) {
                match = i;
                break;
            }
        }
        if (match >= 0) {
            result1[0] = 0;
            cat_text(result1, result1_size, W13_Text(W13_TX_MSG_FOUND_PREFIX));
            cat_text(result1, result1_size, found_names[match]);
            draw_location_result(win, result1, 0);
            copy_config_text(app->config.location, sizeof(app->config.location), found_names[match]);
            W13_FetchWeatherForLocation(found_names[match], &app->data, app->status, sizeof(app->status));
            W13_SetStatus(app, app->status[0] ? app->status : W13_Text(W13_TX_MSG_SEARCH_DONE));
        } else {
            int shown = 0;
            result2[0] = 0;
            for (i = 0; i < count && shown < 2; ++i) {
                if (text_equals_ci(loc_buf, found_names[i]))
                    continue;
                if (shown > 0)
                    cat_text(result2, result2_size, ", ");
                cat_text(result2, result2_size, found_names[i]);
                ++shown;
            }
            draw_location_result(win, W13_Text(W13_TX_MSG_LOCATION_TRY), result2);
            W13_SetStatus(app, W13_Text(W13_TX_MSG_SIMILAR_LOCATIONS));
        }
    }
}

void W13_ShowLocation(W13App *app)
{
    struct NewWindow nw;
    struct Window *win;
    struct Gadget loc_gadget;
    struct StringInfo loc_info;
    static char loc_buf[32];
    static char loc_undo[32];
    char found_names[3][32];
    char result1[48];
    char result2[80];
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
    loc_gadget.LeftEdge = 94;
    loc_gadget.TopEdge = 28;
    loc_gadget.Width = 200;
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
    nw.Width = 340;
    nw.Height = 110;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_GADGETUP | IDCMP_RAWKEY;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.FirstGadget = &loc_gadget;
    nw.Title = (UBYTE *)W13_Text(W13_TX_TITLE_LOCATION);
    nw.Type = WBENCHSCREEN;
    win = OpenWindow(&nw);
    if (!win)
        return;
    SetDrMd(win->RPort, JAM1);
    SetBPen(win->RPort, 0);
    SetAPen(win->RPort, 0);
    RectFill(win->RPort, 2, 10, win->Width - 3, win->Height - 3);
    SetAPen(win->RPort, 1);
    draw_text(win->RPort, 12, 38, W13_Text(W13_TX_LABEL_LOCATION));
    draw_box(win->RPort, 92, 26, 298, 42, 1);
    draw_location_result(win, W13_Text(W13_TX_MSG_ENTER_LOCATION), 0);
    draw_button(win->RPort, 32, 84, 96, 100, W13_Text(W13_TX_BTN_SEARCH));
    draw_button(win->RPort, 116, 84, 170, 100, W13_Text(W13_TX_BTN_SET));
    draw_button(win->RPort, 190, 84, 256, 100, W13_Text(W13_TX_BTN_CANCEL));
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
                if (inside(mx, my, 32, 84, 96, 100)) {
                    location_run_search(app, win, loc_buf, found_names, result1, sizeof(result1), result2, sizeof(result2));
                } else if (inside(mx, my, 116, 84, 170, 100)) {
                    if (loc_buf[0]) {
                        copy_config_text(app->config.location, sizeof(app->config.location), loc_buf);
                        copy_config_text(app->data.location, sizeof(app->data.location), loc_buf);
                        if (app->win)
                            W13_DrawAll(app);
                    }
                    done = 1;
                } else if (inside(mx, my, 190, 84, 256, 100)) {
                    done = 1;
                }
            } else if (cls == IDCMP_RAWKEY) {
                UWORD raw = code & 0x7f;
                if (raw == 0x44) {
                    location_run_search(app, win, loc_buf, found_names, result1, sizeof(result1), result2, sizeof(result2));
                } else if (raw == 0x45) {
                    done = 1;
                }
            }
        }
    }
    CloseWindow(win);
}


void W13_ShowUpdateInterval(W13App *app)
{
    struct NewWindow nw;
    struct Window *win;
    struct Gadget interval_gadget;
    struct StringInfo interval_info;
    static char interval_buf[4];
    static char interval_undo[4];
    ULONG mask;
    int done = 0;
    WORD left = 28;
    WORD top = 28;

    if (!app)
        return;
    interval_buf[0] = 0;
    cat_num(interval_buf, sizeof(interval_buf), app->config.update_interval);
    interval_undo[0] = 0;
    memset(&interval_info, 0, sizeof(interval_info));
    interval_info.Buffer = (UBYTE *)interval_buf;
    interval_info.UndoBuffer = (UBYTE *)interval_undo;
    interval_info.MaxChars = sizeof(interval_buf);
    memset(&interval_gadget, 0, sizeof(interval_gadget));
    interval_gadget.LeftEdge = 128;
    interval_gadget.TopEdge = 28;
    interval_gadget.Width = 48;
    interval_gadget.Height = 12;
    interval_gadget.Flags = GADGHCOMP;
    interval_gadget.Activation = RELVERIFY;
    interval_gadget.GadgetType = STRGADGET;
    interval_gadget.SpecialInfo = (APTR)&interval_info;
    interval_gadget.GadgetID = 1;

    if (app->win) {
        left = app->win->LeftEdge + 28;
        top = app->win->TopEdge + 28;
    }
    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = left;
    nw.TopEdge = top;
    nw.Width = 280;
    nw.Height = 94;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_GADGETUP | IDCMP_RAWKEY;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.FirstGadget = &interval_gadget;
    nw.Title = (UBYTE *)W13_Text(W13_TX_TITLE_UPDATE_INTERVAL);
    nw.Type = WBENCHSCREEN;
    win = OpenWindow(&nw);
    if (!win)
        return;
    SetDrMd(win->RPort, JAM1);
    SetBPen(win->RPort, 0);
    SetAPen(win->RPort, 0);
    RectFill(win->RPort, 2, 10, win->Width - 3, win->Height - 3);
    SetAPen(win->RPort, 1);
    draw_text(win->RPort, 12, 38, W13_Text(W13_TX_LABEL_MINUTES));
    draw_box(win->RPort, 126, 26, 178, 42, 1);
    draw_text(win->RPort, 12, 56, W13_Text(W13_TX_MSG_VALID_INTERVAL));
    draw_button(win->RPort, 82, 68, 134, 84, W13_Text(W13_TX_BTN_SET));
    draw_button(win->RPort, 154, 68, 222, 84, W13_Text(W13_TX_BTN_CANCEL));
    ActivateGadget(&interval_gadget, win, 0);
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
                if (inside(mx, my, 82, 68, 134, 84)) {
                    long value = parse_long_value(interval_buf);
                    if (value_in_range(value, W13_MIN_UPDATE_MINUTES, W13_MAX_UPDATE_MINUTES)) {
                        app->config.update_interval = (WORD)value;
                        app->update_ticks = 0;
                        W13_SetStatus(app, W13_Text(W13_TX_MSG_INTERVAL_CHANGED));
                        done = 1;
                    } else {
                        SetAPen(win->RPort, 0);
                        RectFill(win->RPort, 10, 46, 268, 60);
                        SetAPen(win->RPort, 1);
                        draw_text(win->RPort, 12, 56, W13_Text(W13_TX_MSG_VALID_INTERVAL));
                    }
                } else if (inside(mx, my, 154, 68, 222, 84)) {
                    done = 1;
                }
            } else if (cls == IDCMP_RAWKEY) {
                UWORD raw = code & 0x7f;
                if (raw == 0x44) {
                    long value = parse_long_value(interval_buf);
                    if (value_in_range(value, W13_MIN_UPDATE_MINUTES, W13_MAX_UPDATE_MINUTES)) {
                        app->config.update_interval = (WORD)value;
                        app->update_ticks = 0;
                        W13_SetStatus(app, W13_Text(W13_TX_MSG_INTERVAL_CHANGED));
                        done = 1;
                    } else {
                        SetAPen(win->RPort, 0);
                        RectFill(win->RPort, 10, 46, 268, 60);
                        SetAPen(win->RPort, 1);
                        draw_text(win->RPort, 12, 56, W13_Text(W13_TX_MSG_VALID_INTERVAL));
                    }
                } else if (raw == 0x45) {
                    done = 1;
                }
            }
        }
    }
    CloseWindow(win);
}

void W13_ShowLanguage(W13App *app)
{
    struct NewWindow nw;
    struct Window *win;
    ULONG mask;
    int done = 0;
    WORD left = 30;
    WORD top = 30;

    if (!app)
        return;
    if (app->win) {
        left = app->win->LeftEdge + 30;
        top = app->win->TopEdge + 30;
    }
    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = left;
    nw.TopEdge = top;
    nw.Width = 230;
    nw.Height = 112;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.Title = (UBYTE *)W13_Text(W13_TX_TITLE_LANGUAGE);
    nw.Type = WBENCHSCREEN;
    win = OpenWindow(&nw);
    if (!win)
        return;
    SetDrMd(win->RPort, JAM1);
    SetBPen(win->RPort, 0);
    SetAPen(win->RPort, 0);
    RectFill(win->RPort, 2, 10, win->Width - 3, win->Height - 3);
    SetAPen(win->RPort, 1);
    draw_text(win->RPort, 12, 28, W13_Text(W13_TX_MSG_SELECT_LANGUAGE));
    draw_button(win->RPort, 34, 38, 126, 54, "English");
    draw_button(win->RPort, 34, 60, 126, 76, "Deutsch");
    draw_button(win->RPort, 34, 82, 126, 98, "Polski");
    if (app->config.language == W13_LANG_ENGLISH)
        draw_text(win->RPort, 144, 50, W13_Text(W13_TX_MSG_CURRENT));
    else if (app->config.language == W13_LANG_GERMAN)
        draw_text(win->RPort, 144, 72, W13_Text(W13_TX_MSG_CURRENT));
    else if (app->config.language == W13_LANG_POLISH)
        draw_text(win->RPort, 144, 94, W13_Text(W13_TX_MSG_CURRENT));
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
                if (inside(mx, my, 34, 38, 126, 54)) {
                    app->config.language = W13_LANG_ENGLISH;
                    W13_SetLanguage(app->config.language);
                    if (app->win) {
                        ClearMenuStrip(app->win);
                        setup_menus(app);
                    }
                    W13_SetStatus(app, W13_Text(W13_TX_MSG_LANGUAGE_CHANGED));
                    done = 1;
                } else if (inside(mx, my, 34, 60, 126, 76)) {
                    app->config.language = W13_LANG_GERMAN;
                    W13_SetLanguage(app->config.language);
                    if (app->win) {
                        ClearMenuStrip(app->win);
                        setup_menus(app);
                    }
                    W13_SetStatus(app, W13_Text(W13_TX_MSG_LANGUAGE_CHANGED));
                    done = 1;
                } else if (inside(mx, my, 34, 82, 126, 98)) {
                    app->config.language = W13_LANG_POLISH;
                    W13_SetLanguage(app->config.language);
                    if (app->win) {
                        ClearMenuStrip(app->win);
                        setup_menus(app);
                    }
                    W13_SetStatus(app, W13_Text(W13_TX_MSG_LANGUAGE_CHANGED));
                    done = 1;
                }
            } else if (cls == IDCMP_RAWKEY && ((code & 0x7f) == 0x45)) {
                done = 1;
            }
        }
    }
    CloseWindow(win);
    if (app->win)
        W13_DrawAll(app);
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
    } else if (menu == 1 && item == 1) {
        W13_ShowUpdateInterval(app);
    } else if (menu == 1 && item == 2) {
        W13_ShowLanguage(app);
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
    WORD cy = 67;
    WORD radius = 26;

    if (!app || !app->win)
        return;
    rp = app->win->RPort;
    wind_x = wind_area_x(app);
    cx = (WORD)(wind_x + 110);
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
