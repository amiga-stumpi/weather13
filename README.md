# Weather13

Weather13 is an AmigaOS/Kickstart 1.3 Workbench weather station prototype.

Current status:

- Opens a normal Workbench window, not a custom screen.
- Window position, size, selected location, and update interval are saved to `weather13.conf` on exit and restored on startup.
- Weather data is cached to `weather13.cache` on exit and loaded on startup before the online refresh attempt.
- Uses English text by default. Language is selected manually through `Settings -> Languages` and saved to `weather13.conf`; no locale.library auto-detection is used.
- If the saved window position cannot be opened, Weather13 falls back to the top-left corner.
- Selects 2/3/4 bitplane weather icons from the current Workbench depth.
- Draws the current weather icon, temperature with apparent temperature in brackets, humidity, pressure, location, wind rose and a three-day forecast.
- Provides classic OS1.3 Intuition menus: `Project -> Info`, `Project -> Quit`, `Settings -> Location...`, `Settings -> Update Interval...`, and `Settings -> Languages`.
- The Location window can search/set a place through Open-Meteo geocoding and then fetch current weather plus a three-day forecast.
- On startup Weather13 immediately tries to refresh online weather for the configured location; if no stack/Internet is available it keeps cached data, or demo data if no cache exists. After that it refreshes automatically at the configured interval.
- Press `U` to refresh online weather for the configured location via `bsdsocket.library`.
- The window is dynamically resizable and the content follows the available width and height.
- Daily forecast columns also show maximum rain probability, dominant wind direction, and maximum wind speed.
- Uses Open-Meteo over plain HTTP; no API key is required.
- Falls back to cached data first, then dummy weather data until an online update succeeds.
- The update interval is configured in minutes through `Settings -> Update Interval...`; valid values are 5 to 120 minutes, default 30.
- Available UI languages are English, German, Polish, and Greek; default is English.

Build:

```sh
make clean && make
```

Output:

```text
build/Weather13
```

Keys:

- `U` refreshes online weather for the configured location.
- `Q` or the close gadget exits.
