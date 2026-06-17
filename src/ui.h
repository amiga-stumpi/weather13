#ifndef WEATHER13_UI_H
#define WEATHER13_UI_H

#include <intuition/intuition.h>
#include "weather_data.h"

typedef struct W13WindowConfig {
    WORD left;
    WORD top;
    WORD width;
    WORD height;
    char location[32];
} W13WindowConfig;

typedef struct W13App {
    struct Window *win;
    WeatherData data;
    UWORD screen_depth;
    UWORD asset_depth;
    W13WindowConfig config;
    char status[64];
    WORD wind_jitter_x;
    WORD wind_jitter_y;
    UWORD anim_ticks;
    UWORD anim_next;
    UWORD anim_phase;
} W13App;

void W13_LoadConfig(W13WindowConfig *cfg);
void W13_SaveConfig(const W13WindowConfig *cfg);
int W13_Open(W13App *app);
void W13_Close(W13App *app);
void W13_DrawAll(W13App *app);
void W13_SetStatus(W13App *app, const char *status);
int W13_HandleMenu(W13App *app, UWORD code);
void W13_ShowInfo(W13App *app);
void W13_ShowLocation(W13App *app);

#endif
