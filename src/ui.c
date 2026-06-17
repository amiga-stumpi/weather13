#include "ui.h"
#include "windrose.h"
#include "i18n.h"
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <string.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

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
            if (pos > 0 && line[0] != '#')
                config_set_value(cfg, line);
            pos = 0;
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }
    if (pos > 0) {
        line[pos] = 0;
        if (line[0] != '#')
            config_set_value(cfg, line);
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
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY | IDCMP_NEWSIZE;
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
        nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY;
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
    return 1;
}

void W13_Close(W13App *app)
{
    if (app && app->win) {
        app->config.left = app->win->LeftEdge;
        app->config.top = app->win->TopEdge;
        app->config.width = app->win->Width;
        app->config.height = app->win->Height;
        CloseWindow(app->win);
        app->win = 0;
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
    W13_DrawWindRose(rp, wind_x + 86, 72, 26, app->data.wind_dir_deg, app->data.wind_speed, 1);

    draw_forecast(app, 8, forecast_y - 2, (WORD)(w - 18));

    line[0] = 0;
    cat_text(line, sizeof(line), "U ");
    cat_text(line, sizeof(line), W13_Text(W13_TX_UPDATE));
    cat_text(line, sizeof(line), "    Q ");
    cat_text(line, sizeof(line), W13_Text(W13_TX_END));
    cat_text(line, sizeof(line), "    ");
    cat_text(line, sizeof(line), W13_Text(W13_TX_DEMO_DATA));
    draw_text(rp, 8, h - 8, line);
}
