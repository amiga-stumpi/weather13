# Weather13

Weather13 is an AmigaOS/Kickstart 1.3 Workbench weather station prototype.

Current status:

- Opens a normal Workbench window, not a custom screen.
- Window position, size, and selected location are saved to `weather13.conf` on exit and restored on startup.
- Uses German text by default on Kickstart 1.3; English text can be enabled later through a Weather13-specific setting without locale.library.
- If the saved window position cannot be opened, Weather13 falls back to the top-left corner.
- Selects 2/3/4 bitplane weather icons from the current Workbench depth.
- Draws the current weather icon, temperature with apparent temperature in brackets, humidity, pressure, location, wind rose and a three-day forecast.
- Provides classic OS1.3 Intuition menus: `Project -> Info`, `Project -> Quit`, and `Settings -> Location...`.
- The Location window stores the selected place name locally; the Open-Meteo geocoding endpoint to connect next is `geocoding-api.open-meteo.com/v1/search?name=...`.
- The window is dynamically resizable and the content follows the available width and height.
- Daily forecast columns also show maximum rain probability, dominant wind direction, and maximum wind speed.
- Uses dummy weather data for the first graphics milestone.
- Network/API support is intentionally not implemented yet.

Build:

```sh
make clean && make
```

Output:

```text
build/Weather13
```

Keys:

- `U` redraws/refreshes dummy data.
- `Q` or the close gadget exits.
