# Codex Briefing: Weather13 für AmigaOS 1.3

## Ziel

Baue ein AmigaOS-1.3-Programm namens `Weather13`.

Das Programm ist eine reine Online-Wetteranzeige und Vorhersagestation. Es soll **kein eigener Screen** sein, sondern als normales **Workbench-Fenster** laufen. Die Darstellung soll sich trotzdem wie eine kleine grafische Wetterstation anfühlen:

- große aktuelle Temperatur mittig
- aktuelles Wettersymbol links
- Windrose rechts mit Zeiger, der die Windrichtung zeigt
- Windgeschwindigkeit und Windrichtung als Text
- Vorschau der nächsten drei Tage mit Tag-/Nacht-Temperatur
- kleine Wettericons für die Vorschau
- automatische Anpassung an die Workbench-Farbtiefe: 2, 3 oder 4 Bitplanes

## Plattform

Target:

- AmigaOS 1.3
- C, kompatibel mit bebbo/amiga-gcc
- typischer Compiler: `m68k-amigaos-gcc`
- CRT z. B. `-mcrt=nix13`
- keine OS2.x-spezifischen APIs voraussetzen
- kleine Stacks beachten
- keine Threads
- Eventloop über Intuition + Timer/Polling

## Fenster statt eigener Screen

Das Programm öffnet ein Fenster auf der Workbench:

- `WBENCHSCREEN`
- `WINDOWDRAG`
- `WINDOWDEPTH`
- `WINDOWCLOSE`
- `SMART_REFRESH`
- `ACTIVATE`
- IDCMP mindestens:
  - `CLOSEWINDOW`
  - `REFRESHWINDOW`
  - `RAWKEY`
  - optional `MOUSEBUTTONS`

Beispielgröße für HighRes-Workbench:

- Breite: ca. 600 px
- Höhe: ca. 180 bis 200 px

Falls die aktuelle Workbench kleiner ist, sollte das Layout auf ca. 320 px Breite fallbacken oder das Fenster kleiner öffnen.

## Bitplane-/Farbtiefenstrategie

Nach `OpenWindow()`:

```c
screen_depth = win->WScreen->BitMap.Depth;
if (screen_depth >= 4) asset_depth = 4;
else if (screen_depth == 3) asset_depth = 3;
else asset_depth = 2;
```

Das Iconpaket enthält Icons für:

- 2 Bitplanes = 4 Farben
- 3 Bitplanes = 8 Farben
- 4 Bitplanes = 16 Farben

Wichtig:

- `pen 0` ist Hintergrund/transparent.
- Nicht ungefragt die Workbench-Palette ändern.
- Für 2 Bitplanes muss die Darstellung weiterhin klar lesbar sein.
- In 3/4 Bitplanes dürfen zusätzliche Pens für Sonne/Regen/Wolken genutzt werden.

## Mitgelieferte Dateien

Im Ordner `c/`:

- `weather_icons.h`
- `weather_icons.c`

Im Ordner `png/`:

- Einzel-PNGs als Vorschau
- Icon-Sheets:
  - `weather13_sheet_2bp.png`
  - `weather13_sheet_3bp.png`
  - `weather13_sheet_4bp.png`

Die PNGs sind nur Vorschau/Arbeitsmaterial. Für das Amiga-Programm sind die C-Dateien relevant.

## Icon-API

Header:

```c
const Weather13Icon *Weather13_GetIcon(UWORD weather_code,
                                       UWORD wanted_size,
                                       UWORD screen_depth);
```

`wanted_size`:

- `48` für das große aktuelle Wettericon
- `24` für die Forecast-Icons

`screen_depth`:

- tatsächliche Workbench-Tiefe über `win->WScreen->BitMap.Depth`

Die Funktion liefert automatisch das passende 2bp/3bp/4bp-Icon zurück.

## Icon-Datenformat

```c
typedef struct Weather13Icon {
    UWORD width;
    UWORD height;
    UWORD depth;
    UWORD row_words;
    const UWORD *planes;
    const UWORD *mask;
} Weather13Icon;
```

Format:

- Amiga-planar
- plane-major
- `row_words = ((width + 15) / 16)`
- pro Plane:
  - alle Zeilen
  - je Zeile `row_words` UWORDs
- linkes Pixel eines 16-Pixel-Worts liegt in Bit 15
- `mask` ist 1 Bitplane, row-major
- Maskenbit ist 1, wenn der Pixel nicht pen 0 ist

## Wettercodes intern

```c
#define W13_WEATHER_SUN      0
#define W13_WEATHER_PARTLY   1
#define W13_WEATHER_CLOUD    2
#define W13_WEATHER_RAIN     3
#define W13_WEATHER_STORM    4
#define W13_WEATHER_FOG      5
#define W13_WEATHER_SNOW     6
#define W13_WEATHER_UNKNOWN  7
#define W13_WEATHER_COUNT    8
```

Die API-Wettercodes des Online-Dienstes sollen auf diese internen Codes gemappt werden.

## Empfohlene Datenstruktur

