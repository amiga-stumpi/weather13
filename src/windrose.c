#include "windrose.h"
#include <proto/graphics.h>

static const char *dirs16[] = {
    "N", "NNO", "NO", "ONO", "O", "OSO", "SO", "SSO",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
};

static const int dx16[] = { 0,  4,  7,  9, 10,  9,  7,  4, 0, -4, -7, -9, -10, -9, -7, -4 };
static const int dy16[] = {-10, -9, -7, -4,  0,  4,  7,  9,10,  9,  7,  4,   0, -4, -7, -9 };

const char *W13_WindDirName(int wind_dir_deg)
{
    int idx;
    while (wind_dir_deg < 0)
        wind_dir_deg += 360;
    wind_dir_deg %= 360;
    idx = ((wind_dir_deg + 11) / 22) & 15;
    return dirs16[idx];
}

static void draw_small_text(struct RastPort *rp, int x, int y, const char *s)
{
    int n = 0;
    while (s[n])
        ++n;
    Move(rp, x, y);
    Text(rp, (STRPTR)s, n);
}

void W13_DrawWindRose(struct RastPort *rp, int cx, int cy, int radius,
                      int wind_dir_deg, int wind_speed, WORD jitter_x, WORD jitter_y, UBYTE pen)
{
    int idx;
    int x;
    int y;
    (void)wind_speed;

    SetAPen(rp, pen);
    SetDrMd(rp, JAM1);

    Move(rp, cx, cy - radius);
    Draw(rp, cx + radius, cy);
    Draw(rp, cx, cy + radius);
    Draw(rp, cx - radius, cy);
    Draw(rp, cx, cy - radius);
    Move(rp, cx - radius, cy);
    Draw(rp, cx + radius, cy);
    Move(rp, cx, cy - radius);
    Draw(rp, cx, cy + radius);

    draw_small_text(rp, cx - 3, cy - radius - 5, "N");
    draw_small_text(rp, cx - 3, cy + radius + 11, "S");
    draw_small_text(rp, cx - radius - 12, cy + 3, "W");
    draw_small_text(rp, cx + radius + 6, cy + 3, "O");

    while (wind_dir_deg < 0)
        wind_dir_deg += 360;
    wind_dir_deg %= 360;
    idx = ((wind_dir_deg + 11) / 22) & 15;
    x = cx + (dx16[idx] * (radius - 5)) / 10 + jitter_x;
    y = cy + (dy16[idx] * (radius - 5)) / 10 + jitter_y;
    Move(rp, cx, cy);
    Draw(rp, x, y);
    Move(rp, x - 2, y);
    Draw(rp, x + 2, y);
    Move(rp, x, y - 2);
    Draw(rp, x, y + 2);


}
