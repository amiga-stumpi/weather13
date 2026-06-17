#ifndef WEATHER13_NET_H
#define WEATHER13_NET_H

#include "weather_data.h"

int W13_FetchWeatherForLocation(const char *location, WeatherData *data, char *status, int status_size);

#endif