```c
typedef struct ForecastDay {
    char name[12];       /* Heute, Morgen, Mi, Do ... */
    int temp_day;        /* Grad Celsius, ganzzahlig */
    int temp_night;      /* Grad Celsius, ganzzahlig */
    int weather_code;    /* W13_WEATHER_* */
} ForecastDay;

typedef struct WeatherData {
    char location[32];

    int temp_tenths;     /* 214 = 21.4 C */
    int humidity;        /* Prozent */
    int pressure;        /* hPa */

    int wind_speed;      /* km/h */
    int wind_dir_deg;    /* 0..359, meteorologisch: Wind kommt aus dieser Richtung */

    int weather_code;    /* W13_WEATHER_* */
    char condition[32];

    char updated[20];    /* z. B. "14:30" oder "2026-06-16 14:30" */

    ForecastDay forecast[3];
} WeatherData;
```

Temperatur nicht als float speichern. Für `21.4 C`:

```c
int whole = temp_tenths / 10;
int frac = temp_tenths % 10;
```

Bei negativen Temperaturen Sonderfall sauber behandeln.

## Layout-Vorschlag

Für ca. 600x190 px:

```text
+----------------------------------------------------------+
| Weather13                                  Update 14:30  |
+-------------------+----------------------+---------------+
|                   |                      |    WIND       |
|   [48px Icon]     |        21.4 C        |      N        |
|                   |      Muenster        |   W--+--O     |
|   Bewoelkt        |    leicht bewoelkt   |      S        |
|                   |                      |  12 km/h SW   |
+-------------------+----------------------+---------------+
| Heute             | Morgen               | Uebermorgen   |
| [24px] Tag 23 C   | [24px] Tag 25 C      | [24px] Tag 19 C|
|        Nacht 16 C |        Nacht 17 C    |        Nacht 13 C|
+----------------------------------------------------------+
| U Update    1-3 Ort    Q Ende                            |
+----------------------------------------------------------+
```

Koordinatenvorschlag:

- Fenster-Inhalt ab `(4, 12)`
- linker Iconbereich:
  - x=8, y=28, w=170, h=82
  - großes Icon bei x=20, y=36
  - Text Zustand bei x=82, y=58
- Mittelbereich:
  - x=180, y=28, w=235, h=82
  - große Temperatur zentriert
  - Ort darunter
- Windbereich:
  - x=420, y=28, w=170, h=82
  - Windrose Zentrum ca. x=505, y=63
  - Radius ca. 30
- Forecast:
  - x=8, y=118, w=584, h=54
  - drei Spalten je ca. 194 px

## Große Temperaturanzeige

Die Temperatur soll nicht einfach in Topaz 8 erscheinen.

Empfehlung:

- eigene Bitmap-Ziffern oder
- doppelt/geblockt gezeichnete Ziffern
- Anzeige zentral als `21.4 C`

Erste Implementierung darf mit `Text()` starten. Danach kann Codex eine einfache 7-Segment- oder Blockziffer-Funktion ergänzen.

## Windrose

Die Windrose wird nicht als Bitmap gespeichert, sondern vektorbasiert gezeichnet.

Funktionen:

```c
void W13_DrawWindRose(struct RastPort *rp, int cx, int cy, int radius,
                      int wind_dir_deg, int wind_speed);
```

Anzeige:

- Kreis oder Oktagon
- N/O/S/W, optional NO/SO/SW/NW
- Zeiger vom Zentrum nach außen
- Text darunter: `12 km/h SW`

Die Windrichtung ist meteorologisch: Wind kommt aus dieser Richtung. Wenn API `225` meldet, Zeiger nach SW.

Keine Floating-Point-Pflicht. Möglichkeiten:

1. 16-Richtungen-Lookup-Tabelle
2. Integer-Sin/Cos-Tabelle
3. nur 8 Richtungen für erste Version

Empfohlene erste Version: 16 Richtungen.

Richtungsname:

```c
static const char *dirs16[] = {
  "N","NNO","NO","ONO","O","OSO","SO","SSO",
  "S","SSW","SW","WSW","W","WNW","NW","NNW"
};
idx = ((wind_dir_deg + 11) / 22) & 15;
```

## Windzeiger-Animation

Beim neuen Datensatz:

- alten Winkel merken
- neuen Winkel berechnen
- in kleinen Schritten neu zeichnen
- nur Windbereich invalidieren/neuzeichnen

Für OS1.3 lieber robust:

```c
for (...) {
    draw_wind_panel(... angle ...);
    Delay(1);
}
```

Nicht versuchen, einzelne Linien XOR-artig zu löschen, außer sehr kontrolliert. Komplettes Panel neuzeichnen ist sicherer.

## Netzwerk / API ohne Proxy

Anforderung: kein Proxy.

Das Programm soll direkt einen Wetterdienst per HTTP abrufen. Wichtig ist ein Dienst ohne HTTPS-Zwang oder ein Endpunkt, der per HTTP erreichbar ist. Die konkrete API muss vor der Implementierung verifiziert werden, da viele moderne Dienste HTTPS erzwingen.

