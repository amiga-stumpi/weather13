#ifndef WEATHER13_DATA_H
#define WEATHER13_DATA_H

#include <exec/types.h>
#include "weather_icons.h"

typedef struct ForecastDay {
    char name[12];
    int temp_day;
    int temp_night;
    int weather_code;
    int precip_probability;
    int wind_speed_max;
    int wind_dir_deg;
} ForecastDay;

typedef struct WeatherData {
    char location[32];
    int temp_tenths;
    int apparent_temp_tenths;
    int humidity;
    int pressure;
    int wind_speed;
    int wind_dir_deg;
    int weather_code;
    char condition[32];
    char updated[20];
    ForecastDay forecast[3];
} WeatherData;

void W13_FillDummyWeather(WeatherData *data);
int W13_LoadWeatherCache(WeatherData *data);
int W13_SaveWeatherCache(const WeatherData *data);

#endif
