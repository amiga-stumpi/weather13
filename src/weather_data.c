#include "weather_data.h"
#include <proto/dos.h>
#include <dos/dos.h>

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

void W13_FillDummyWeather(WeatherData *data)
{
    if (!data)
        return;
    copy_text(data->location, sizeof(data->location), "Muenster");
    data->temp_tenths = 214;
    data->apparent_temp_tenths = 207;
    data->humidity = 63;
    data->pressure = 1016;
    data->wind_speed = 12;
    data->wind_dir_deg = 225;
    data->weather_code = W13_WEATHER_PARTLY;
    copy_text(data->condition, sizeof(data->condition), "Leicht bewoelkt");
    copy_text(data->updated, sizeof(data->updated), "Demo 14:30");

    copy_text(data->forecast[0].name, sizeof(data->forecast[0].name), "Heute");
    data->forecast[0].temp_day = 23;
    data->forecast[0].temp_night = 16;
    data->forecast[0].weather_code = W13_WEATHER_PARTLY;
    data->forecast[0].precip_probability = 25;
    data->forecast[0].wind_speed_max = 18;
    data->forecast[0].wind_dir_deg = 225;

    copy_text(data->forecast[1].name, sizeof(data->forecast[1].name), "Morgen");
    data->forecast[1].temp_day = 25;
    data->forecast[1].temp_night = 17;
    data->forecast[1].weather_code = W13_WEATHER_SUN;
    data->forecast[1].precip_probability = 10;
    data->forecast[1].wind_speed_max = 14;
    data->forecast[1].wind_dir_deg = 180;

    copy_text(data->forecast[2].name, sizeof(data->forecast[2].name), "Uebermorgen");
    data->forecast[2].temp_day = 19;
    data->forecast[2].temp_night = 13;
    data->forecast[2].weather_code = W13_WEATHER_RAIN;
    data->forecast[2].precip_probability = 75;
    data->forecast[2].wind_speed_max = 26;
    data->forecast[2].wind_dir_deg = 270;
}


#define W13_CACHE_FILE "weather13.cache"
#define W13_CACHE_MAGIC 0x57313343UL
#define W13_CACHE_VERSION 1UL

typedef struct WeatherCacheFile {
    ULONG magic;
    ULONG version;
    WeatherData data;
} WeatherCacheFile;

int W13_LoadWeatherCache(WeatherData *data)
{
    BPTR fh;
    WeatherCacheFile cache;
    LONG got;

    if (!data)
        return 0;
    fh = Open((STRPTR)W13_CACHE_FILE, MODE_OLDFILE);
    if (!fh)
        return 0;
    got = Read(fh, &cache, sizeof(cache));
    Close(fh);
    if (got != (LONG)sizeof(cache))
        return 0;
    if (cache.magic != W13_CACHE_MAGIC || cache.version != W13_CACHE_VERSION)
        return 0;
    *data = cache.data;
    return 1;
}

int W13_SaveWeatherCache(const WeatherData *data)
{
    BPTR fh;
    WeatherCacheFile cache;
    LONG written;

    if (!data)
        return 0;
    cache.magic = W13_CACHE_MAGIC;
    cache.version = W13_CACHE_VERSION;
    cache.data = *data;
    fh = Open((STRPTR)W13_CACHE_FILE, MODE_NEWFILE);
    if (!fh)
        return 0;
    written = Write(fh, &cache, sizeof(cache));
    Close(fh);
    return written == (LONG)sizeof(cache);
}