HTTP-Client:

- DNS auflösen
- TCP Port 80 verbinden
- `GET <path> HTTP/1.0`
- `Host: <server>`
- `User-Agent: Weather13/0.1`
- leere Zeile
- Antwort lesen
- Header bis `\r\n\r\n` überspringen
- Body parsen

Keine Chunked-Encoding-Unterstützung in Version 1 erzwingen. Mit HTTP/1.0 und `Connection: close` arbeiten.

## Parser-Strategie

Falls die API JSON liefert:

- keinen vollständigen JSON-Parser bauen
- gezielt bekannte Felder suchen
- Arrays mit einfachen Hilfsfunktionen auslesen
- robust gegen Whitespace

Beispielfunktionen:

```c
int json_find_number(const char *body, const char *key, int *out);
int json_find_array_number(const char *body, const char *key, int index, int *out);
```

Falls der Dienst CSV oder Text liefern kann, dieses Format bevorzugen.

## API-Mapping

Codex soll eine Datei `weather_api.c` bauen, die nur diese Aufgabe hat:

```c
int W13_FetchWeather(const W13Config *cfg, WeatherData *out);
```

Intern:

1. URL aus config bauen
2. HTTP GET ausführen
3. Body parsen
4. Wettercodes auf `W13_WEATHER_*` mappen
5. `WeatherData` füllen

## Konfiguration

Einfache Textdatei `Weather13.cfg` im Programmverzeichnis:

```text
SERVER=example-weather-api.org
PATH=/weather?lat=51.96&lon=7.63&days=3&units=metric
LOCATION=Muenster
UPDATE_MINUTES=30
```

Später mehrere Orte:

```text
LOCATION1=Muenster;51.96;7.63
LOCATION2=Coesfeld;51.94;7.17
LOCATION3=Berlin;52.52;13.41
```

## Programmstruktur

Vorgeschlagene Dateien:

```text
src/weather13.c
src/weather_data.h
src/weather_config.c
src/weather_config.h
src/http_client.c
src/http_client.h
src/weather_api.c
src/weather_api.h
src/ui.c
src/ui.h
src/windrose.c
src/windrose.h
src/weather_icons.c
src/weather_icons.h
Makefile
```

## Eventloop

Pseudoablauf:

```c
open_libraries();
load_config();
open_workbench_window();
detect_depth();
load_initial_weather_or_show_placeholder();
draw_all();

while (!done) {
    wait on window user port or timer signal;

    if CLOSEWINDOW or Q:
        done = 1;

    if REFRESHWINDOW:
        BeginRefresh(win);
        draw_all();
        EndRefresh(win, TRUE);

    if U pressed or update timer expired:
        show_status("Update...");
        if (W13_FetchWeather(&cfg, &newdata)) {
            old_angle = data.wind_dir_deg;
            data = newdata;
            draw_all();
            animate_wind_to(old_angle, data.wind_dir_deg);
        } else {
            show_status("Keine Wetterdaten");
        }
}
```

## Bedienung

Minimal:

- `U` = sofort aktualisieren
- `Q` oder Close-Gadget = beenden
- `1`, `2`, `3` = Ort wechseln, falls mehrere Orte konfiguriert sind

## Fehlerfälle

Das Programm soll grafisch stabil bleiben, auch wenn Netzwerk fehlschlägt.

Anzeigen:

- `Keine Verbindung`
- `DNS Fehler`
- `HTTP Fehler`
- `Parse Fehler`
- letzter erfolgreicher Datensatz bleibt sichtbar, wenn vorhanden

## Build-Hinweis

Für bebbo/amiga-gcc ungefähr:

```make
CC=m68k-amigaos-gcc
CFLAGS=-mcrt=nix13 -Os -Wall -fomit-frame-pointer
LDFLAGS=-mcrt=nix13

weather13: weather13.o ui.o windrose.o weather_icons.o http_client.o weather_api.o weather_config.o
	$(CC) $(LDFLAGS) -o weather13 $^
```

`bsdsocket.library` Socket-Aufrufe sauber kapseln.

## Wichtige Constraints

- Kein HTTPS/TLS im Amiga-Programm.
- Kein Proxy.
- HTTP API muss tatsächlich per Port 80 erreichbar sein.
- Keine großen dynamischen Speicherblöcke.
- Keine vollständige JSON-Library, wenn vermeidbar.
- Keine OS2.x-only APIs.
- Pen 0 der Icons nicht als feste Farbe verstehen, sondern als transparent/Hintergrund.
- Workbench-Palette nicht ungefragt verändern.

## Nächster sinnvoller Codex-Auftrag

1. Projektgerüst anlegen.
2. `weather_icons.c/.h` integrieren.
3. Workbench-Fenster öffnen.
4. Dummy-Wetterdaten anzeigen.
5. Windrose vektorbasiert zeichnen.
6. Icons abhängig von Workbench-Depth auswählen.
7. Danach erst HTTP/API ergänzen.

So kann die Grafik vollständig getestet werden, bevor Netzwerk und API dazukommen.
