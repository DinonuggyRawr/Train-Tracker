# 🚆 Northeast Train Tracker

A Windows-based **live train tracking application written in C** for tracking MBTA Commuter Rail and Amtrak trains throughout New England.

Northeast Train Tracker combines live transit data, train locations, route information, upcoming stops, service alerts, distance calculations, and railroad radio-frequency references into one application designed for railfans and anyone interested in following trains across the Northeast.

---

## 🚉 Features

### 📍 Live Train Tracking
View currently active trains and information such as:

- Vehicle / train ID
- Car number
- Railroad / source
- Route
- Destination
- Current status
- Current or next stop
- Latitude and longitude
- Estimated speed
- Upcoming stops

### 🚆 MBTA + Amtrak Support

The tracker currently supports:

- **MBTA Commuter Rail**
- **Amtrak**

MBTA train information is retrieved using the **MBTA V3 API**, while Amtrak information is retrieved using the **Amtraker API v3**.

> Freight trains are currently not displayed because a suitable free live freight-location feed is not available.

---

## 🗺️ Interactive Map

The GUI includes a map view for displaying trains and railroad routes.

Map functionality includes:

- Live train positions
- Route lines
- Map tile loading and caching
- Zoom controls
- Map navigation
- Selected train information
- Route overview information

---

## 🔎 Train Search

Search active trains using keywords instead of needing to know an exact vehicle number.

Examples:

```text
list heritage trains
find Providence
search Amtrak
show Fitchburg
```

The search system can match information including:

- Railroad
- Route
- Destination
- Stop
- Vehicle ID
- Heritage equipment

Common filler words such as `all`, `active`, `train`, `show`, and `currently` are automatically ignored.

---

## 📏 Nearest Train & Distance Tools

Find trains near a particular station, city, or set of coordinates.

Example:

```text
nearest Worcester
```

or:

```text
nearest 42.35,-71.06
```

The tracker can return the **10 nearest active trains**.

You can also calculate the approximate distance between a specific train and a location:

```text
distance 1234 Providence
```

Distance calculations use geographic coordinates and the Haversine formula.

---

## 🚨 MBTA Service Alerts

Retrieve active service alerts for MBTA Commuter Rail routes.

```text
alerts 1234
```

or search directly by route ID.

Service alerts are currently available for **MBTA trains only**.

---

## 📻 Railroad Radio Frequencies

Northeast Train Tracker also includes a railroad radio-frequency reference system.

Search by railroad line or channel type:

```text
freq road Fitchburg
```

```text
freq dispatch Old Colony
```

Radio information can include:

- Railroad / line
- Channel type
- AAR channel
- Frequency
- Notes

---

## ⏱️ Watch Mode

The command-line version includes an automatic monitoring mode:

```text
watch
```

Watch mode refreshes the active train list every **30 seconds** until a key is pressed.

This makes it useful for continuously monitoring active equipment without manually refreshing the program.

---

## 🛤️ Route Summaries

Use:

```text
routes
```

to display active trains grouped by route.

The route summary can also track the number of recognized **heritage trains** operating on each route.

---

## ⭐ GUI Features

In addition to the command-line interface, the project contains a native Windows GUI.

The GUI includes functionality for:

- Train list
- Train details
- Upcoming stops
- Search and filtering
- Refreshing live data
- Route information
- MBTA alerts
- Favorites
- Saved searches
- Nearest-train searches
- Distance calculations
- Map view
- Delayed-train filtering
- Data export
- Notifications
- System tray support

---

## 🧠 Estimated Train Speed

When multiple location samples are available, Northeast Train Tracker can estimate train speed based on the distance traveled between reported positions.

Because this is calculated from successive API location updates, the displayed speed should be considered an **estimate**, not an official locomotive speed measurement.

---

## 🏙️ Location Search

The application contains a built-in list of cities and stations throughout the Northeast, allowing commands such as:

