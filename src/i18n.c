#include "i18n.h"
#include "weather_icons.h"

static int g_english;

static const char *de_texts[W13_TX_COUNT] = {
    "Update",
    "Feuchte",
    "Druck",
    "Tag",
    "Nacht",
    "Regen",
    "Wind",
    "Ende",
    "Demo-Daten",
    "Heute",
    "Morgen",
    "Uebermorgen"
};

static const char *en_texts[W13_TX_COUNT] = {
    "Update",
    "Humidity",
    "Pressure",
    "Day",
    "Night",
    "Rain",
    "Wind",
    "Quit",
    "Demo data",
    "Today",
    "Tomorrow",
    "Day after"
};

static const char *de_conditions[W13_WEATHER_COUNT] = {
    "Sonnig",
    "Leicht bewoelkt",
    "Bewoelkt",
    "Regen",
    "Gewitter",
    "Nebel",
    "Schnee",
    "Unbekannt"
};

static const char *en_conditions[W13_WEATHER_COUNT] = {
    "Sunny",
    "Partly cloudy",
    "Cloudy",
    "Rain",
    "Storm",
    "Fog",
    "Snow",
    "Unknown"
};

void W13_InitLanguage(void)
{
    g_english = 0;
}

int W13_IsEnglish(void)
{
    return g_english;
}

const char *W13_Text(W13TextId id)
{
    if (id < 0 || id >= W13_TX_COUNT)
        id = W13_TX_DEMO_DATA;
    return g_english ? en_texts[id] : de_texts[id];
}

const char *W13_ConditionText(int weather_code)
{
    if (weather_code < 0 || weather_code >= W13_WEATHER_COUNT)
        weather_code = W13_WEATHER_UNKNOWN;
    return g_english ? en_conditions[weather_code] : de_conditions[weather_code];
}
