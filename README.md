# Weather13

Weather13 is an AmigaOS/Kickstart 1.3 Workbench weather station prototype.

Current status:

- Opens a normal Workbench window, not a custom screen.
- Window position, size, and selected location are saved to `weather13.conf` on exit and restored on startup.
- Weather data is cached to `weather13.cache` on exit and loaded on startup before the online refresh attempt.
- Uses German text by default on Kickstart 1.3; English text can be enabled later through a Weather13-specific setting without locale.library.
- If the saved window position cannot be opened, Weather13 falls back to the top-left corner.
- Selects 2/3/4 bitplane weather icons from the current Workbench depth.
- Draws the current weather icon, temperature with apparent temperature in brackets, humidity, pressure, location, wind rose and a three-day forecast.
- Provides classic OS1.3 Intuition menus: `Project -> Info`, `Project -> Quit`, and `Settings -> Location...`.
- The Location window can search/set a place through Open-Meteo geocoding and then fetch current weather plus a three-day forecast.
- On startup Weather13 immediately tries to refresh online weather for the configured location; if no stack/Internet is available it keeps cached data, or demo data if no cache exists.
- Press `U` to refresh online weather for the configured location via `bsdsocket.library`.
- The window is dynamically resizable and the content follows the available width and height.
- Daily forecast columns also show maximum rain probability, dominant wind direction, and maximum wind speed.
- Uses Open-Meteo over plain HTTP; no API key is required.
- Falls back to cached data first, then dummy weather data until an online update succeeds.

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
