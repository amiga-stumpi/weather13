#ifndef WEATHER13_NET_H
#define WEATHER13_NET_H

#include "weather_data.h"

int W13_FetchWeatherForLocation(const char *location, WeatherData *data, char *status, int status_size);
int W13_SearchLocations(const char *location, char names[][32], char labels[][64], int max_names, char *status, int status_size);

#endif