```text
nearest Boston
nearest Worcester
nearest Providence
nearest Lowell
nearest New Haven
nearest Portland
```

Coordinates can also be entered directly.

---

## 💻 Technology

The project is primarily written in **C** and uses native Windows APIs.

### Core Technologies

- C
- Win32 API
- Windows HTTP Services (`WinHTTP`)
- Windows Common Controls
- GDI / GDI+
- JSON parsing
- REST APIs
- Geographic coordinate calculations

The application communicates directly with web APIs rather than requiring a large external framework.

---

## 📂 Project Structure

```text
TrainTracker/
│
├── Main.c
│   └── Command-line interface and command handling
│
├── Gui.c
│   └── Native Windows graphical interface
│
├── TrainTracker.c
│   └── Core train tracking, API, parsing, location,
│       distance, alert, map, and radio functionality
│
├── TrainTracker.h
│   └── Shared structures, constants, and function declarations
│
└── README.md
```

---

## 🔧 Building

The project is designed for **Windows** and relies on Windows-specific libraries.

Required Windows libraries include:

```text
user32.lib
gdi32.lib
comctl32.lib
shell32.lib
comdlg32.lib
winhttp.lib
winmm.lib
```

Because the program uses Windows APIs such as WinHTTP and the Win32 GUI system, it should be compiled using a compatible Windows C compiler such as **Microsoft Visual C/C++ (MSVC)**.

---

## 🎮 Command-Line Commands

| Command | Description |
|---|---|
| `<train ID>` | Display information about a specific train |
| `list <query>` | Search active trains |
| `find <query>` | Search active trains |
| `search <query>` | Search active trains |
| `routes` | Show active trains grouped by route |
| `nearest <location>` | Find the nearest active trains |
| `distance <train> <location>` | Calculate distance from a train |
| `alerts <train/route>` | Display MBTA service alerts |
| `freq <query>` | Search railroad radio frequencies |
| `watch` | Automatically refresh train data |
| `help` | Display available commands |
| `Q` | Quit |

---

## 📡 Data Sources

Train information is retrieved from external transit data sources.

### MBTA

MBTA Commuter Rail information is retrieved through the **MBTA V3 API**.

### Amtrak

Amtrak information is retrieved through the **Amtraker API v3**.

Amtraker data is identified by the application as being provided under **ODC-By 1.0**.

Availability and accuracy of train information depend on the upstream services and the data being reported by each railroad.

---

## ⚠️ Disclaimer

Northeast Train Tracker is an independent project and is **not affiliated with, endorsed by, or operated by the MBTA, Amtrak, Keolis, or any other railroad or transit agency**.

Train positions, arrival predictions, speeds, delays, and other information may be delayed, estimated, incomplete, or unavailable.

**Do not use this application for safety-critical decisions or railroad operations.**

---

## 🚧 Future Development

Possible future improvements include:

- Additional Northeast railroads
- Improved train history
- More detailed delay statistics
- Better map visualization
- Additional notification options
- Expanded heritage equipment database
- Improved station searching
- Train movement history
- Additional railfan-focused tools
- Additional data sources when publicly available

---

## 📸 Screenshots

Screenshots of the application can be added here.

```text
[ GUI Screenshot ]

[ Map View Screenshot ]

[ Train Details Screenshot ]
```

---

## 🤝 Contributions

Contributions, bug reports, and feature suggestions are welcome.

If you find an issue, feel free to open a GitHub Issue describing:

- What happened
- What you expected to happen
- Steps to reproduce the problem
- Your Windows version
- Any relevant error messages

---

## 🚆 About the Project

Northeast Train Tracker was created as a C programming project combining an interest in **software development, trains, transit data, and railfanning**.

The goal is to create a practical desktop tool that makes publicly available train information easier and more enjoyable to explore.

---

### 🚉 Happy Railfanning!

*Track the train. Find the route. Catch the shot.*
