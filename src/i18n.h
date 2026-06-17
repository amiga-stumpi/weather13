#ifndef WEATHER13_I18N_H
#define WEATHER13_I18N_H

#include <exec/types.h>

typedef enum W13Language {
    W13_LANG_ENGLISH = 0,
    W13_LANG_GERMAN,
    W13_LANG_POLISH
} W13Language;

typedef enum W13TextId {
    W13_TX_UPDATE = 0,
    W13_TX_HUMIDITY,
    W13_TX_PRESSURE,
    W13_TX_DAY,
    W13_TX_NIGHT,
    W13_TX_RAIN,
    W13_TX_WIND,
    W13_TX_END,
    W13_TX_DEMO_DATA,
    W13_TX_TODAY,
    W13_TX_TOMORROW,
    W13_TX_DAY_AFTER,
    W13_TX_COUNT
} W13TextId;

void W13_InitLanguage(void);
void W13_SetLanguage(int language);
int W13_GetLanguage(void);
int W13_IsEnglish(void);
const char *W13_Text(W13TextId id);
const char *W13_ConditionText(int weather_code);

#endif
