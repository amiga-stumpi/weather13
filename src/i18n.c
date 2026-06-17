#include "i18n.h"
#include "weather_icons.h"

static int g_language = W13_LANG_ENGLISH;

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

static const char *pl_texts[W13_TX_COUNT] = {
    "Aktualizacja",
    "Wilgotnosc",
    "Cisnienie",
    "Dzien",
    "Noc",
    "Deszcz",
    "Wiatr",
    "Koniec",
    "Dane demo",
    "Dzisiaj",
    "Jutro",
    "Pojutrze"
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

static const char *pl_conditions[W13_WEATHER_COUNT] = {
    "Slonecznie",
    "Lekko pochmurno",
    "Pochmurno",
    "Deszcz",
    "Burza",
    "Mgla",
    "Snieg",
    "Nieznane"
};

void W13_InitLanguage(void)
{
    g_language = W13_LANG_ENGLISH;
}

void W13_SetLanguage(int language)
{
    if (language < W13_LANG_ENGLISH || language > W13_LANG_POLISH)
        language = W13_LANG_ENGLISH;
    g_language = language;
}

int W13_GetLanguage(void)
{
    return g_language;
}

int W13_IsEnglish(void)
{
    return g_language == W13_LANG_ENGLISH;
}

const char *W13_Text(W13TextId id)
{
    if (id < 0 || id >= W13_TX_COUNT)
        id = W13_TX_DEMO_DATA;
    if (g_language == W13_LANG_GERMAN)
        return de_texts[id];
    if (g_language == W13_LANG_POLISH)
        return pl_texts[id];
    return en_texts[id];
}

const char *W13_ConditionText(int weather_code)
{
    if (weather_code < 0 || weather_code >= W13_WEATHER_COUNT)
        weather_code = W13_WEATHER_UNKNOWN;
    if (g_language == W13_LANG_GERMAN)
        return de_conditions[weather_code];
    if (g_language == W13_LANG_POLISH)
        return pl_conditions[weather_code];
    return en_conditions[weather_code];
}
