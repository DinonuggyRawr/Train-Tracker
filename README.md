# Northeast Train Tracker

A native Windows train tracker written in C for following **MBTA Commuter Rail** and **Amtrak** trains. It brings active trains, route and stop information, service alerts, radio-frequency references, and a map into one desktop application. A command-line interface is included as well.

## Screenshots

### Active trains

Browse trains by source, line, destination, current stop, status, and estimated speed.

![Main train list in the Windows application](screenshots/train-list.png)

### Map view

See reported train positions and MBTA route lines; click a marker to inspect a train.

![Map with route lines, train markers, and a selected train](screenshots/map-view.png)

### Train details and upcoming stops

Select a train to see its position, last update, upcoming stops, predictions, and available delay information.

![Selected Lowell Line train with details and upcoming stops](screenshots/train-details.png)

## Features

- Live MBTA Commuter Rail and Amtrak train lists, with search and filtering.
- Map with train markers, MBTA route lines, zoom controls, and OpenStreetMap tiles.
- Train details, upcoming stops, arrival and departure predictions where available, and MBTA route alerts.
- Nearest-train search and train-to-location distance calculations using coordinates or recognized city/station names.
- Estimated speeds calculated from successive reported positions. These are estimates, and may initially display as unavailable.
- Route summaries, a railroad radio-frequency reference, favorites, saved searches, notifications, a delayed-train filter, and CSV export in the GUI.
- Automatic refresh every 30 seconds in watch mode.

The application does not provide live freight train positions.

## Build on Windows

Use the **x64 Native Tools Command Prompt for Visual Studio** (or another MSVC developer prompt). From the folder containing these source files, build either application:

```bat
cl /nologo /W3 /TC /Fe:TrainTrackerGUI.exe Gui.c TrainTracker.c /link /SUBSYSTEM:WINDOWS comctl32.lib user32.lib gdi32.lib shell32.lib comdlg32.lib winhttp.lib winmm.lib
cl /nologo /W3 /TC /Fe:TrainTrackerCLI.exe Main.c TrainTracker.c /link /SUBSYSTEM:CONSOLE winhttp.lib
```

Run `TrainTrackerGUI.exe` for the desktop interface or `TrainTrackerCLI.exe` for the terminal interface. An internet connection is needed to load current train data and map tiles. The program makes HTTPS requests through Windows WinHTTP.

## Command-line usage

Start `TrainTrackerCLI.exe` and enter a command at its prompt:

| Command | Purpose |
| --- | --- |
| `<train ID>` | Show details for an active train. |
| `list <query>`, `show <query>`, `find <query>`, `search <query>` | Filter active trains by keywords such as line, destination, source, stop, or heritage equipment. |
| `routes` | Summarize active trains by line. |
| `nearest <place or lat,lon>` | List up to 10 nearby active trains. |
| `distance <train ID> <place or lat,lon>` | Estimate distance between a train and a location. |
| `alerts <train ID or route ID>` | Show available MBTA route alerts. |
| `freq <line or channel keywords>` | Search the built-in railroad radio-frequency reference. |
| `watch` | Refresh the active train list every 30 seconds until a key is pressed. |
| `help` or `?` | Show command help. |
| `Q` | Quit. |

For example:

```text
find Providence
nearest Lowell
nearest 42.35,-71.06
distance 1234 Worcester
freq road Fitchburg
```

Train IDs in examples are illustrative; use an ID currently shown by the application.

## Project files

```text
Northeast-Train-Tracker/
├── Gui.c              # Win32 desktop application
├── Main.c             # Command-line application
├── TrainTracker.c     # Data retrieval and shared train logic
├── TrainTracker.h     # Shared types and declarations
├── README.md
└── screenshots/
    ├── train-list.png
    ├── map-view.png
    └── train-details.png
```

## Data and attribution

- MBTA Commuter Rail data: [MBTA V3 API](https://www.mbta.com/developers/v3-api).
- Amtrak data: [Amtraker API v3](https://amtraker.com/) (identified in the application as ODC-By 1.0).
- Map tiles: [OpenStreetMap contributors](https://www.openstreetmap.org/copyright).

Positions, predictions, alerts, and availability depend on the upstream feeds. This is an independent project and is not affiliated with the MBTA, Amtrak, or their operators. Do not rely on it for railroad operations or safety decisions.

## Contributing

Issues and suggestions are welcome. Include the Windows version, steps to reproduce, the expected result, and any error message when reporting a bug.
