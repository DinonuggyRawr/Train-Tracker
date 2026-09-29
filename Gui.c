#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <winhttp.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#include "TrainTracker.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "winmm.lib")

#define ID_FILTER 1001
#define ID_REFRESH 1002
#define ID_TRAIN_LIST 1003
#define ID_DETAILS 1004
#define ID_STATUS 1005
#define ID_TRAINS_VIEW 1006
#define ID_RADIO_VIEW 1007
#define ID_RUN_COMMAND 1008
#define ID_WATCH 1009
#define ID_DETAIL_LIST 1010
#define ID_STOPS_LIST 1011
#define ID_FAVORITES_VIEW 1012
#define ID_SAVED_SEARCH 1013
#define ID_SAVE_SEARCH 1014
#define ID_FAVORITE 1015
#define ID_SELECTED_ALERTS 1016
#define ID_NOTIFY 1017
#define ID_NEAREST 1018
#define ID_DISTANCE 1019
#define ID_MAP_VIEW 1020
#define ID_EXPORT 1021
#define ID_DELAYED_FILTER 1022
#define ID_MAP_ZOOM_IN 1023
#define ID_MAP_ZOOM_OUT 1024
#define ID_MAP_FULL_DETAILS 1025
#define ID_MAP_OVERVIEW 1026
#define WM_REFRESH_COMPLETE (WM_APP + 1)
#define WM_ALERTS_COMPLETE (WM_APP + 2)
#define WM_DETAILS_COMPLETE (WM_APP + 3)
#define WM_TRAY_CALLBACK (WM_APP + 4)
#define WM_MAP_DATA_COMPLETE (WM_APP + 5)
#define WM_EASTER_EGG_COMPLETE (WM_APP + 6)
#define EASTER_EGG_TIMER 2

#define VIEW_TRAINS 0
#define VIEW_RADIO 1
#define VIEW_ROUTES 2
#define VIEW_NEAREST 3
#define VIEW_ALERTS 4
#define VIEW_DETAIL 5
#define VIEW_FAVORITES 6
#define VIEW_MAP 7

#define MAX_FAVORITES 64
#define MAX_SAVED_SEARCHES 16
#define MAX_STATION_ALERTS 32
#define MAX_DELAY_SAMPLES 512
#define MAX_MAP_TILES 64
#define MAX_CACHED_MAP_TILES 48
#define MAX_EASTER_HIDDEN_CONTROLS 128

typedef struct RouteSummary {
	char name[96];
	int train_count;
	int heritage_count;
} RouteSummary;

typedef struct NearestMatch {
	int train_index;
	double distance;
} NearestMatch;

typedef struct AlertsRequest {
	char route_id[32];
} AlertsRequest;

typedef struct AlertsResult {
	char route_id[32];
	char error[256];
	int count;
	RouteAlert alerts[MAX_ROUTE_ALERTS];
} AlertsResult;

typedef struct DetailsRequest {
	Train train;
	int full_view;
} DetailsRequest;

typedef struct DetailsResult {
	char vehicle_id[64];
	int full_view;
	int stop_count;
	TrainStopPrediction stops[MAX_UPCOMING_STOPS];
	char text[8192];
} DetailsResult;

typedef struct StationAlert {
	char vehicle_id[64];
	char station[96];
	double latitude;
	double longitude;
	int notified;
} StationAlert;

typedef struct DelaySample {
	char vehicle_id[64];
	char timestamp[32];
	double delay_minutes;
} DelaySample;

typedef struct WaveFileHeader {
	char riff[4];
	DWORD file_size;
	char wave[4];
	char format_chunk[4];
	DWORD format_size;
	WORD format;
	WORD channels;
	DWORD sample_rate;
	DWORD byte_rate;
	WORD block_align;
	WORD bits_per_sample;
	char data_chunk[4];
	DWORD data_size;
} WaveFileHeader;

typedef struct RefreshResult {
	int count;
	char error[256];
	Train trains[MAX_TRAINS];
} RefreshResult;

typedef struct MapDataRequest {
	int zoom;
	int tile_count;
	int tile_x[MAX_MAP_TILES];
	int tile_y[MAX_MAP_TILES];
	int refresh_shapes;
} MapDataRequest;

typedef struct MapDataResult {
	int shape_count;
	int tiles_loaded;
	char error[256];
	MapRouteLine lines[MAX_MAP_ROUTE_LINES];
} MapDataResult;

typedef void GpImage;
typedef void GpGraphics;

typedef struct CachedMapTile {
	int zoom;
	int tile_x;
	int tile_y;
	ULONGLONG last_used;
	GpImage* image;
} CachedMapTile;

typedef struct GdiplusStartupInput {
	UINT32 version;
	void* debug_callback;
	BOOL suppress_background_thread;
	BOOL suppress_external_codecs;
} GdiplusStartupInput;

typedef INT (WINAPI *GdiplusStartupProc)(ULONG_PTR*, const GdiplusStartupInput*, void*);
typedef void (WINAPI *GdiplusShutdownProc)(ULONG_PTR);
typedef INT (WINAPI *GdipLoadImageFromFileProc)(const WCHAR*, GpImage**);
typedef INT (WINAPI *GdipCreateFromHDCProc)(HDC, GpGraphics**);
typedef INT (WINAPI *GdipDrawImageRectIProc)(GpGraphics*, GpImage*, INT, INT, INT, INT);
typedef INT (WINAPI *GdipDeleteGraphicsProc)(GpGraphics*);
typedef INT (WINAPI *GdipDisposeImageProc)(GpImage*);
typedef INT (WINAPI *GdipTranslateWorldTransformProc)(GpGraphics*, FLOAT, FLOAT, INT);
typedef INT (WINAPI *GdipRotateWorldTransformProc)(GpGraphics*, FLOAT, INT);

static HWND main_window;
static HWND easter_hidden_controls[MAX_EASTER_HIDDEN_CONTROLS];
static int easter_hidden_control_count;
static HWND filter_edit;
static HWND refresh_button;
static HWND train_list;
static HWND details_label;
static HWND details_edit;
static HWND status_text;
static HWND trains_view_button;
static HWND radio_view_button;
static HWND run_button;
static HWND watch_button;
static HWND detail_list;
static HWND stops_list;
static HWND stops_label;
static HWND favorites_view_button;
static HWND saved_search_combo;
static HWND save_search_button;
static HWND favorite_button;
static HWND selected_alerts_button;
static HWND notify_button;
static HWND nearest_button;
static HWND distance_button;
static HWND map_view_button;
static HWND export_button;
static HWND delayed_filter_button;
static HWND map_zoom_in_button;
static HWND map_zoom_out_button;
static HWND map_overview_edit;
static HWND map_overview_label;
static HWND map_full_details_button;
static Train trains[MAX_TRAINS];
static TrainStopPrediction displayed_stops[MAX_UPCOMING_STOPS];
static int displayed_stop_count;
static RadioChannel radio_channels[MAX_RADIO_CHANNELS];
static RouteSummary route_summaries[64];
static NearestMatch nearest_matches[10];
static RouteAlert route_alerts[MAX_ROUTE_ALERTS];
static char favorite_queries[MAX_FAVORITES][96];
static char saved_searches[MAX_SAVED_SEARCHES][160];
static StationAlert station_alerts[MAX_STATION_ALERTS];
static DelaySample delay_samples[MAX_DELAY_SAMPLES];
static char settings_path[MAX_PATH];
static char delay_history_path[MAX_PATH];
static char map_cache_directory[MAX_PATH];
static MapRouteLine map_route_lines[MAX_MAP_ROUTE_LINES];
static CachedMapTile map_tile_cache[MAX_CACHED_MAP_TILES];
static ULONGLONG map_tile_cache_clock;
static int train_count;
static int radio_count;
static int route_count;
static int nearest_count;
static int alert_count;
static int favorite_count;
static int saved_search_count;
static int station_alert_count;
static int delay_sample_count;
static int view_mode;
static int list_column_count;
static int sort_column;
static int sort_ascending = 1;
static int stop_sort_column;
static int stop_sort_ascending = 1;
static int delayed_only;
static int map_route_line_count;
static int map_shapes_loaded;
static int map_shapes_attempted;
static int map_data_dirty = 1;
static int map_selected_train_index = -1;
static HDC map_backbuffer_dc;
static HBITMAP map_backbuffer_bitmap;
static HGDIOBJ map_backbuffer_original_bitmap;
static int map_backbuffer_width;
static int map_backbuffer_height;
static int map_zoom = 8;
static int map_dragging;
static int map_drag_moved;
static int map_drag_start_x;
static int map_drag_start_y;
static double map_center_latitude = 42.35;
static double map_center_longitude = -71.5;
static double map_drag_start_center_x;
static double map_drag_start_center_y;
static int watch_enabled = 1;
static char nearest_location[96];
static double nearest_latitude;
static double nearest_longitude;
static char alert_route_id[32];
static char selected_details_vehicle[64];
static volatile LONG refresh_in_progress;
static volatile LONG alerts_in_progress;
static volatile LONG details_in_progress;
static volatile LONG map_data_in_progress;
static NOTIFYICONDATAA tray_icon;
static int tray_icon_added;
static HWND title_control;
static HWND subtitle_control;
static HFONT title_font;
static HFONT subtitle_font;
static HFONT easter_font;
static HBRUSH workspace_brush;
static HBRUSH white_brush;
static HBRUSH header_brush;
static HBRUSH accent_brush;
static HMODULE gdiplus_module;
static ULONG_PTR gdiplus_token;
static GpImage* logo_image;
static GpImage* easter_logo_image;
static GdipLoadImageFromFileProc gdip_load_image;
static GdipCreateFromHDCProc gdip_create_from_hdc;
static GdipDrawImageRectIProc gdip_draw_image_rect;
static GdipDeleteGraphicsProc gdip_delete_graphics;
static GdipDisposeImageProc gdip_dispose_image;
static GdipTranslateWorldTransformProc gdip_translate_world;
static GdipRotateWorldTransformProc gdip_rotate_world;
static GdiplusShutdownProc gdiplus_shutdown;
static RECT easter_original_rect;
static ULONGLONG easter_start_tick;
static FLOAT easter_logo_angle;
static int easter_egg_active;
static int easter_mp3_open;

#define COLOR_BRAND_NAVY RGB(11, 25, 47)
#define COLOR_ACCENT_BLUE RGB(52, 105, 160)
#define COLOR_WORKSPACE RGB(242, 246, 250)
#define COLOR_WHITE RGB(255, 255, 255)
#define COLOR_BORDER RGB(207, 218, 230)

static void begin_train_details(int train_index, int full_view);
static void add_detail_pair(int row, const char* field, const char* value);
static void show_upcoming_stops(const TrainStopPrediction* stops, int count);
static void refresh_visible_list(void);
static void format_eta_cell(const char* timestamp, char* output, size_t output_size);
static void format_delay_cell(const TrainStopPrediction* stop, char* output, size_t output_size);
static void set_view_mode(int mode);
static void show_selected_train(void);
static void begin_map_data_request(HWND window, int width, int height, int refresh_shapes);
static void update_map_overview(int train_index);
static int ensure_map_backbuffer(HDC reference_dc, int width, int height);
static void dispose_map_backbuffer(void);
static DWORD WINAPI map_data_worker(void* parameter);
static int CALLBACK compare_main_rows(LPARAM first_data, LPARAM second_data, LPARAM context);
static int CALLBACK compare_stop_rows(LPARAM first_data, LPARAM second_data, LPARAM context);
static void get_map_rect(int width, int height, RECT* map);

static void draw_easter_train(HDC device_context, int client_width, double progress, int reverse)
{
	HBRUSH car_brush = CreateSolidBrush(RGB(245, 244, 232));
	HBRUSH engine_brush = CreateSolidBrush(RGB(202, 54, 45));
	HBRUSH window_brush = CreateSolidBrush(RGB(87, 174, 205));
	HBRUSH wheel_brush = CreateSolidBrush(RGB(20, 28, 38));
	HBRUSH gold_brush = CreateSolidBrush(RGB(231, 177, 68));
	HPEN track_pen = CreatePen(PS_SOLID, 2, RGB(231, 177, 68));
	int saved_dc = SaveDC(device_context);
	HGDIOBJ previous_pen = SelectObject(device_context, track_pen);
	HGDIOBJ previous_brush;
	int x = -150 + (int)(progress * (client_width + 300));
	int y = 31;
	int tie_x;
	RECT shape;
	for (tie_x = 0; tie_x < client_width; tie_x += 28) {
		shape.left = tie_x;
		shape.top = 66;
		shape.right = tie_x + 4;
		shape.bottom = 71;
		FillRect(device_context, &shape, gold_brush);
	}
	MoveToEx(device_context, 0, 65, NULL);
	LineTo(device_context, client_width, 65);
	MoveToEx(device_context, 0, 70, NULL);
	LineTo(device_context, client_width, 70);
	if (reverse && saved_dc != 0) {
		XFORM mirror_transform;
		SetGraphicsMode(device_context, GM_ADVANCED);
		mirror_transform.eM11 = -1.0f;
		mirror_transform.eM12 = 0.0f;
		mirror_transform.eM21 = 0.0f;
		mirror_transform.eM22 = 1.0f;
		mirror_transform.eDx = (FLOAT)(2 * x + 148);
		mirror_transform.eDy = 0.0f;
		SetWorldTransform(device_context, &mirror_transform);
	}
	shape.left = x;
	shape.top = y + 7;
	shape.right = x + 41;
	shape.bottom = y + 25;
	FillRect(device_context, &shape, car_brush);
	shape.left = x + 45;
	shape.right = x + 86;
	FillRect(device_context, &shape, car_brush);
	shape.left = x + 7;
	shape.top = y + 10;
	shape.right = x + 17;
	shape.bottom = y + 18;
	FillRect(device_context, &shape, window_brush);
	shape.left = x + 22;
	shape.right = x + 32;
	FillRect(device_context, &shape, window_brush);
	shape.left = x + 52;
	shape.right = x + 62;
	FillRect(device_context, &shape, window_brush);
	shape.left = x + 67;
	shape.right = x + 77;
	FillRect(device_context, &shape, window_brush);
	shape.left = x + 4;
	shape.top = y + 23;
	shape.right = x + 42;
	shape.bottom = y + 26;
	FillRect(device_context, &shape, gold_brush);
	shape.left = x + 49;
	shape.right = x + 87;
	FillRect(device_context, &shape, gold_brush);
	shape.left = x + 90;
	shape.top = y + 10;
	shape.right = x + 140;
	shape.bottom = y + 27;
	FillRect(device_context, &shape, engine_brush);
	shape.left = x + 108;
	shape.top = y + 3;
	shape.right = x + 132;
	shape.bottom = y + 12;
	FillRect(device_context, &shape, engine_brush);
	shape.left = x + 114;
	shape.top = y + 5;
	shape.right = x + 124;
	shape.bottom = y + 10;
	FillRect(device_context, &shape, window_brush);
	shape.left = x + 96;
	shape.top = y + 2;
	shape.right = x + 102;
	shape.bottom = y + 10;
	FillRect(device_context, &shape, gold_brush);
	shape.left = x + 138;
	shape.top = y + 17;
	shape.right = x + 148;
	shape.bottom = y + 24;
	FillRect(device_context, &shape, gold_brush);
	previous_brush = SelectObject(device_context, wheel_brush);
	SelectObject(device_context, GetStockObject(NULL_PEN));
	Ellipse(device_context, x + 7, y + 22, x + 19, y + 34);
	Ellipse(device_context, x + 28, y + 22, x + 40, y + 34);
	Ellipse(device_context, x + 52, y + 22, x + 64, y + 34);
	Ellipse(device_context, x + 73, y + 22, x + 85, y + 34);
	Ellipse(device_context, x + 96, y + 23, x + 110, y + 37);
	Ellipse(device_context, x + 121, y + 23, x + 135, y + 37);
	if (saved_dc != 0) RestoreDC(device_context, saved_dc);
	else {
		SelectObject(device_context, previous_brush);
		SelectObject(device_context, previous_pen);
	}
	DeleteObject(track_pen);
	DeleteObject(car_brush);
	DeleteObject(engine_brush);
	DeleteObject(window_brush);
	DeleteObject(wheel_brush);
	DeleteObject(gold_brush);
}

static void draw_easter_word(HDC device_context, int client_width, int client_height,
	ULONGLONG elapsed, int copy_count)
{
	static const char word[] = "AUTISTIC";
	static const COLORREF colors[] = {
		RGB(255, 96, 105), RGB(255, 176, 72), RGB(255, 226, 92), RGB(113, 224, 142),
		RGB(91, 205, 235), RGB(145, 145, 255), RGB(238, 126, 206), RGB(80, 230, 255)
	};
	int letter_widths[sizeof(word) - 1];
	int word_width = 0;
	int word_height = 0;
	int saved_dc;
	double angle;
	double cosine;
	double sine;
	double scale;
	double rotated_width;
	double rotated_height;
	double half_width;
	double half_height;
	double horizontal_bounce;
	double vertical_bounce;
	ULONGLONG copy_elapsed;
	XFORM transform;
	int index;
	int text_x;
	int copy_index;
	if (easter_font == NULL || client_width <= 0 || client_height <= 0) return;
	saved_dc = SaveDC(device_context);
	if (saved_dc == 0) return;
	if (SelectObject(device_context, easter_font) == NULL) {
		RestoreDC(device_context, saved_dc);
		return;
	}
	for (index = 0; index < (int)(sizeof(word) - 1); ++index) {
		char letter[2] = { word[index], '\0' };
		SIZE letter_size;
		if (!GetTextExtentPoint32A(device_context, letter, 1, &letter_size)) {
			RestoreDC(device_context, saved_dc);
			return;
		}
		letter_widths[index] = letter_size.cx;
		word_width += letter_size.cx;
		if (letter_size.cy > word_height) word_height = letter_size.cy;
	}
	if (copy_count > 32) copy_count = 32;
	if (copy_count < 1) copy_count = 1;
	if (SetGraphicsMode(device_context, GM_ADVANCED) != 0) {
		SetBkMode(device_context, TRANSPARENT);
		for (copy_index = 0; copy_index < copy_count; ++copy_index) {
			copy_elapsed = elapsed + (ULONGLONG)copy_index * 430;
			angle = (double)copy_elapsed * 0.003 + copy_index * 0.41;
			cosine = cos(angle);
			sine = sin(angle);
			scale = 1.05 + 0.5 * sin((double)copy_elapsed * 0.004);
			rotated_width = fabs(cosine) * word_width + fabs(sine) * word_height;
			rotated_height = fabs(sine) * word_width + fabs(cosine) * word_height;
			if (rotated_width * scale > client_width - 8) scale = (client_width - 8) / rotated_width;
			if (rotated_height * scale > client_height - 8) scale = (client_height - 8) / rotated_height;
			if (scale < 0.1) scale = 0.1;
			half_width = rotated_width * scale / 2.0;
			half_height = rotated_height * scale / 2.0;
			horizontal_bounce = fmod((double)copy_elapsed / 3000.0 + copy_index * 0.23, 2.0);
			vertical_bounce = fmod((double)copy_elapsed / 2100.0 + 0.31 + copy_index * 0.37, 2.0);
			if (horizontal_bounce > 1.0) horizontal_bounce = 2.0 - horizontal_bounce;
			if (vertical_bounce > 1.0) vertical_bounce = 2.0 - vertical_bounce;
			transform.eM11 = (FLOAT)(scale * cosine);
			transform.eM12 = (FLOAT)(scale * sine);
			transform.eM21 = (FLOAT)(-scale * sine);
			transform.eM22 = (FLOAT)(scale * cosine);
			transform.eDx = (FLOAT)(half_width + horizontal_bounce * (client_width - 2.0 * half_width));
			transform.eDy = (FLOAT)(half_height + vertical_bounce * (client_height - 2.0 * half_height));
			if (!SetWorldTransform(device_context, &transform)) continue;
			text_x = -word_width / 2;
			for (index = 0; index < (int)(sizeof(word) - 1); ++index) {
				SetTextColor(device_context, colors[index]);
				TextOutA(device_context, text_x, -word_height / 2, &word[index], 1);
				text_x += letter_widths[index];
			}
		}
	}
	RestoreDC(device_context, saved_dc);
}

static void load_delay_history(void)
{
	FILE* file;
	char line[160];
	if (delay_history_path[0] == '\0' || fopen_s(&file, delay_history_path, "r") != 0) return;
	while (delay_sample_count < MAX_DELAY_SAMPLES && fgets(line, sizeof(line), file) != NULL) {
		char* first_comma = strchr(line, ',');
		char* second_comma;
		DelaySample* sample;
		if (first_comma == NULL) continue;
		second_comma = strchr(first_comma + 1, ',');
		if (second_comma == NULL) continue;
		*first_comma = '\0';
		*second_comma = '\0';
		sample = &delay_samples[delay_sample_count++];
		snprintf(sample->vehicle_id, sizeof(sample->vehicle_id), "%s", line);
		snprintf(sample->timestamp, sizeof(sample->timestamp), "%s", first_comma + 1);
		sample->delay_minutes = atof(second_comma + 1);
	}
	fclose(file);
}

static void record_delay_sample(const char* vehicle_id, double delay_minutes, char* trend, size_t trend_size)
{
	FILE* file;
	SYSTEMTIME now;
	char timestamp[32];
	char previous_vehicle[64] = "";
	double previous_delay = 0.0;
	int index;
	for (index = delay_sample_count - 1; index >= 0; --index) {
		if (strcmp(delay_samples[index].vehicle_id, vehicle_id) == 0) {
			snprintf(previous_vehicle, sizeof(previous_vehicle), "%s", vehicle_id);
			previous_delay = delay_samples[index].delay_minutes;
			break;
		}
	}
	GetLocalTime(&now);
	snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
		now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
	if (previous_vehicle[0] == '\0') snprintf(trend, trend_size, "First sample: %.0f min late", delay_minutes);
	else if (delay_minutes < previous_delay - 0.5) {
		snprintf(trend, trend_size, "Improving by %.0f min", previous_delay - delay_minutes);
	} else if (delay_minutes > previous_delay + 0.5) {
		snprintf(trend, trend_size, "Worsening by %.0f min", delay_minutes - previous_delay);
	} else {
		snprintf(trend, trend_size, "Steady");
	}
	if (delay_sample_count == MAX_DELAY_SAMPLES) {
		memmove(delay_samples, delay_samples + 1, sizeof(delay_samples[0]) * (MAX_DELAY_SAMPLES - 1));
		--delay_sample_count;
	}
	snprintf(delay_samples[delay_sample_count].vehicle_id,
		sizeof(delay_samples[delay_sample_count].vehicle_id), "%s", vehicle_id);
	snprintf(delay_samples[delay_sample_count].timestamp,
		sizeof(delay_samples[delay_sample_count].timestamp), "%s", timestamp);
	delay_samples[delay_sample_count].delay_minutes = delay_minutes;
	++delay_sample_count;
	if (delay_history_path[0] != '\0' && fopen_s(&file, delay_history_path, "a") == 0) {
		fprintf(file, "%s,%s,%.2f\n", vehicle_id, timestamp, delay_minutes);
		fclose(file);
	}
}

static int latest_delay_for(const char* vehicle_id, double* delay_minutes)
{
	int index;
	for (index = delay_sample_count - 1; index >= 0; --index) {
		if (strcmp(delay_samples[index].vehicle_id, vehicle_id) == 0) {
			*delay_minutes = delay_samples[index].delay_minutes;
			return 1;
		}
	}
	return 0;
}

static void initialize_tray_icon(HWND window)
{
	memset(&tray_icon, 0, sizeof(tray_icon));
	tray_icon.cbSize = sizeof(tray_icon);
	tray_icon.hWnd = window;
	tray_icon.uID = 1;
	tray_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	tray_icon.uCallbackMessage = WM_TRAY_CALLBACK;
	tray_icon.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	snprintf(tray_icon.szTip, sizeof(tray_icon.szTip), "MBTA Train Tracker");
	tray_icon_added = Shell_NotifyIconA(NIM_ADD, &tray_icon) != FALSE;
}

static void show_tray_notification(const char* title, const char* message)
{
	if (!tray_icon_added) return;
	tray_icon.uFlags = NIF_INFO;
	tray_icon.dwInfoFlags = NIIF_INFO;
	tray_icon.uTimeout = 10000;
	snprintf(tray_icon.szInfoTitle, sizeof(tray_icon.szInfoTitle), "%s", title);
	snprintf(tray_icon.szInfo, sizeof(tray_icon.szInfo), "%s", message);
	Shell_NotifyIconA(NIM_MODIFY, &tray_icon);
}

static void remove_tray_icon(void)
{
	if (tray_icon_added) Shell_NotifyIconA(NIM_DELETE, &tray_icon);
	tray_icon_added = 0;
}

static void initialize_settings(void)
{
	char appdata[MAX_PATH];
	char directory[MAX_PATH];
	DWORD length = GetEnvironmentVariableA("APPDATA", appdata, (DWORD)sizeof(appdata));
	int index;
	if (length == 0 || length >= sizeof(appdata)) return;
	snprintf(directory, sizeof(directory), "%s\\MBTATracker", appdata);
	if (!CreateDirectoryA(directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return;
	snprintf(settings_path, sizeof(settings_path), "%s\\settings.ini", directory);
	snprintf(delay_history_path, sizeof(delay_history_path), "%s\\delay-history.csv", directory);
	snprintf(map_cache_directory, sizeof(map_cache_directory), "%s\\map-tiles", directory);
	if (!CreateDirectoryA(map_cache_directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
		map_cache_directory[0] = '\0';
	}
	favorite_count = GetPrivateProfileIntA("Favorites", "Count", 0, settings_path);
	if (favorite_count < 0 || favorite_count > MAX_FAVORITES) favorite_count = 0;
	for (index = 0; index < favorite_count; ++index) {
		char key[32];
		snprintf(key, sizeof(key), "Item%d", index);
		GetPrivateProfileStringA("Favorites", key, "", favorite_queries[index],
			(DWORD)sizeof(favorite_queries[index]), settings_path);
	}
	saved_search_count = GetPrivateProfileIntA("SavedSearches", "Count", 0, settings_path);
	if (saved_search_count < 0 || saved_search_count > MAX_SAVED_SEARCHES) saved_search_count = 0;
	for (index = 0; index < saved_search_count; ++index) {
		char key[32];
		snprintf(key, sizeof(key), "Search%d", index);
		GetPrivateProfileStringA("SavedSearches", key, "", saved_searches[index],
			(DWORD)sizeof(saved_searches[index]), settings_path);
	}
	station_alert_count = GetPrivateProfileIntA("StationAlerts", "Count", 0, settings_path);
	if (station_alert_count < 0 || station_alert_count > MAX_STATION_ALERTS) station_alert_count = 0;
	for (index = 0; index < station_alert_count; ++index) {
		char key[40];
		char value[48];
		snprintf(key, sizeof(key), "Alert%dTrain", index);
		GetPrivateProfileStringA("StationAlerts", key, "", station_alerts[index].vehicle_id,
			(DWORD)sizeof(station_alerts[index].vehicle_id), settings_path);
		snprintf(key, sizeof(key), "Alert%dStation", index);
		GetPrivateProfileStringA("StationAlerts", key, "", station_alerts[index].station,
			(DWORD)sizeof(station_alerts[index].station), settings_path);
		snprintf(key, sizeof(key), "Alert%dLatitude", index);
		GetPrivateProfileStringA("StationAlerts", key, "0", value, (DWORD)sizeof(value), settings_path);
		station_alerts[index].latitude = atof(value);
		snprintf(key, sizeof(key), "Alert%dLongitude", index);
		GetPrivateProfileStringA("StationAlerts", key, "0", value, (DWORD)sizeof(value), settings_path);
		station_alerts[index].longitude = atof(value);
		snprintf(key, sizeof(key), "Alert%dNotified", index);
		station_alerts[index].notified = GetPrivateProfileIntA("StationAlerts", key, 0, settings_path) != 0;
	}
	load_delay_history();
}

static void save_settings(void)
{
	int index;
	char key[40];
	char value[96];
	if (settings_path[0] == '\0') return;
	snprintf(value, sizeof(value), "%d", favorite_count);
	WritePrivateProfileStringA("Favorites", "Count", value, settings_path);
	for (index = 0; index < favorite_count; ++index) {
		snprintf(key, sizeof(key), "Item%d", index);
		WritePrivateProfileStringA("Favorites", key, favorite_queries[index], settings_path);
	}
	snprintf(value, sizeof(value), "%d", saved_search_count);
	WritePrivateProfileStringA("SavedSearches", "Count", value, settings_path);
	for (index = 0; index < saved_search_count; ++index) {
		snprintf(key, sizeof(key), "Search%d", index);
		WritePrivateProfileStringA("SavedSearches", key, saved_searches[index], settings_path);
	}
	snprintf(value, sizeof(value), "%d", station_alert_count);
	WritePrivateProfileStringA("StationAlerts", "Count", value, settings_path);
	for (index = 0; index < station_alert_count; ++index) {
		snprintf(key, sizeof(key), "Alert%dTrain", index);
		WritePrivateProfileStringA("StationAlerts", key, station_alerts[index].vehicle_id, settings_path);
		snprintf(key, sizeof(key), "Alert%dStation", index);
		WritePrivateProfileStringA("StationAlerts", key, station_alerts[index].station, settings_path);
		snprintf(key, sizeof(key), "Alert%dLatitude", index);
		snprintf(value, sizeof(value), "%.7f", station_alerts[index].latitude);
		WritePrivateProfileStringA("StationAlerts", key, value, settings_path);
		snprintf(key, sizeof(key), "Alert%dLongitude", index);
		snprintf(value, sizeof(value), "%.7f", station_alerts[index].longitude);
		WritePrivateProfileStringA("StationAlerts", key, value, settings_path);
		snprintf(key, sizeof(key), "Alert%dNotified", index);
		WritePrivateProfileStringA("StationAlerts", key, station_alerts[index].notified ? "1" : "0", settings_path);
	}
}

static int initialize_logo(void)
{
	GdiplusStartupProc startup;
	GdipLoadImageFromFileProc load_image;
	GdiplusStartupInput input;
	WCHAR path[MAX_PATH];
	WCHAR* separator;
	DWORD path_length;
	ULONG_PTR token = 0;
	INT status;
	gdiplus_module = LoadLibraryW(L"gdiplus.dll");
	if (gdiplus_module == NULL) return 0;
	startup = (GdiplusStartupProc)GetProcAddress(gdiplus_module, "GdiplusStartup");
	gdiplus_shutdown = (GdiplusShutdownProc)GetProcAddress(gdiplus_module, "GdiplusShutdown");
	gdip_create_from_hdc = (GdipCreateFromHDCProc)GetProcAddress(gdiplus_module, "GdipCreateFromHDC");
	gdip_draw_image_rect = (GdipDrawImageRectIProc)GetProcAddress(gdiplus_module, "GdipDrawImageRectI");
	gdip_delete_graphics = (GdipDeleteGraphicsProc)GetProcAddress(gdiplus_module, "GdipDeleteGraphics");
	gdip_dispose_image = (GdipDisposeImageProc)GetProcAddress(gdiplus_module, "GdipDisposeImage");
	gdip_translate_world = (GdipTranslateWorldTransformProc)GetProcAddress(gdiplus_module, "GdipTranslateWorldTransform");
	gdip_rotate_world = (GdipRotateWorldTransformProc)GetProcAddress(gdiplus_module, "GdipRotateWorldTransform");
	load_image = (GdipLoadImageFromFileProc)GetProcAddress(gdiplus_module, "GdipLoadImageFromFile");
	if (startup == NULL || gdiplus_shutdown == NULL || gdip_create_from_hdc == NULL ||
		gdip_draw_image_rect == NULL || gdip_delete_graphics == NULL ||
		gdip_translate_world == NULL || gdip_rotate_world == NULL ||
		gdip_dispose_image == NULL || load_image == NULL) return 0;
	gdip_load_image = load_image;
	memset(&input, 0, sizeof(input));
	input.version = 1;
	if (startup(&token, &input, NULL) != 0) return 0;
	gdiplus_token = token;
	path_length = GetModuleFileNameW(NULL, path, MAX_PATH);
	if (path_length == 0 || path_length >= MAX_PATH) return 0;
	separator = wcsrchr(path, L'\\');
	if (separator == NULL) return 0;
	separator[1] = L'\0';
	if (wcscat_s(path, MAX_PATH, L"Train tracker Logo.png") != 0) return 0;
	status = load_image(path, &logo_image);
	if (status != 0 || logo_image == NULL) return 0;
	separator = wcsrchr(path, L'\\');
	if (separator == NULL) return 1;
	separator[1] = L'\0';
	if (wcscat_s(path, MAX_PATH, L"Biggie Thomas YouTube Thumbnail.jpg") == 0) {
		load_image(path, &easter_logo_image);
	}
	return 1;
}

static void draw_owner_button(const DRAWITEMSTRUCT* item)
{
	RECT bounds = item->rcItem;
	char label[96];
	HBRUSH fill_brush = white_brush;
	HBRUSH frame_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
	COLORREF text_color = COLOR_BRAND_NAVY;
	int is_view = item->CtlID == ID_TRAINS_VIEW || item->CtlID == ID_FAVORITES_VIEW ||
		item->CtlID == ID_RADIO_VIEW || item->CtlID == ID_MAP_VIEW;
	int is_active_view = (item->CtlID == ID_TRAINS_VIEW && view_mode == VIEW_TRAINS) ||
		(item->CtlID == ID_FAVORITES_VIEW && view_mode == VIEW_FAVORITES) ||
		(item->CtlID == ID_RADIO_VIEW && view_mode == VIEW_RADIO) ||
		(item->CtlID == ID_MAP_VIEW && view_mode == VIEW_MAP);
	if (is_view && is_active_view) {
		fill_brush = header_brush;
		frame_brush = accent_brush;
		text_color = COLOR_WHITE;
	} else if (item->CtlID == ID_RUN_COMMAND || item->CtlID == ID_REFRESH || item->CtlID == ID_SAVE_SEARCH ||
		item->CtlID == ID_MAP_ZOOM_IN || item->CtlID == ID_MAP_ZOOM_OUT ||
		(item->CtlID == ID_WATCH && watch_enabled)) {
		fill_brush = accent_brush;
		frame_brush = accent_brush;
		text_color = COLOR_WHITE;
	} else {
		frame_brush = (HBRUSH)GetStockObject(GRAY_BRUSH);
	}
	if (item->itemState & ODS_DISABLED) {
		fill_brush = workspace_brush;
		text_color = RGB(125, 135, 147);
	}
	FillRect(item->hDC, &bounds, fill_brush);
	FrameRect(item->hDC, &bounds, frame_brush);
	GetWindowTextA(item->hwndItem, label, (int)sizeof(label));
	SetBkMode(item->hDC, TRANSPARENT);
	SetTextColor(item->hDC, text_color);
	DrawTextA(item->hDC, label, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &bounds);
}

static void map_geo_to_world(double latitude, double longitude, int zoom, double* world_x, double* world_y)
{
	const double pi = 3.14159265358979323846;
	double size = 256.0 * (double)(1 << zoom);
	double latitude_radians;
	if (latitude > 85.05112878) latitude = 85.05112878;
	if (latitude < -85.05112878) latitude = -85.05112878;
	latitude_radians = latitude * pi / 180.0;
	*world_x = (longitude + 180.0) / 360.0 * size;
	*world_y = (1.0 - log(tan(latitude_radians) + 1.0 / cos(latitude_radians)) / pi) / 2.0 * size;
}

static void map_world_to_geo(double world_x, double world_y, int zoom, double* latitude, double* longitude)
{
	const double pi = 3.14159265358979323846;
	double size = 256.0 * (double)(1 << zoom);
	double mercator_y = pi - 2.0 * pi * world_y / size;
	*longitude = world_x / size * 360.0 - 180.0;
	*latitude = atan(sinh(mercator_y)) * 180.0 / pi;
}

static int map_tile_path(int zoom, int tile_x, int tile_y, char* path, size_t path_size)
{
	int tile_count = 1 << zoom;
	if (map_cache_directory[0] == '\0' || tile_y < 0 || tile_y >= tile_count) return 0;
	tile_x %= tile_count;
	if (tile_x < 0) tile_x += tile_count;
	snprintf(path, path_size, "%s\\%d-%d-%d.png", map_cache_directory, zoom, tile_x, tile_y);
	return 1;
}

static int map_tile_is_cached(const char* path)
{
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	FILETIME now;
	ULARGE_INTEGER modified;
	ULARGE_INTEGER current;
	const ULONGLONG cache_lifetime = 7ULL * 24ULL * 60ULL * 60ULL * 10000000ULL;
	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &file_data)) return 0;
	modified.LowPart = file_data.ftLastWriteTime.dwLowDateTime;
	modified.HighPart = file_data.ftLastWriteTime.dwHighDateTime;
	GetSystemTimeAsFileTime(&now);
	current.LowPart = now.dwLowDateTime;
	current.HighPart = now.dwHighDateTime;
	return current.QuadPart >= modified.QuadPart && current.QuadPart - modified.QuadPart < cache_lifetime;
}

static GpImage* get_decoded_map_tile(int zoom, int tile_x, int tile_y)
{
	char path[MAX_PATH];
	WCHAR wide_path[MAX_PATH];
	GpImage* image = NULL;
	int tile_count = 1 << zoom;
	int normalized_x = (tile_x % tile_count + tile_count) % tile_count;
	int index;
	int replacement = 0;
	ULONGLONG oldest = ~(ULONGLONG)0;
	if (tile_y < 0 || tile_y >= tile_count || gdip_load_image == NULL ||
		!map_tile_path(zoom, normalized_x, tile_y, path, sizeof(path)) ||
		!map_tile_is_cached(path) ||
		MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, MAX_PATH) == 0) return NULL;
	for (index = 0; index < MAX_CACHED_MAP_TILES; ++index) {
		if (map_tile_cache[index].image != NULL && map_tile_cache[index].zoom == zoom &&
			map_tile_cache[index].tile_x == normalized_x && map_tile_cache[index].tile_y == tile_y) {
			map_tile_cache[index].last_used = ++map_tile_cache_clock;
			return map_tile_cache[index].image;
		}
		if (map_tile_cache[index].image == NULL) {
			replacement = index;
			oldest = 0;
		} else if (oldest != 0 && map_tile_cache[index].last_used < oldest) {
			oldest = map_tile_cache[index].last_used;
			replacement = index;
		}
	}
	if (gdip_load_image(wide_path, &image) != 0 || image == NULL) return NULL;
	if (map_tile_cache[replacement].image != NULL) gdip_dispose_image(map_tile_cache[replacement].image);
	map_tile_cache[replacement].zoom = zoom;
	map_tile_cache[replacement].tile_x = normalized_x;
	map_tile_cache[replacement].tile_y = tile_y;
	map_tile_cache[replacement].last_used = ++map_tile_cache_clock;
	map_tile_cache[replacement].image = image;
	return image;
}

static int download_map_tile(int zoom, int tile_x, int tile_y)
{
	char path[MAX_PATH];
	wchar_t request_path[96];
	static const wchar_t request_headers[] = L"User-Agent: MBTATracker/1.2 map viewer\r\nAccept: image/png\r\n";
	HINTERNET session = NULL;
	HINTERNET connection = NULL;
	HINTERNET request = NULL;
	DWORD status_code = 0;
	DWORD status_size = sizeof(status_code);
	FILE* file = NULL;
	int success = 0;
	if (!map_tile_path(zoom, tile_x, tile_y, path, sizeof(path))) return 0;
	if (map_tile_is_cached(path)) return 1;
	DeleteFileA(path);
	if (swprintf_s(request_path, sizeof(request_path) / sizeof(request_path[0]),
		L"/%d/%d/%d.png", zoom, (tile_x % (1 << zoom) + (1 << zoom)) % (1 << zoom), tile_y) < 0) return 0;
	session = WinHttpOpen(L"MBTATracker/1.2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (session == NULL) goto cleanup;
	WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);
	connection = WinHttpConnect(session, L"tile.openstreetmap.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (connection == NULL) goto cleanup;
	request = WinHttpOpenRequest(connection, L"GET", request_path, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (request == NULL || !WinHttpSendRequest(request, request_headers, (DWORD)-1L,
		WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, NULL)) goto cleanup;
	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) || status_code != 200) goto cleanup;
	if (fopen_s(&file, path, "wb") != 0) goto cleanup;
	for (;;) {
		BYTE bytes[8192];
		DWORD bytes_read = 0;
		if (!WinHttpReadData(request, bytes, sizeof(bytes), &bytes_read)) break;
		if (bytes_read == 0) {
			success = 1;
			break;
		}
		if (fwrite(bytes, 1, bytes_read, file) != bytes_read) break;
	}
cleanup:
	if (file != NULL) fclose(file);
	if (!success && path[0] != '\0') DeleteFileA(path);
	if (request != NULL) WinHttpCloseHandle(request);
	if (connection != NULL) WinHttpCloseHandle(connection);
	if (session != NULL) WinHttpCloseHandle(session);
	return success;
}

static DWORD WINAPI map_data_worker(void* parameter)
{
	MapDataRequest* request = (MapDataRequest*)parameter;
	MapDataResult* result = (MapDataResult*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MapDataResult));
	HWND window = main_window;
	int index;
	if (result != NULL) {
		result->shape_count = -2;
		for (index = 0; index < request->tile_count; ++index) {
			if (index > 0) Sleep(500);
			if (download_map_tile(request->zoom, request->tile_x[index], request->tile_y[index])) {
				++result->tiles_loaded;
			}
		}
		if (request->refresh_shapes) {
			result->shape_count = fetch_map_route_lines(result->lines,
				MAX_MAP_ROUTE_LINES, result->error, sizeof(result->error));
		}
	}
	HeapFree(GetProcessHeap(), 0, request);
	if (!PostMessageA(window, WM_MAP_DATA_COMPLETE, 0, (LPARAM)result) && result != NULL) {
		HeapFree(GetProcessHeap(), 0, result);
	}
	return 0;
}

static void begin_map_data_request(HWND window, int width, int height, int refresh_shapes)
{
	MapDataRequest* request;
	HANDLE thread;
	RECT map;
	double center_x;
	double center_y;
	double world_left;
	double world_top;
	int first_tile_x;
	int first_tile_y;
	int last_tile_x;
	int last_tile_y;
	int tile_x;
	int tile_y;
	if (view_mode != VIEW_MAP || InterlockedCompareExchange(&map_data_in_progress, 1, 0) != 0) return;
	request = (MapDataRequest*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MapDataRequest));
	if (request == NULL) {
		InterlockedExchange(&map_data_in_progress, 0);
		return;
	}
	get_map_rect(width, height, &map);
	request->zoom = map_zoom;
	map_geo_to_world(map_center_latitude, map_center_longitude, map_zoom, &center_x, &center_y);
	world_left = center_x - (map.right - map.left) / 2.0;
	world_top = center_y - (map.bottom - map.top) / 2.0;
	first_tile_x = (int)floor(world_left / 256.0);
	first_tile_y = (int)floor(world_top / 256.0);
	last_tile_x = (int)floor((world_left + map.right - map.left) / 256.0);
	last_tile_y = (int)floor((world_top + map.bottom - map.top) / 256.0);
	for (tile_y = first_tile_y; tile_y <= last_tile_y && request->tile_count < MAX_MAP_TILES; ++tile_y) {
		for (tile_x = first_tile_x; tile_x <= last_tile_x && request->tile_count < MAX_MAP_TILES; ++tile_x) {
			char path[MAX_PATH];
			int normalized_x = (tile_x % (1 << map_zoom) + (1 << map_zoom)) % (1 << map_zoom);
			if (map_tile_path(map_zoom, normalized_x, tile_y, path, sizeof(path)) &&
				!map_tile_is_cached(path)) {
				request->tile_x[request->tile_count] = tile_x;
				request->tile_y[request->tile_count] = tile_y;
				++request->tile_count;
			}
		}
	}
	request->refresh_shapes = refresh_shapes && !map_shapes_loaded && !map_shapes_attempted;
	if (request->tile_count == 0 && !request->refresh_shapes) {
		HeapFree(GetProcessHeap(), 0, request);
		InterlockedExchange(&map_data_in_progress, 0);
		return;
	}
	if (request->refresh_shapes) map_shapes_attempted = 1;
	thread = CreateThread(NULL, 0, map_data_worker, request, 0, NULL);
	if (thread == NULL) {
		if (request->refresh_shapes) map_shapes_attempted = 0;
		HeapFree(GetProcessHeap(), 0, request);
		InterlockedExchange(&map_data_in_progress, 0);
		return;
	}
	CloseHandle(thread);
}

static void get_map_rect(int width, int height, RECT* map)
{
	map->left = 38;
	map->top = 178;
	map->right = width - 38;
	map->bottom = height - 54;
}

static POINT project_map_point(const RECT* map, double latitude, double longitude)
{
	POINT point;
	double center_x;
	double center_y;
	double point_x;
	double point_y;
	map_geo_to_world(map_center_latitude, map_center_longitude, map_zoom, &center_x, &center_y);
	map_geo_to_world(latitude, longitude, map_zoom, &point_x, &point_y);
	point.x = map->left + (map->right - map->left) / 2 + (int)(point_x - center_x);
	point.y = map->top + (map->bottom - map->top) / 2 + (int)(point_y - center_y);
	return point;
}

static COLORREF map_route_color(const char* route_id)
{
	static const struct {
		const char* route_id;
		COLORREF color;
	} styles[] = {
		{"CR-Fairmount", RGB(21, 101, 192)},
		{"CR-NewBedford", RGB(198, 72, 43)},
		{"CR-Fitchburg", RGB(0, 126, 105)},
		{"CR-Worcester", RGB(125, 69, 153)},
		{"CR-Franklin", RGB(194, 119, 0)},
		{"CR-Greenbush", RGB(0, 112, 153)},
		{"CR-Haverhill", RGB(176, 57, 108)},
		{"CR-Kingston", RGB(83, 102, 48)},
		{"CR-Lowell", RGB(36, 126, 139)},
		{"CR-Needham", RGB(176, 70, 48)},
		{"CR-Newburyport", RGB(47, 92, 160)},
		{"CR-Providence", RGB(117, 83, 42)},
		{"CR-Foxboro", RGB(72, 117, 89)}
	};
	int index;
	unsigned int hash = 2166136261u;
	for (index = 0; index < sizeof(styles) / sizeof(styles[0]); ++index) {
		if (strcmp(styles[index].route_id, route_id) == 0) return styles[index].color;
	}
	for (index = 0; route_id[index] != '\0'; ++index) hash = (hash ^ (unsigned char)route_id[index]) * 16777619u;
	return RGB(48 + (hash & 0x7f), 58 + ((hash >> 8) & 0x7f), 70 + ((hash >> 16) & 0x7f));
}

static const char* map_route_name(const char* route_id)
{
	int train_index;
	int shape_index;
	for (shape_index = 0; shape_index < map_route_line_count; ++shape_index) {
		if (strcmp(map_route_lines[shape_index].route_id, route_id) == 0 && map_route_lines[shape_index].route_name[0]) {
			return map_route_lines[shape_index].route_name;
		}
	}
	for (train_index = 0; train_index < train_count; ++train_index) {
		if (trains[train_index].source == TRAIN_SOURCE_MBTA &&
			strcmp(trains[train_index].route_id, route_id) == 0 && trains[train_index].route_name[0]) {
			return trains[train_index].route_name;
		}
	}
	return route_id;
}

static void draw_map_view(HDC device_context, int width, int height)
{
	RECT map;
	RECT title;
	RECT legend;
	RECT attribution;
	GpGraphics* graphics = NULL;
	int loaded_tiles = 0;
	HBRUSH land_brush;
	HBRUSH previous_brush;
	COLORREF previous_text;
	double world_center_x;
	double world_center_y;
	double world_left;
	double world_top;
	int first_tile_x;
	int first_tile_y;
	int last_tile_x;
	int last_tile_y;
	int tile_x;
	int tile_y;
	int index;
	static const struct {
		const char* name;
		double latitude;
		double longitude;
	} landmarks[] = {
		{"Boston", 42.3519, -71.0552},
		{"Worcester", 42.2626, -71.7997},
		{"Providence", 41.8240, -71.4128},
		{"Lowell", 42.6334, -71.3162},
		{"Portland", 43.6746, -70.2929},
		{"Springfield", 42.1155, -72.5900}
	};
	get_map_rect(width, height, &map);
	if (map.right - map.left < 260 || map.bottom - map.top < 180) return;
	FillRect(device_context, &map, white_brush);
	FrameRect(device_context, &map, (HBRUSH)GetStockObject(GRAY_BRUSH));
	map_geo_to_world(map_center_latitude, map_center_longitude, map_zoom, &world_center_x, &world_center_y);
	world_left = world_center_x - (map.right - map.left) / 2.0;
	world_top = world_center_y - (map.bottom - map.top) / 2.0;
	first_tile_x = (int)floor(world_left / 256.0);
	first_tile_y = (int)floor(world_top / 256.0);
	last_tile_x = (int)floor((world_left + map.right - map.left) / 256.0);
	last_tile_y = (int)floor((world_top + map.bottom - map.top) / 256.0);
	if (gdip_create_from_hdc != NULL && gdip_load_image != NULL && gdip_draw_image_rect != NULL &&
		gdip_create_from_hdc(device_context, &graphics) != 0) graphics = NULL;
	for (tile_y = first_tile_y; tile_y <= last_tile_y; ++tile_y) {
		for (tile_x = first_tile_x; tile_x <= last_tile_x; ++tile_x) {
			GpImage* tile_image;
			int destination_x = map.left + tile_x * 256 - (int)world_left;
			int destination_y = map.top + tile_y * 256 - (int)world_top;
			tile_image = get_decoded_map_tile(map_zoom, tile_x, tile_y);
			if (graphics != NULL && tile_image != NULL) {
				gdip_draw_image_rect(graphics, tile_image, destination_x, destination_y, 256, 256);
				++loaded_tiles;
			}
		}
	}
	if (graphics != NULL) gdip_delete_graphics(graphics);
	SetBkMode(device_context, TRANSPARENT);
	previous_text = SetTextColor(device_context, RGB(108, 123, 140));
	if (loaded_tiles == 0) {
		static const char offline_message[] = "Map tiles load when online; train markers remain available offline.";
		TextOutA(device_context, map.left + 18, map.top + 18, offline_message, (int)strlen(offline_message));
	}
	for (index = 0; index < map_route_line_count; ++index) {
		MapRouteLine* route_line = &map_route_lines[index];
		HPEN casing_pen = CreatePen(PS_SOLID, 8, COLOR_WHITE);
		HPEN route_pen = CreatePen(PS_SOLID, 4, map_route_color(route_line->route_id));
		HPEN previous_route_pen = (HPEN)SelectObject(device_context, casing_pen);
		int point_index;
		for (point_index = 0; point_index < route_line->point_count; ++point_index) {
			POINT point = project_map_point(&map, route_line->latitude[point_index], route_line->longitude[point_index]);
			if (point_index == 0) MoveToEx(device_context, point.x, point.y, NULL);
			else LineTo(device_context, point.x, point.y);
		}
		SelectObject(device_context, route_pen);
		for (point_index = 0; point_index < route_line->point_count; ++point_index) {
			POINT point = project_map_point(&map, route_line->latitude[point_index], route_line->longitude[point_index]);
			if (point_index == 0) MoveToEx(device_context, point.x, point.y, NULL);
			else LineTo(device_context, point.x, point.y);
		}
		SelectObject(device_context, previous_route_pen);
		DeleteObject(casing_pen);
		DeleteObject(route_pen);
	}
	{
		int unique_indices[MAX_MAP_ROUTE_LINES];
		int unique_count = 0;
		int columns;
		int rows;
		int legend_index;
		RECT legend_box;
		for (index = 0; index < map_route_line_count; ++index) {
			int earlier;
			for (earlier = 0; earlier < index; ++earlier) {
				if (strcmp(map_route_lines[earlier].route_id, map_route_lines[index].route_id) == 0) break;
			}
			if (earlier == index) unique_indices[unique_count++] = index;
		}
		columns = unique_count > 12 ? 3 : 2;
		rows = unique_count == 0 ? 0 : (unique_count + columns - 1) / columns;
		if (unique_count > 0) {
			legend_box.left = map.left + 10;
			legend_box.top = map.top + 10;
			legend_box.right = legend_box.left + columns * 146 + 12;
			legend_box.bottom = legend_box.top + 25 + rows * 18;
			FillRect(device_context, &legend_box, white_brush);
			FrameRect(device_context, &legend_box, (HBRUSH)GetStockObject(GRAY_BRUSH));
			SetTextColor(device_context, COLOR_BRAND_NAVY);
			TextOutA(device_context, legend_box.left + 8, legend_box.top + 4, "MBTA rail lines", 14);
			for (legend_index = 0; legend_index < unique_count; ++legend_index) {
				int row = legend_index % rows;
				int column = legend_index / rows;
				int line_index = unique_indices[legend_index];
				int x = legend_box.left + 8 + column * 146;
				int y = legend_box.top + 24 + row * 18;
				RECT label_rect;
				HPEN sample_pen = CreatePen(PS_SOLID, 4,
					map_route_color(map_route_lines[line_index].route_id));
				HPEN old_pen = (HPEN)SelectObject(device_context, sample_pen);
				MoveToEx(device_context, x, y + 8, NULL);
				LineTo(device_context, x + 20, y + 8);
				SelectObject(device_context, old_pen);
				DeleteObject(sample_pen);
				label_rect.left = x + 26;
				label_rect.top = y;
				label_rect.right = x + 142;
				label_rect.bottom = y + 17;
				DrawTextA(device_context, map_route_name(map_route_lines[line_index].route_id), -1,
					&label_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
			}
		}
	}
	land_brush = CreateSolidBrush(RGB(115, 129, 145));
	previous_brush = (HBRUSH)SelectObject(device_context, land_brush);
	for (index = 0; index < sizeof(landmarks) / sizeof(landmarks[0]); ++index) {
		POINT point = project_map_point(&map, landmarks[index].latitude, landmarks[index].longitude);
		Ellipse(device_context, point.x - 3, point.y - 3, point.x + 3, point.y + 3);
		TextOutA(device_context, point.x + 6, point.y - 9, landmarks[index].name,
			(int)strlen(landmarks[index].name));
	}
	SelectObject(device_context, previous_brush);
	DeleteObject(land_brush);
	for (index = 0; index < train_count; ++index) {
		POINT point;
		HBRUSH marker_brush;
		HBRUSH white_marker;
		HBRUSH previous_marker;
		COLORREF marker_color;
		int selected;
		if (!trains[index].has_location || trains[index].latitude < 40.5 || trains[index].latitude > 44.8 ||
			trains[index].longitude < -74.7 || trains[index].longitude > -69.3) continue;
		point = project_map_point(&map, trains[index].latitude, trains[index].longitude);
		selected = strcmp(selected_details_vehicle, trains[index].vehicle_id) == 0;
		marker_color = trains[index].source == TRAIN_SOURCE_AMTRAK ? RGB(206, 112, 67) :
			map_route_color(trains[index].route_id);
		white_marker = CreateSolidBrush(COLOR_WHITE);
		marker_brush = CreateSolidBrush(marker_color);
		previous_marker = (HBRUSH)SelectObject(device_context, white_marker);
		Ellipse(device_context, point.x - (selected ? 8 : 6), point.y - (selected ? 8 : 6),
			point.x + (selected ? 8 : 6), point.y + (selected ? 8 : 6));
		SelectObject(device_context, marker_brush);
		Ellipse(device_context, point.x - (selected ? 5 : 3), point.y - (selected ? 5 : 3),
			point.x + (selected ? 5 : 3), point.y + (selected ? 5 : 3));
		SelectObject(device_context, previous_marker);
		DeleteObject(white_marker);
		DeleteObject(marker_brush);
		if (selected) {
			char label[112];
			snprintf(label, sizeof(label), "%s  %s", trains[index].car_label, trains[index].destination);
			SetTextColor(device_context, COLOR_BRAND_NAVY);
			TextOutA(device_context, point.x + 10, point.y - 10, label, (int)strlen(label));
		}
	}
	if (map_selected_train_index >= 0) {
		RECT overview_panel;
		overview_panel.left = map.right - 352;
		overview_panel.top = map.top + 8;
		overview_panel.right = map.right - 8;
		overview_panel.bottom = map.top + 170;
		FillRect(device_context, &overview_panel, white_brush);
		FrameRect(device_context, &overview_panel, (HBRUSH)GetStockObject(GRAY_BRUSH));
	}
	SetTextColor(device_context, previous_text);
	title.left = map.left;
	title.top = 145;
	title.right = map.right;
	title.bottom = 171;
	SetTextColor(device_context, COLOR_BRAND_NAVY);
	DrawTextA(device_context, "Live New England train positions", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	legend.left = map.left;
	legend.top = height - 42;
	legend.right = map.right;
	legend.bottom = height - 22;
	SetTextColor(device_context, COLOR_ACCENT_BLUE);
	DrawTextA(device_context, "MBTA", -1, &legend, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	legend.left += 52;
	SetTextColor(device_context, RGB(206, 112, 67));
	DrawTextA(device_context, "Amtrak", -1, &legend, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	attribution.left = map.left;
	attribution.top = height - 60;
	attribution.right = map.right;
	attribution.bottom = height - 43;
	SetTextColor(device_context, RGB(88, 100, 115));
	DrawTextA(device_context, "Map data: OpenStreetMap contributors", -1, &attribution,
		DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
	SetTextColor(device_context, COLOR_BRAND_NAVY);
}

static void dispose_map_backbuffer(void)
{
	if (map_backbuffer_dc != NULL && map_backbuffer_original_bitmap != NULL) {
		SelectObject(map_backbuffer_dc, map_backbuffer_original_bitmap);
	}
	if (map_backbuffer_bitmap != NULL) DeleteObject(map_backbuffer_bitmap);
	if (map_backbuffer_dc != NULL) DeleteDC(map_backbuffer_dc);
	map_backbuffer_dc = NULL;
	map_backbuffer_bitmap = NULL;
	map_backbuffer_original_bitmap = NULL;
	map_backbuffer_width = 0;
	map_backbuffer_height = 0;
}

static int ensure_map_backbuffer(HDC reference_dc, int width, int height)
{
	if (map_backbuffer_dc != NULL && map_backbuffer_width == width && map_backbuffer_height == height) return 1;
	dispose_map_backbuffer();
	map_backbuffer_dc = CreateCompatibleDC(reference_dc);
	if (map_backbuffer_dc == NULL) return 0;
	map_backbuffer_bitmap = CreateCompatibleBitmap(reference_dc, width, height);
	if (map_backbuffer_bitmap == NULL) {
		dispose_map_backbuffer();
		return 0;
	}
	map_backbuffer_original_bitmap = SelectObject(map_backbuffer_dc, map_backbuffer_bitmap);
	if (map_backbuffer_original_bitmap == NULL || map_backbuffer_original_bitmap == HGDI_ERROR) {
		map_backbuffer_original_bitmap = NULL;
		dispose_map_backbuffer();
		return 0;
	}
	map_backbuffer_width = width;
	map_backbuffer_height = height;
	return 1;
}

static void update_map_overview(int train_index)
{
	char summary[640];
	char eta[64];
	char delay[32];
	char updated[64];
	const Train* train;
	if (train_index < 0 || train_index >= train_count) {
		map_selected_train_index = -1;
		SetWindowTextA(map_overview_label, "Select a train marker");
		SetWindowTextA(map_overview_edit, "Train overview will appear here.");
		EnableWindow(map_full_details_button, FALSE);
		SetWindowTextA(status_text, "Map view: click a colored train marker to inspect it.");
		InvalidateRect(main_window, NULL, FALSE);
		return;
	}
	train = &trains[train_index];
	map_selected_train_index = train_index;
	snprintf(selected_details_vehicle, sizeof(selected_details_vehicle), "%s", train->vehicle_id);
	format_eta_cell(train->updated_at, updated, sizeof(updated));
	if (train->upcoming_stop_count > 0) {
		format_eta_cell(train->upcoming_stops[0].arrival, eta, sizeof(eta));
		format_delay_cell(&train->upcoming_stops[0], delay, sizeof(delay));
	} else {
		snprintf(eta, sizeof(eta), "Loading prediction...");
		snprintf(delay, sizeof(delay), "Not reported");
	}
	snprintf(summary, sizeof(summary),
		"%s  |  %s\r\nTo: %s\r\nAt: %s   Status: %s\r\nNext: %s  |  ETA: %s\r\nDelay: %s   Updated: %s",
		train->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA",
		train->car_label[0] ? train->car_label : train->vehicle_id,
		train->destination[0] ? train->destination : "Destination unavailable",
		train->current_stop[0] ? train->current_stop : "Location unavailable",
		train->current_status[0] ? train->current_status : "Status unavailable",
		train->upcoming_stop_count > 0 ? train->upcoming_stops[0].name : "Loading...",
		eta, delay, updated);
	SetWindowTextA(map_overview_label, "Selected train");
	SetWindowTextA(map_overview_edit, summary);
	EnableWindow(map_full_details_button, TRUE);
	SetWindowTextA(status_text, "Selected train. Full predictions are loading in the background.");
	InvalidateRect(main_window, NULL, FALSE);
}

static void select_map_train(int click_x, int click_y, int width, int height)
{
	RECT map;
	int index;
	int selected = -1;
	long nearest_distance = 100;
	map.left = 38;
	map.top = 178;
	map.right = width - 38;
	map.bottom = height - 54;
	for (index = 0; index < train_count; ++index) {
		POINT point;
		long delta_x;
		long delta_y;
		long distance;
		if (!trains[index].has_location || trains[index].latitude < 40.5 || trains[index].latitude > 44.8 ||
			trains[index].longitude < -74.7 || trains[index].longitude > -69.3) continue;
		point = project_map_point(&map, trains[index].latitude, trains[index].longitude);
		delta_x = point.x - click_x;
		delta_y = point.y - click_y;
		distance = delta_x * delta_x + delta_y * delta_y;
		if (distance < nearest_distance) {
			nearest_distance = distance;
			selected = index;
		}
	}
	if (selected >= 0) {
		update_map_overview(selected);
		begin_train_details(selected, 0);
	}
}

static void set_control_font(HWND control)
{
	SendMessageA(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

static int is_train_search_filler(const char* word)
{
	static const char* filler_words[] = {
		"list", "all", "active", "trains", "train", "locomotive", "locomotives",
		"show", "find", "search", "for", "of", "the", "currently", "running", "please"
	};
	size_t index;
	for (index = 0; index < sizeof(filler_words) / sizeof(filler_words[0]); ++index) {
		if (_stricmp(word, filler_words[index]) == 0) return 1;
	}
	return 0;
}

static int train_matches_description(const Train* train, const char* filter)
{
	char buffer[256];
	char* context = NULL;
	char* keyword;
	int keyword_count = 0;
	snprintf(buffer, sizeof(buffer), "%s", filter);
	keyword = strtok_s(buffer, " \t", &context);
	while (keyword != NULL) {
		if (!is_train_search_filler(keyword)) {
			if (!train_matches_keyword(train, keyword)) return 0;
			++keyword_count;
		}
		keyword = strtok_s(NULL, " \t", &context);
	}
	return keyword_count > 0 || filter[0] == '\0';
}

static int parse_gui_location(const char* text, double* latitude, double* longitude)
{
	char buffer[96];
	char* comma;
	if (text == NULL || text[0] == '\0') return 0;
	snprintf(buffer, sizeof(buffer), "%s", text);
	comma = strchr(buffer, ',');
	if (comma != NULL) {
		*comma = '\0';
		*latitude = atof(buffer);
		*longitude = atof(comma + 1);
		return 1;
	}
	return find_station(text, latitude, longitude);
}

static void build_route_summaries(void)
{
	int index;
	route_count = 0;
	for (index = 0; index < train_count; ++index) {
		const char* name = trains[index].route_name[0] ? trains[index].route_name : trains[index].route_id;
		int route_index;
		for (route_index = 0; route_index < route_count; ++route_index) {
			if (strcmp(route_summaries[route_index].name, name) == 0) break;
		}
		if (route_index == route_count && route_count < 64) {
			snprintf(route_summaries[route_count].name, sizeof(route_summaries[route_count].name), "%s", name);
			route_summaries[route_count].train_count = 0;
			route_summaries[route_count].heritage_count = 0;
			++route_count;
		}
		if (route_index < route_count) {
			++route_summaries[route_index].train_count;
			if (train_matches_keyword(&trains[index], "heritage")) {
				++route_summaries[route_index].heritage_count;
			}
		}
	}
}

static void build_nearest_matches(double latitude, double longitude)
{
	int index;
	nearest_count = 0;
	for (index = 0; index < train_count; ++index) {
		NearestMatch match;
		int position;
		if (!trains[index].has_location) continue;
		match.train_index = index;
		match.distance = haversine_miles(latitude, longitude, trains[index].latitude, trains[index].longitude);
		if (nearest_count == 10 && match.distance >= nearest_matches[nearest_count - 1].distance) continue;
		position = nearest_count < 10 ? nearest_count++ : 9;
		while (position > 0 && nearest_matches[position - 1].distance > match.distance) {
			nearest_matches[position] = nearest_matches[position - 1];
			--position;
		}
		nearest_matches[position] = match;
	}
}

static int find_train_index(const char* identifier)
{
	int index;
	for (index = 0; index < train_count; ++index) {
		if (strcmp(identifier, trains[index].vehicle_id) == 0 ||
			strcmp(identifier, trains[index].car_label) == 0) return index;
	}
	return -1;
}

static int is_favorite_train(const Train* train)
{
	int index;
	for (index = 0; index < favorite_count; ++index) {
		if (strcmp(favorite_queries[index], train->vehicle_id) == 0 ||
			strcmp(favorite_queries[index], train->car_label) == 0 ||
			train_matches_description(train, favorite_queries[index])) return 1;
	}
	return 0;
}

static int is_favorite_id(const char* vehicle_id)
{
	int index;
	for (index = 0; index < favorite_count; ++index) {
		if (strcmp(favorite_queries[index], vehicle_id) == 0) return 1;
	}
	return 0;
}

static int selected_train_index(void)
{
	int selected = ListView_GetNextItem(train_list, -1, LVNI_SELECTED);
	LVITEMA item;
	if (selected < 0) return -1;
	memset(&item, 0, sizeof(item));
	item.mask = LVIF_PARAM;
	item.iItem = selected;
	if (!ListView_GetItem(train_list, &item)) return -1;
	return (int)item.lParam;
}

static void update_favorite_button(void)
{
	int train_index = selected_train_index();
	int alert_index;
	if (train_index < 0) {
		SetWindowTextA(favorite_button, "Favorite");
		SetWindowTextA(notify_button, "Notify");
		EnableWindow(favorite_button, FALSE);
		EnableWindow(notify_button, FALSE);
		return;
	}
	EnableWindow(favorite_button, TRUE);
	EnableWindow(notify_button, TRUE);
	SetWindowTextA(favorite_button, is_favorite_id(trains[train_index].vehicle_id) ? "Unfavorite" : "Favorite");
	for (alert_index = 0; alert_index < station_alert_count; ++alert_index) {
		if (strcmp(station_alerts[alert_index].vehicle_id, trains[train_index].vehicle_id) == 0) break;
	}
	SetWindowTextA(notify_button, alert_index < station_alert_count ? "Unnotify" : "Notify");
}

static int save_favorite_query(const char* query)
{
	int index;
	if (query == NULL || query[0] == '\0') return 0;
	for (index = 0; index < favorite_count; ++index) {
		if (_stricmp(favorite_queries[index], query) == 0) return 1;
	}
	if (favorite_count >= MAX_FAVORITES) return 0;
	snprintf(favorite_queries[favorite_count++], sizeof(favorite_queries[0]), "%s", query);
	save_settings();
	return 1;
}

static void toggle_selected_favorite(void)
{
	int train_index = selected_train_index();
	int index;
	if (train_index < 0) return;
	for (index = 0; index < favorite_count; ++index) {
		if (strcmp(favorite_queries[index], trains[train_index].vehicle_id) == 0) {
			memmove(&favorite_queries[index], &favorite_queries[index + 1],
				(size_t)(favorite_count - index - 1) * sizeof(favorite_queries[0]));
			--favorite_count;
			save_settings();
			update_favorite_button();
			if (view_mode == VIEW_FAVORITES) refresh_visible_list();
			SetWindowTextA(status_text, "Removed from favorites.");
			return;
		}
	}
	if (save_favorite_query(trains[train_index].vehicle_id)) {
		update_favorite_button();
		SetWindowTextA(status_text, "Added to favorites.");
	} else {
		SetWindowTextA(status_text, "Favorites are full.");
	}
}

static void refresh_saved_search_combo(void)
{
	int index;
	SendMessageA(saved_search_combo, CB_RESETCONTENT, 0, 0);
	SendMessageA(saved_search_combo, CB_ADDSTRING, 0, (LPARAM)"Saved searches");
	for (index = 0; index < saved_search_count; ++index) {
		SendMessageA(saved_search_combo, CB_ADDSTRING, 0, (LPARAM)saved_searches[index]);
	}
	SendMessageA(saved_search_combo, CB_SETCURSEL, 0, 0);
}

static void save_current_search(void)
{
	char query[160];
	int index;
	GetWindowTextA(filter_edit, query, (int)sizeof(query));
	if (query[0] == '\0') {
		SetWindowTextA(status_text, "Enter a train search before saving it.");
		return;
	}
	for (index = 0; index < saved_search_count; ++index) {
		if (_stricmp(saved_searches[index], query) == 0) {
			SendMessageA(saved_search_combo, CB_SETCURSEL, index + 1, 0);
			SetWindowTextA(status_text, "That search is already saved.");
			return;
		}
	}
	if (saved_search_count >= MAX_SAVED_SEARCHES) {
		SetWindowTextA(status_text, "Saved search list is full.");
		return;
	}
	snprintf(saved_searches[saved_search_count++], sizeof(saved_searches[0]), "%s", query);
	save_settings();
	refresh_saved_search_combo();
	SendMessageA(saved_search_combo, CB_SETCURSEL, saved_search_count, 0);
	SetWindowTextA(status_text, "Search saved.");
}

static int save_station_alert(const char* identifier, const char* station)
{
	double latitude;
	double longitude;
	int train_index = find_train_index(identifier);
	int index;
	if (train_index < 0) {
		SetWindowTextA(status_text, "No active train matches that ID.");
		return 0;
	}
	if (!parse_gui_location(station, &latitude, &longitude)) {
		SetWindowTextA(status_text, "Could not find that station. Try a listed city/station or latitude,longitude.");
		return 0;
	}
	for (index = 0; index < station_alert_count; ++index) {
		if (strcmp(station_alerts[index].vehicle_id, trains[train_index].vehicle_id) == 0) break;
	}
	if (index == station_alert_count) {
		if (station_alert_count >= MAX_STATION_ALERTS) {
			SetWindowTextA(status_text, "Station alert list is full.");
			return 0;
		}
		++station_alert_count;
	}
	snprintf(station_alerts[index].vehicle_id, sizeof(station_alerts[index].vehicle_id), "%s", trains[train_index].vehicle_id);
	snprintf(station_alerts[index].station, sizeof(station_alerts[index].station), "%s", station);
	station_alerts[index].latitude = latitude;
	station_alerts[index].longitude = longitude;
	station_alerts[index].notified = 0;
	save_settings();
	SetWindowTextA(status_text, "Station alert saved. You will be notified within about 2 miles.");
	return 1;
}

static void remove_station_alert(const char* identifier)
{
	int index = 0;
	int removed = 0;
	while (index < station_alert_count) {
		if (strcmp(station_alerts[index].vehicle_id, identifier) == 0) {
			memmove(&station_alerts[index], &station_alerts[index + 1],
				(size_t)(station_alert_count - index - 1) * sizeof(station_alerts[0]));
			--station_alert_count;
			removed = 1;
		} else {
			++index;
		}
	}
	save_settings();
	SetWindowTextA(status_text, removed ? "Station alert removed." : "No station alert was set for that train.");
}

static void check_station_alerts(void)
{
	int index;
	int triggered = 0;
	char message[192] = "";
	for (index = 0; index < station_alert_count; ++index) {
		int train_index;
		if (station_alerts[index].notified) continue;
		train_index = find_train_index(station_alerts[index].vehicle_id);
		if (train_index < 0 || !trains[train_index].has_location) continue;
		if (haversine_miles(trains[train_index].latitude, trains[train_index].longitude,
			station_alerts[index].latitude, station_alerts[index].longitude) <= 2.0) {
			station_alerts[index].notified = 1;
		snprintf(message, sizeof(message), "Train %s is approaching %s.",
				trains[train_index].car_label[0] ? trains[train_index].car_label : trains[train_index].vehicle_id,
				station_alerts[index].station);
			++triggered;
		}
	}
	if (triggered > 0) {
		FLASHWINFO flash;
		save_settings();
		show_tray_notification("Train approaching", message);
		MessageBeep(MB_ICONEXCLAMATION);
		memset(&flash, 0, sizeof(flash));
		flash.cbSize = sizeof(flash);
		flash.hwnd = main_window;
		flash.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
		flash.uCount = 4;
		FlashWindowEx(&flash);
		if (triggered > 1) {
			char summary[192];
			snprintf(summary, sizeof(summary), "%d station alerts triggered. %s", triggered, message);
			SetWindowTextA(status_text, summary);
		} else {
			SetWindowTextA(status_text, message);
		}
	}
}

static void set_list_cell(int row, int column, const char* text)
{
	ListView_SetItemText(train_list, row, column, (LPSTR)text);
}

static int CALLBACK compare_main_rows(LPARAM first_data, LPARAM second_data, LPARAM context)
{
	int result = 0;
	(void)context;
	if (view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES) {
		const Train* first = &trains[(int)first_data];
		const Train* second = &trains[(int)second_data];
		switch (sort_column) {
		case 0: result = _stricmp(first->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA",
			second->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA"); break;
		case 1: result = _stricmp(first->car_label, second->car_label); break;
		case 2: result = _stricmp(first->route_name, second->route_name); break;
		case 3: result = _stricmp(first->destination, second->destination); break;
		case 4: result = _stricmp(first->current_stop, second->current_stop); break;
		case 5: result = _stricmp(first->current_status, second->current_status); break;
		case 6:
			if (first->has_speed != second->has_speed) result = first->has_speed ? -1 : 1;
			else result = first->speed_mph < second->speed_mph ? -1 : first->speed_mph > second->speed_mph ? 1 : 0;
			break;
		case 7: result = is_favorite_train(first) == is_favorite_train(second) ? 0 : is_favorite_train(first) ? -1 : 1; break;
		}
	} else if (view_mode == VIEW_RADIO) {
		const RadioChannel* first = &radio_channels[(int)first_data];
		const RadioChannel* second = &radio_channels[(int)second_data];
		switch (sort_column) {
		case 0: result = _stricmp(first->line, second->line); break;
		case 1: result = _stricmp(first->channel_type, second->channel_type); break;
		case 2: result = _stricmp(first->aar_channel, second->aar_channel); break;
		case 3: result = first->frequency_mhz < second->frequency_mhz ? -1 : first->frequency_mhz > second->frequency_mhz ? 1 : 0; break;
		case 4: result = _stricmp(first->notes, second->notes); break;
		}
	} else if (view_mode == VIEW_ROUTES) {
		const RouteSummary* first = &route_summaries[(int)first_data];
		const RouteSummary* second = &route_summaries[(int)second_data];
		if (sort_column == 0) result = _stricmp(first->name, second->name);
		else if (sort_column == 1) result = first->train_count < second->train_count ? -1 : first->train_count > second->train_count ? 1 : 0;
		else if (sort_column == 2) result = first->heritage_count < second->heritage_count ? -1 : first->heritage_count > second->heritage_count ? 1 : 0;
	} else if (view_mode == VIEW_NEAREST) {
		const NearestMatch* first_match = &nearest_matches[(int)first_data];
		const NearestMatch* second_match = &nearest_matches[(int)second_data];
		const Train* first = &trains[first_match->train_index];
		const Train* second = &trains[second_match->train_index];
		switch (sort_column) {
		case 0: result = _stricmp(first->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA",
			second->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA"); break;
		case 1: result = _stricmp(first->car_label, second->car_label); break;
		case 2: result = _stricmp(first->route_name, second->route_name); break;
		case 3: result = first_match->distance < second_match->distance ? -1 : first_match->distance > second_match->distance ? 1 : 0; break;
		case 4: result = _stricmp(first->destination, second->destination); break;
		}
	} else if (view_mode == VIEW_ALERTS) {
		const RouteAlert* first = &route_alerts[(int)first_data];
		const RouteAlert* second = &route_alerts[(int)second_data];
		result = sort_column == 0 ? _stricmp(first->effect, second->effect) : _stricmp(first->header, second->header);
	}
	return sort_ascending ? result : -result;
}

static int CALLBACK compare_stop_rows(LPARAM first_data, LPARAM second_data, LPARAM context)
{
	const TrainStopPrediction* first = &displayed_stops[(int)first_data];
	const TrainStopPrediction* second = &displayed_stops[(int)second_data];
	int result = 0;
	(void)context;
	switch (stop_sort_column) {
	case 0: result = _stricmp(first->name, second->name); break;
	case 1: result = _stricmp(first->arrival, second->arrival); break;
	case 2: result = _stricmp(first->departure, second->departure); break;
	case 3:
		if (first->has_delay != second->has_delay) result = first->has_delay ? -1 : 1;
		else result = first->delay_minutes < second->delay_minutes ? -1 : first->delay_minutes > second->delay_minutes ? 1 : 0;
		break;
	}
	return stop_sort_ascending ? result : -result;
}

static void refresh_visible_list(void)
{
	char filter[256];
	int index;
	int visible_count = 0;
	GetWindowTextA(filter_edit, filter, (int)sizeof(filter));
	if (view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES) selected_details_vehicle[0] = '\0';
	ListView_DeleteAllItems(train_list);
	{
		char status[128];
		if (view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES) {
			for (index = 0; index < train_count; ++index) {
				LVITEMA item;
				char source[16];
				char speed[24];
				double latest_delay;
				int row;
				if (view_mode == VIEW_FAVORITES && !is_favorite_train(&trains[index])) continue;
				if (delayed_only && (!latest_delay_for(trains[index].vehicle_id, &latest_delay) || latest_delay <= 0.5)) continue;
				if (!train_matches_description(&trains[index], filter)) continue;
				snprintf(source, sizeof(source), "%s", trains[index].source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA");
				snprintf(speed, sizeof(speed), "%s", trains[index].has_speed ? "" : "N/A");
				if (trains[index].has_speed) snprintf(speed, sizeof(speed), "%.0f mph", trains[index].speed_mph);
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_TEXT | LVIF_PARAM;
				item.iItem = visible_count;
				item.lParam = index;
				item.pszText = source;
				row = ListView_InsertItem(train_list, &item);
				set_list_cell(row, 1, trains[index].car_label[0] ? trains[index].car_label : trains[index].vehicle_id);
				set_list_cell(row, 2, trains[index].route_name);
				set_list_cell(row, 3, trains[index].destination);
				set_list_cell(row, 4, trains[index].current_stop);
				set_list_cell(row, 5, trains[index].current_status);
				set_list_cell(row, 6, speed);
				set_list_cell(row, 7, is_favorite_train(&trains[index]) ? "Saved" : "");
				++visible_count;
			}
			if (view_mode == VIEW_FAVORITES) {
				snprintf(status, sizeof(status), "%d favorite%s shown%s", visible_count,
					visible_count == 1 ? "" : "s", delayed_only ? " (known delayed)" : "");
			} else {
				snprintf(status, sizeof(status), "%d train(s) shown, %d active%s", visible_count, train_count,
					delayed_only ? " (known delayed)" : "");
			}
		} else if (view_mode == VIEW_RADIO) {
			radio_count = search_radio_frequencies(filter, radio_channels, MAX_RADIO_CHANNELS);
			for (index = 0; index < radio_count; ++index) {
				LVITEMA item;
				char frequency[24];
				int row;
				snprintf(frequency, sizeof(frequency), "%.4f", radio_channels[index].frequency_mhz);
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_TEXT | LVIF_PARAM;
				item.iItem = visible_count;
				item.lParam = index;
				item.pszText = (LPSTR)radio_channels[index].line;
				row = ListView_InsertItem(train_list, &item);
				set_list_cell(row, 1, radio_channels[index].channel_type);
				set_list_cell(row, 2, radio_channels[index].aar_channel);
				set_list_cell(row, 3, frequency);
				set_list_cell(row, 4, radio_channels[index].notes);
				++visible_count;
			}
			snprintf(status, sizeof(status), "%d radio channel(s) match", visible_count);
		} else if (view_mode == VIEW_ROUTES) {
			for (index = 0; index < route_count; ++index) {
				LVITEMA item;
				char count[24];
				char heritage[24];
				int row;
				snprintf(count, sizeof(count), "%d", route_summaries[index].train_count);
				snprintf(heritage, sizeof(heritage), "%s", route_summaries[index].heritage_count > 0 ? "yes" : "");
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_TEXT | LVIF_PARAM;
				item.iItem = visible_count;
				item.lParam = index;
				item.pszText = route_summaries[index].name;
				row = ListView_InsertItem(train_list, &item);
				set_list_cell(row, 1, count);
				set_list_cell(row, 2, heritage);
				++visible_count;
			}
			snprintf(status, sizeof(status), "%d active train(s) across %d line(s)", train_count, visible_count);
		} else if (view_mode == VIEW_NEAREST) {
			for (index = 0; index < nearest_count; ++index) {
				const Train* train = &trains[nearest_matches[index].train_index];
				LVITEMA item;
				char source[16];
				char distance[24];
				int row;
				snprintf(source, sizeof(source), "%s", train->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA");
				snprintf(distance, sizeof(distance), "%.1f mi", nearest_matches[index].distance);
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_TEXT | LVIF_PARAM;
				item.iItem = visible_count;
				item.lParam = index;
				item.pszText = source;
				row = ListView_InsertItem(train_list, &item);
				set_list_cell(row, 1, train->car_label);
				set_list_cell(row, 2, train->route_name);
				set_list_cell(row, 3, distance);
				set_list_cell(row, 4, train->destination);
				++visible_count;
			}
			snprintf(status, sizeof(status), "%d nearest train(s) to %s", visible_count, nearest_location);
		} else if (view_mode == VIEW_ALERTS) {
			for (index = 0; index < alert_count; ++index) {
				LVITEMA item;
				int row;
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_TEXT | LVIF_PARAM;
				item.iItem = visible_count;
				item.lParam = index;
				item.pszText = route_alerts[index].effect;
				row = ListView_InsertItem(train_list, &item);
				set_list_cell(row, 1, route_alerts[index].header);
				++visible_count;
			}
			snprintf(status, sizeof(status), "%d active alert(s) for route %s", visible_count, alert_route_id);
		} else {
			int located_count = 0;
			for (index = 0; index < train_count; ++index) {
				if (trains[index].has_location) ++located_count;
			}
			snprintf(status, sizeof(status), "%d train positions mapped", located_count);
		}
		if (view_mode != VIEW_MAP && sort_column >= 0) {
			ListView_SortItems(train_list, compare_main_rows, 0);
		}
		SetWindowTextA(status_text, status);
	}
}

static void show_selected_train(void)
{
	int selected = ListView_GetNextItem(train_list, -1, LVNI_SELECTED);
	LVITEMA item;
	char location[128];
	char speed[64];
	char last_updated[64];
	char delay[32];
	Train* train;
	if (view_mode != VIEW_TRAINS && view_mode != VIEW_FAVORITES) return;
	ListView_DeleteAllItems(detail_list);
	if (selected < 0) {
		selected_details_vehicle[0] = '\0';
		ListView_DeleteAllItems(stops_list);
		add_detail_pair(0, "Selection", "Select a train above");
		update_favorite_button();
		return;
	}
	memset(&item, 0, sizeof(item));
	item.mask = LVIF_PARAM;
	item.iItem = selected;
	if (!ListView_GetItem(train_list, &item)) return;
	train = &trains[(int)item.lParam];
	if (train->has_location) {
		snprintf(location, sizeof(location), "%.5f, %.5f", train->latitude, train->longitude);
	} else {
		snprintf(location, sizeof(location), "Not reported");
	}
	if (train->has_speed) snprintf(speed, sizeof(speed), "%.1f mph", train->speed_mph);
	else snprintf(speed, sizeof(speed), "Waiting for another position update");
	format_eta_cell(train->updated_at, last_updated, sizeof(last_updated));
	if (train->upcoming_stop_count > 0) format_delay_cell(&train->upcoming_stops[0], delay, sizeof(delay));
	else snprintf(delay, sizeof(delay), "Loading schedule comparison");
	add_detail_pair(0, "Source", train->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA");
	add_detail_pair(1, "Train", train->car_label[0] ? train->car_label : train->vehicle_id);
	add_detail_pair(2, "Vehicle ID", train->vehicle_id);
	add_detail_pair(3, "Line", train->route_name[0] ? train->route_name : train->route_id);
	add_detail_pair(4, "Destination", train->destination);
	add_detail_pair(5, "Current stop", train->current_stop);
	add_detail_pair(6, "Status", train->current_status);
	add_detail_pair(7, "Position", location);
	add_detail_pair(8, "Estimated speed", speed);
	add_detail_pair(9, "Last update", last_updated);
	add_detail_pair(10, "Next stop delay", delay);
	{
		int alert_index;
		const char* alert_state = "Not armed";
		for (alert_index = 0; alert_index < station_alert_count; ++alert_index) {
			if (strcmp(station_alerts[alert_index].vehicle_id, train->vehicle_id) == 0) {
				alert_state = station_alerts[alert_index].notified ? "Already notified" : station_alerts[alert_index].station;
				break;
			}
		}
		add_detail_pair(11, "Station alert", alert_state);
	}
	show_upcoming_stops(train->upcoming_stops, train->upcoming_stop_count);
	update_favorite_button();
	if (strcmp(selected_details_vehicle, train->vehicle_id) != 0) {
		snprintf(selected_details_vehicle, sizeof(selected_details_vehicle), "%s", train->vehicle_id);
		SetWindowTextA(status_text, "Loading upcoming stops and ETAs...");
		begin_train_details((int)item.lParam, 0);
	}
}

static DWORD WINAPI refresh_worker(void* parameter)
{
	HWND window = (HWND)parameter;
	RefreshResult* result = (RefreshResult*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RefreshResult));
	if (result != NULL) {
		result->count = fetch_trains(result->trains, MAX_TRAINS, result->error, sizeof(result->error));
	}
	if (!PostMessageA(window, WM_REFRESH_COMPLETE, 0, (LPARAM)result) && result != NULL) {
		HeapFree(GetProcessHeap(), 0, result);
	}
	return 0;
}

static void begin_refresh(void)
{
	HANDLE thread;
	if (InterlockedCompareExchange(&refresh_in_progress, 1, 0) != 0) return;
	SetWindowTextA(refresh_button, "Refreshing...");
	EnableWindow(refresh_button, FALSE);
	SetWindowTextA(status_text, "Contacting MBTA and Amtraker...");
	thread = CreateThread(NULL, 0, refresh_worker, main_window, 0, NULL);
	if (thread == NULL) {
		InterlockedExchange(&refresh_in_progress, 0);
		SetWindowTextA(refresh_button, "Refresh");
		EnableWindow(refresh_button, TRUE);
		SetWindowTextA(status_text, "Could not start the refresh task.");
		return;
	}
	CloseHandle(thread);
}

static void add_column(const char* title, int width, int index)
{
	LVCOLUMNA column;
	memset(&column, 0, sizeof(column));
	column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	column.pszText = (LPSTR)title;
	column.cx = width;
	column.iSubItem = index;
	ListView_InsertColumn(train_list, index, &column);
	++list_column_count;
}

static void add_table_column(HWND list, const char* title, int width, int index)
{
	LVCOLUMNA column;
	memset(&column, 0, sizeof(column));
	column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	column.pszText = (LPSTR)title;
	column.cx = width;
	column.iSubItem = index;
	ListView_InsertColumn(list, index, &column);
}

static void set_table_cell(HWND list, int row, int column, const char* text)
{
	ListView_SetItemText(list, row, column, (LPSTR)text);
}

static void format_eta_cell(const char* timestamp, char* output, size_t output_size)
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int hour_12;
	char zone[12] = "";
	if (timestamp == NULL || timestamp[0] == '\0') {
		snprintf(output, output_size, "Not provided");
		return;
	}
	if (sscanf_s(timestamp, "%4d-%2d-%2dT%2d:%2d", &year, &month, &day, &hour, &minute) != 5) {
		snprintf(output, output_size, "%s", timestamp);
		return;
	}
	if (timestamp[19] == 'Z') snprintf(zone, sizeof(zone), " UTC");
	else if (timestamp[19] == '+' || timestamp[19] == '-') snprintf(zone, sizeof(zone), " %.6s", timestamp + 19);
	hour_12 = hour % 12;
	if (hour_12 == 0) hour_12 = 12;
	snprintf(output, output_size, "%04d-%02d-%02d  %d:%02d %s%s",
		year, month, day, hour_12, minute, hour < 12 ? "AM" : "PM", zone);
}

static void format_delay_cell(const TrainStopPrediction* stop, char* output, size_t output_size)
{
	if (!stop->has_delay) snprintf(output, output_size, "Not reported");
	else if (stop->delay_minutes > 0.5) snprintf(output, output_size, "%.0f min late", stop->delay_minutes);
	else if (stop->delay_minutes < -0.5) snprintf(output, output_size, "%.0f min early", -stop->delay_minutes);
	else snprintf(output, output_size, "On time");
}

static void add_detail_pair(int row, const char* field, const char* value)
{
	LVITEMA item;
	memset(&item, 0, sizeof(item));
	item.mask = LVIF_TEXT;
	item.iItem = row;
	item.pszText = (LPSTR)field;
	ListView_InsertItem(detail_list, &item);
	set_table_cell(detail_list, row, 1, value[0] ? value : "Not available");
}

static void show_upcoming_stops(const TrainStopPrediction* stops, int count)
{
	int index;
	ListView_DeleteAllItems(stops_list);
	displayed_stop_count = 0;
	if (count <= 0) {
		LVITEMA item;
		int row;
		memset(&item, 0, sizeof(item));
		item.mask = LVIF_TEXT;
		item.pszText = "No predictions reported";
		row = ListView_InsertItem(stops_list, &item);
		set_table_cell(stops_list, row, 1, "ETA unavailable");
		set_table_cell(stops_list, row, 2, "ETA unavailable");
		set_table_cell(stops_list, row, 3, "Not reported");
		return;
	}
	if (count > MAX_UPCOMING_STOPS) count = MAX_UPCOMING_STOPS;
	displayed_stop_count = count;
	memcpy(displayed_stops, stops, sizeof(TrainStopPrediction) * count);
	for (index = 0; index < count; ++index) {
		LVITEMA item;
		char arrival[64];
		char departure[64];
		char delay[32];
		int row;
		format_eta_cell(stops[index].arrival, arrival, sizeof(arrival));
		format_eta_cell(stops[index].departure, departure, sizeof(departure));
		format_delay_cell(&stops[index], delay, sizeof(delay));
		memset(&item, 0, sizeof(item));
		item.mask = LVIF_TEXT | LVIF_PARAM;
		item.iItem = index;
		item.lParam = index;
		item.pszText = (LPSTR)(stops[index].name[0] ? stops[index].name : "Unknown stop");
		row = ListView_InsertItem(stops_list, &item);
		set_table_cell(stops_list, row, 1, arrival);
		set_table_cell(stops_list, row, 2, departure);
		set_table_cell(stops_list, row, 3, delay);
	}
	if (stop_sort_column >= 0) ListView_SortItems(stops_list, compare_stop_rows, 0);
}

static void reset_columns(void)
{
	while (list_column_count > 0) {
		ListView_DeleteColumn(train_list, 0);
		--list_column_count;
	}
}

static void layout_controls(int width, int height)
{
	int list_height = height - ((view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES) ? 430 : 378);
	int list_width = width - 40;
	int input_width = width - ((view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES) ? 620 : 390);
	if (list_height < 200) list_height = 200;
	if (list_width < 300) list_width = 300;
	if (input_width < 180) input_width = 180;
	MoveWindow(trains_view_button, 20, 77, 96, 28, TRUE);
	MoveWindow(favorites_view_button, 120, 77, 112, 28, TRUE);
	MoveWindow(radio_view_button, 236, 77, 150, 28, TRUE);
	MoveWindow(map_view_button, 392, 77, 78, 28, TRUE);
	MoveWindow(delayed_filter_button, 478, 80, 124, 24, TRUE);
	MoveWindow(filter_edit, 20, 114, input_width, 28, TRUE);
	MoveWindow(saved_search_combo, width - 590, 114, 160, 180, TRUE);
	MoveWindow(save_search_button, width - 420, 114, 72, 28, TRUE);
	MoveWindow(run_button, width - 340, 114, 72, 28, TRUE);
	MoveWindow(watch_button, width - 250, 114, 84, 28, TRUE);
	MoveWindow(refresh_button, width - 154, 114, 74, 28, TRUE);
	MoveWindow(export_button, width - 74, 114, 70, 28, TRUE);
	if (view_mode == VIEW_MAP) {
		MoveWindow(map_overview_label, width - 370, 188, 330, 22, TRUE);
		MoveWindow(map_overview_edit, width - 370, 212, 330, 86, TRUE);
		MoveWindow(map_full_details_button, width - 370, 304, 330, 28, TRUE);
		ShowWindow(map_overview_label, SW_SHOW);
		ShowWindow(map_overview_edit, SW_SHOW);
		ShowWindow(map_full_details_button, SW_SHOW);
	} else {
		ShowWindow(map_overview_label, SW_HIDE);
		ShowWindow(map_overview_edit, SW_HIDE);
		ShowWindow(map_full_details_button, SW_HIDE);
	}
	if (view_mode == VIEW_DETAIL) {
		ShowWindow(train_list, SW_HIDE);
		ShowWindow(detail_list, SW_HIDE);
		ShowWindow(stops_list, SW_HIDE);
		ShowWindow(stops_label, SW_HIDE);
		ShowWindow(details_edit, SW_SHOW);
		MoveWindow(details_label, 20, 153, list_width, 20, TRUE);
		MoveWindow(details_edit, 20, 178, list_width, height - 210, TRUE);
	} else if (view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES) {
		int table_top = 153 + list_height + 8;
		int table_height = height - table_top - 56;
		int left_width = (width - 56) / 2;
		int right_x = 20 + left_width + 16;
		int right_width = width - right_x - 20;
		if (table_height < 110) table_height = 110;
		ShowWindow(train_list, SW_SHOW);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(detail_list, SW_SHOW);
		ShowWindow(stops_label, SW_SHOW);
		ShowWindow(stops_list, SW_SHOW);
		ShowWindow(details_edit, SW_HIDE);
		MoveWindow(train_list, 20, 153, list_width, list_height, TRUE);
		MoveWindow(details_label, 20, table_top, left_width - 332, 22, TRUE);
		MoveWindow(favorite_button, 20 + left_width - 326, table_top - 1, 72, 24, TRUE);
		MoveWindow(selected_alerts_button, 20 + left_width - 250, table_top - 1, 56, 24, TRUE);
		MoveWindow(notify_button, 20 + left_width - 190, table_top - 1, 62, 24, TRUE);
		MoveWindow(nearest_button, 20 + left_width - 124, table_top - 1, 62, 24, TRUE);
		MoveWindow(distance_button, 20 + left_width - 58, table_top - 1, 56, 24, TRUE);
		MoveWindow(stops_label, right_x, table_top, right_width, 22, TRUE);
		MoveWindow(detail_list, 20, table_top + 24, left_width, table_height, TRUE);
		MoveWindow(stops_list, right_x, table_top + 24, right_width, table_height, TRUE);
	} else if (view_mode == VIEW_MAP) {
		ShowWindow(train_list, SW_HIDE);
		ShowWindow(detail_list, SW_HIDE);
		ShowWindow(stops_list, SW_HIDE);
		ShowWindow(stops_label, SW_HIDE);
		ShowWindow(details_label, SW_HIDE);
		ShowWindow(details_edit, SW_HIDE);
		ShowWindow(map_overview_label, SW_SHOW);
		ShowWindow(map_overview_edit, SW_SHOW);
		ShowWindow(map_full_details_button, SW_SHOW);
		MoveWindow(map_overview_label, width - 370, 188, 330, 22, TRUE);
		MoveWindow(map_overview_edit, width - 370, 212, 330, 86, TRUE);
		MoveWindow(map_full_details_button, width - 370, 304, 330, 28, TRUE);
	} else {
		ShowWindow(train_list, SW_SHOW);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(detail_list, SW_HIDE);
		ShowWindow(stops_list, SW_HIDE);
		ShowWindow(stops_label, SW_HIDE);
		ShowWindow(details_edit, SW_SHOW);
		ShowWindow(map_overview_label, SW_HIDE);
		ShowWindow(map_overview_edit, SW_HIDE);
		ShowWindow(map_full_details_button, SW_HIDE);
		MoveWindow(train_list, 20, 153, list_width, list_height, TRUE);
		MoveWindow(details_label, 20, 153 + list_height + 8, list_width, 20, TRUE);
		MoveWindow(details_edit, 20, 153 + list_height + 30, list_width, 150, TRUE);
	}
	MoveWindow(status_text, 20, height - 24, list_width, 20, TRUE);
}

static void set_view_mode(int mode)
{
	RECT client;
	view_mode = mode;
	sort_column = 0;
	sort_ascending = 1;
	stop_sort_column = 0;
	stop_sort_ascending = 1;
	SendMessageA(trains_view_button, BM_SETCHECK, mode == VIEW_TRAINS ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageA(favorites_view_button, BM_SETCHECK, mode == VIEW_FAVORITES ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageA(radio_view_button, BM_SETCHECK, mode == VIEW_RADIO ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageA(map_view_button, BM_SETCHECK, mode == VIEW_MAP ? BST_CHECKED : BST_UNCHECKED, 0);
	ShowWindow(delayed_filter_button,
		(mode == VIEW_TRAINS || mode == VIEW_FAVORITES) ? SW_SHOW : SW_HIDE);
	ShowWindow(export_button, SW_SHOW);
	if (mode == VIEW_TRAINS || mode == VIEW_FAVORITES) {
		ShowWindow(favorite_button, SW_SHOW);
		ShowWindow(selected_alerts_button, SW_SHOW);
		ShowWindow(notify_button, SW_SHOW);
		ShowWindow(nearest_button, SW_SHOW);
		ShowWindow(distance_button, SW_SHOW);
		ShowWindow(saved_search_combo, SW_SHOW);
		ShowWindow(save_search_button, SW_SHOW);
	} else {
		ShowWindow(favorite_button, SW_HIDE);
		ShowWindow(selected_alerts_button, SW_HIDE);
		ShowWindow(notify_button, SW_HIDE);
		ShowWindow(nearest_button, SW_HIDE);
		ShowWindow(distance_button, SW_HIDE);
		ShowWindow(saved_search_combo, SW_HIDE);
		ShowWindow(save_search_button, SW_HIDE);
	}
	ShowWindow(map_zoom_in_button, mode == VIEW_MAP ? SW_SHOW : SW_HIDE);
	ShowWindow(map_zoom_out_button, mode == VIEW_MAP ? SW_SHOW : SW_HIDE);
	ShowWindow(map_overview_label, mode == VIEW_MAP ? SW_SHOW : SW_HIDE);
	ShowWindow(map_overview_edit, mode == VIEW_MAP ? SW_SHOW : SW_HIDE);
	ShowWindow(map_full_details_button, mode == VIEW_MAP ? SW_SHOW : SW_HIDE);
	if (mode == VIEW_TRAINS || mode == VIEW_FAVORITES) {
		ShowWindow(detail_list, SW_SHOW);
		ShowWindow(stops_list, SW_SHOW);
		ShowWindow(stops_label, SW_SHOW);
		ShowWindow(details_edit, SW_HIDE);
		SetWindowTextA(details_label, "Train details");
		SetWindowTextA(stops_label, "Upcoming stops and times");
	} else {
		ShowWindow(detail_list, SW_HIDE);
		ShowWindow(stops_list, SW_HIDE);
		ShowWindow(stops_label, SW_HIDE);
		ShowWindow(details_edit, SW_SHOW);
	}
	reset_columns();
	if (mode == VIEW_TRAINS || mode == VIEW_FAVORITES) {
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE,
			(LPARAM)"Search trains or run: routes, nearest Worcester, distance 1234 Providence, alerts 1234");
		ShowWindow(refresh_button, SW_SHOW);
		ShowWindow(details_label, SW_SHOW);
		SetWindowTextA(details_label, mode == VIEW_FAVORITES ? "Favorite train details" : "Train details");
		SendMessageA(delayed_filter_button, BM_SETCHECK, delayed_only ? BST_CHECKED : BST_UNCHECKED, 0);
		add_column("Source", 78, 0);
		add_column("Train", 82, 1);
		add_column("Line", 180, 2);
		add_column("Destination", 155, 3);
		add_column("Current stop", 165, 4);
		add_column("Status", 105, 5);
		add_column("Speed", 80, 6);
		add_column("Favorite", 72, 7);
		refresh_visible_list();
		show_selected_train();
	} else if (mode == VIEW_MAP) {
		if (!map_shapes_loaded) map_shapes_attempted = 0;
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE, (LPARAM)"Select a marker for train details, or search trains on the list view");
		map_data_dirty = 1;
		ShowWindow(refresh_button, SW_SHOW);
		ShowWindow(train_list, SW_HIDE);
		ShowWindow(detail_list, SW_HIDE);
		ShowWindow(stops_list, SW_HIDE);
		ShowWindow(stops_label, SW_HIDE);
		ShowWindow(details_label, SW_HIDE);
		ShowWindow(details_edit, SW_HIDE);
		update_map_overview(map_selected_train_index);
		if (GetClientRect(main_window, &client)) {
			MoveWindow(map_zoom_in_button, client.right - 82, 145, 32, 28, TRUE);
			MoveWindow(map_zoom_out_button, client.right - 44, 145, 32, 28, TRUE);
		}
		InvalidateRect(main_window, NULL, TRUE);
	} else if (mode == VIEW_RADIO) {
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE,
			(LPARAM)"Try: road for Fitchburg Line, dispatch Old Colony, or yard");
		ShowWindow(refresh_button, SW_HIDE);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(details_edit, SW_SHOW);
		SetWindowTextA(details_label, "Source and use");
		SetWindowTextA(details_edit,
			"Source: Scan New England Wiki, a community-maintained scanner database. Entries are dated there. "
			"Channel assignments can be reused or reassigned; cross-check RadioReference.com or the FCC ULS "
			"before relying on them.");
		add_column("Line", 245, 0);
		add_column("Type", 125, 1);
		add_column("Ch", 70, 2);
		add_column("Frequency MHz", 110, 3);
		add_column("Notes", 600, 4);
		refresh_visible_list();
	} else if (mode == VIEW_ROUTES) {
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE, (LPARAM)"Enter a command such as routes or nearest Worcester");
		ShowWindow(refresh_button, SW_SHOW);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(details_edit, SW_SHOW);
		SetWindowTextA(details_label, "Route summary");
		SetWindowTextA(details_edit, "Counts are based on the currently loaded active train list.");
		add_column("Line", 300, 0);
		add_column("Active trains", 110, 1);
		add_column("Heritage", 100, 2);
		build_route_summaries();
		refresh_visible_list();
	} else if (mode == VIEW_NEAREST) {
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE, (LPARAM)"Enter nearest <station or latitude,longitude>");
		ShowWindow(refresh_button, SW_SHOW);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(details_edit, SW_SHOW);
		SetWindowTextA(details_label, "Nearest trains");
		SetWindowTextA(details_edit, "Distances use the train positions currently reported by the feeds.");
		add_column("Source", 85, 0);
		add_column("Train", 90, 1);
		add_column("Line", 210, 2);
		add_column("Distance", 95, 3);
		add_column("Destination", 240, 4);
		refresh_visible_list();
	} else if (mode == VIEW_ALERTS) {
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE, (LPARAM)"Enter alerts <train ID or route ID>");
		ShowWindow(refresh_button, SW_SHOW);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(details_edit, SW_SHOW);
		SetWindowTextA(details_label, "Service alerts");
		SetWindowTextA(details_edit, "Live service alerts are available for MBTA commuter rail routes.");
		add_column("Effect", 140, 0);
		add_column("Alert", 760, 1);
		refresh_visible_list();
	} else {
		SendMessageA(filter_edit, EM_SETCUEBANNER, TRUE, (LPARAM)"Enter a train ID and choose Run for full details");
		ShowWindow(refresh_button, SW_HIDE);
		ShowWindow(details_label, SW_SHOW);
		ShowWindow(details_edit, SW_SHOW);
		SetWindowTextA(details_label, "Full train details");
		SetWindowTextA(details_edit, "Loading full train details and predictions...");
	}
	if (GetClientRect(main_window, &client)) layout_controls(client.right, client.bottom);
}

static DWORD WINAPI alerts_worker(void* parameter)
{
	AlertsRequest* request = (AlertsRequest*)parameter;
	AlertsResult* result = (AlertsResult*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(AlertsResult));
	HWND window = main_window;
	if (result != NULL) {
		snprintf(result->route_id, sizeof(result->route_id), "%s", request->route_id);
		result->count = fetch_route_alerts(request->route_id, result->alerts, MAX_ROUTE_ALERTS,
			result->error, sizeof(result->error));
	}
	HeapFree(GetProcessHeap(), 0, request);
	if (!PostMessageA(window, WM_ALERTS_COMPLETE, 0, (LPARAM)result) && result != NULL) {
		HeapFree(GetProcessHeap(), 0, result);
	}
	return 0;
}

static void begin_alerts(const char* route_id)
{
	AlertsRequest* request;
	HANDLE thread;
	if (InterlockedCompareExchange(&alerts_in_progress, 1, 0) != 0) return;
	request = (AlertsRequest*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(AlertsRequest));
	if (request == NULL) {
		InterlockedExchange(&alerts_in_progress, 0);
		SetWindowTextA(status_text, "Could not allocate the service alert request.");
		return;
	}
	snprintf(request->route_id, sizeof(request->route_id), "%s", route_id);
	EnableWindow(run_button, FALSE);
	SetWindowTextA(status_text, "Loading active route alerts...");
	thread = CreateThread(NULL, 0, alerts_worker, request, 0, NULL);
	if (thread == NULL) {
		HeapFree(GetProcessHeap(), 0, request);
		InterlockedExchange(&alerts_in_progress, 0);
		EnableWindow(run_button, TRUE);
		SetWindowTextA(status_text, "Could not start the alert request.");
		return;
	}
	CloseHandle(thread);
}

static DWORD WINAPI details_worker(void* parameter)
{
	DetailsRequest* request = (DetailsRequest*)parameter;
	DetailsResult* result = (DetailsResult*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DetailsResult));
	HWND window = main_window;
	if (result != NULL) {
		snprintf(result->vehicle_id, sizeof(result->vehicle_id), "%s", request->train.vehicle_id);
		result->full_view = request->full_view;
		format_train_details_with_stops(&request->train, result->text, sizeof(result->text),
			result->stops, MAX_UPCOMING_STOPS, &result->stop_count);
	}
	HeapFree(GetProcessHeap(), 0, request);
	if (!PostMessageA(window, WM_DETAILS_COMPLETE, 0, (LPARAM)result) && result != NULL) {
		HeapFree(GetProcessHeap(), 0, result);
	}
	return 0;
}

static void begin_train_details(int train_index, int full_view)
{
	DetailsRequest* request;
	HANDLE thread;
	if (InterlockedCompareExchange(&details_in_progress, 1, 0) != 0) {
		SetWindowTextA(status_text, "A train detail request is already running.");
		return;
	}
	request = (DetailsRequest*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DetailsRequest));
	if (request == NULL) {
		InterlockedExchange(&details_in_progress, 0);
		SetWindowTextA(status_text, "Could not allocate the train detail request.");
		return;
	}
	request->train = trains[train_index];
	request->full_view = full_view;
	if (full_view) set_view_mode(VIEW_DETAIL);
	SetWindowTextA(details_edit, full_view ? "Loading full train details and predictions..." :
		"Loading upcoming stops and ETAs...");
	SetWindowTextA(status_text, "Loading train predictions and schedule...");
	EnableWindow(run_button, FALSE);
	thread = CreateThread(NULL, 0, details_worker, request, 0, NULL);
	if (thread == NULL) {
		HeapFree(GetProcessHeap(), 0, request);
		InterlockedExchange(&details_in_progress, 0);
		EnableWindow(run_button, TRUE);
		SetWindowTextA(status_text, "Could not start the train detail request.");
		return;
	}
	CloseHandle(thread);
}

static void toggle_watch(void)
{
	watch_enabled = !watch_enabled;
	if (watch_enabled) {
		SetTimer(main_window, 1, 30000, NULL);
		SetWindowTextA(watch_button, "Watch: On");
		SetWindowTextA(status_text, "Automatic refresh every 30 seconds.");
	} else {
		KillTimer(main_window, 1);
		SetWindowTextA(watch_button, "Watch: Off");
		SetWindowTextA(status_text, "Automatic refresh paused.");
	}
}

static DWORD WINAPI play_train_egg_horn(void* parameter)
{
	HWND window = (HWND)parameter;
	const DWORD sample_rate = 22050;
	const double duration = 1.35;
	DWORD sample_count = (DWORD)(sample_rate * duration);
	DWORD data_size = sample_count * sizeof(short);
	WaveFileHeader* wave = (WaveFileHeader*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		sizeof(WaveFileHeader) + data_size);
	short* samples;
	DWORD index;
	if (wave == NULL) {
		PostMessageA(window, WM_EASTER_EGG_COMPLETE, 0, 0);
		return 0;
	}
	memcpy(wave->riff, "RIFF", 4);
	wave->file_size = 36 + data_size;
	memcpy(wave->wave, "WAVE", 4);
	memcpy(wave->format_chunk, "fmt ", 4);
	wave->format_size = 16;
	wave->format = 1;
	wave->channels = 1;
	wave->sample_rate = sample_rate;
	wave->byte_rate = sample_rate * sizeof(short);
	wave->block_align = sizeof(short);
	wave->bits_per_sample = 16;
	memcpy(wave->data_chunk, "data", 4);
	wave->data_size = data_size;
	samples = (short*)(wave + 1);
	for (index = 0; index < sample_count; ++index) {
		double time = (double)index / sample_rate;
		double attack = time < 0.045 ? time / 0.045 : 1.0;
		double release = time > duration - 0.22 ? (duration - time) / 0.22 : 1.0;
		double envelope = attack < release ? attack : release;
		double horn = 0.43 * sin(2.0 * 3.141592653589793 * 311.0 * time) +
			0.34 * sin(2.0 * 3.141592653589793 * 392.0 * time) +
			0.23 * sin(2.0 * 3.141592653589793 * 466.0 * time) +
			0.07 * sin(2.0 * 3.141592653589793 * 622.0 * time);
		samples[index] = (short)(horn * envelope * 24500.0);
	}
	PlaySoundA((LPCSTR)wave, NULL, SND_MEMORY | SND_SYNC);
	HeapFree(GetProcessHeap(), 0, wave);
	PostMessageA(window, WM_EASTER_EGG_COMPLETE, 0, 0);
	return 0;
}

static int play_local_train_song_segment(void)
{
	char executable_path[MAX_PATH];
	char command[MAX_PATH + 128];
	char* separator;
	DWORD length;
	if (easter_mp3_open) {
		mciSendStringA("stop mbtaEggTrack", NULL, 0, NULL);
		mciSendStringA("close mbtaEggTrack", NULL, 0, NULL);
		easter_mp3_open = 0;
	}
	length = GetModuleFileNameA(NULL, executable_path, (DWORD)sizeof(executable_path));
	if (length == 0 || length >= sizeof(executable_path)) return 0;
	separator = strrchr(executable_path, '\\');
	if (separator == NULL) return 0;
	separator[1] = '\0';
	if (strcat_s(executable_path, sizeof(executable_path),
		"Biggie Smalls - Thomas the Tank Engine REMASTERED ( original by Norkkom ).mp3") != 0) return 0;
	if (GetFileAttributesA(executable_path) == INVALID_FILE_ATTRIBUTES) return 0;
	if (snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias mbtaEggTrack", executable_path) < 0 ||
		mciSendStringA(command, NULL, 0, NULL) != 0) return 0;
	easter_mp3_open = 1;
	if (mciSendStringA("set mbtaEggTrack time format milliseconds", NULL, 0, NULL) != 0 ||
		mciSendStringA("play mbtaEggTrack from 16000 to 36000 notify", NULL, 0, main_window) != 0) {
		mciSendStringA("close mbtaEggTrack", NULL, 0, NULL);
		easter_mp3_open = 0;
		return 0;
	}
	return 1;
}

static BOOL CALLBACK hide_easter_control(HWND control, LPARAM context)
{
	(void)context;
	if (easter_hidden_control_count < MAX_EASTER_HIDDEN_CONTROLS && IsWindowVisible(control)) {
		easter_hidden_controls[easter_hidden_control_count++] = control;
		ShowWindow(control, SW_HIDE);
	}
	return TRUE;
}

static void restore_easter_controls(void)
{
	int index;
	for (index = 0; index < easter_hidden_control_count; ++index) {
		if (IsWindow(easter_hidden_controls[index])) ShowWindow(easter_hidden_controls[index], SW_SHOW);
	}
	easter_hidden_control_count = 0;
}

static void start_train_easter_egg(void)
{
	HANDLE horn_thread;
	if (easter_egg_active) return;
	if (!GetWindowRect(main_window, &easter_original_rect)) return;
	SetWindowTextA(filter_edit, "");
	easter_start_tick = GetTickCount64();
	easter_logo_angle = 0.0f;
	easter_egg_active = 1;
	easter_hidden_control_count = 0;
	EnumChildWindows(main_window, hide_easter_control, 0);
	SetTimer(main_window, EASTER_EGG_TIMER, 16, NULL);
	InvalidateRect(main_window, NULL, TRUE);
	if (play_local_train_song_segment()) {
		SetWindowTextA(status_text, "I like trains. Playing the local 0:16-0:36 clip!");
	} else {
		SetWindowTextA(status_text, "MP3 unavailable; sounding the train horn instead.");
		horn_thread = CreateThread(NULL, 0, play_train_egg_horn, main_window, 0, NULL);
		if (horn_thread != NULL) CloseHandle(horn_thread);
		else PostMessageA(main_window, WM_EASTER_EGG_COMPLETE, 0, 0);
	}
}

static void finish_train_easter_egg(HWND window)
{
	if (!easter_egg_active) return;
	easter_egg_active = 0;
	easter_logo_angle = 0.0f;
	KillTimer(window, EASTER_EGG_TIMER);
	restore_easter_controls();
	SetWindowPos(window, NULL, easter_original_rect.left, easter_original_rect.top,
		easter_original_rect.right - easter_original_rect.left,
		easter_original_rect.bottom - easter_original_rect.top,
		SWP_NOACTIVATE | SWP_NOZORDER);
	InvalidateRect(window, NULL, TRUE);
}

static void show_command_message(const char* title, const char* message)
{
	SetWindowTextA(details_label, title);
	SetWindowTextA(details_edit, message);
	SetWindowTextA(status_text, title);
}

static void execute_command(void)
{
	char query[256];
	char first_word[32];
	char* remainder;
	size_t word_length;
	int train_index;
	GetWindowTextA(filter_edit, query, (int)sizeof(query));
	word_length = strcspn(query, " \t");
	if (query[0] == '\0') return;
	if (word_length >= sizeof(first_word)) {
		SetWindowTextA(status_text, "Command is too long.");
		return;
	}
	memcpy(first_word, query, word_length);
	first_word[word_length] = '\0';
	remainder = query + word_length;
	while (*remainder == ' ' || *remainder == '\t') ++remainder;

	if (_stricmp(query, "I like trains") == 0) {
		start_train_easter_egg();
		return;
	}
	if (_stricmp(query, "help") == 0 || _stricmp(query, "?") == 0) {
		static const char help[] =
			"Train search: list | show | find | search <description>\r\n"
			"Example: list all heritage trains active\r\n\r\n"
			"Train details: enter a vehicle ID, car number, or Amtrak run ID\r\n\r\n"
			"routes\r\n"
			"nearest <latitude,longitude or station name>\r\n"
			"distance <train ID> <latitude,longitude or station name>\r\n"
			"alerts <train ID or MBTA route ID>\r\n"
			"freq <line and/or road/dispatch/yard keywords>\r\n"
			"favorite [train ID or description] | favorites\r\n"
			"notify <train ID> <station or latitude,longitude> | unnotify <train ID>\r\n"
			"watch toggles automatic 30-second refresh. Close the window or enter Q to quit.";
		MessageBoxA(main_window, help, "MBTA Train Tracker Commands", MB_OK | MB_ICONINFORMATION);
		return;
	}
	if (_stricmp(query, "q") == 0 || _stricmp(query, "quit") == 0) {
		DestroyWindow(main_window);
		return;
	}
	if (_stricmp(query, "watch") == 0) {
		toggle_watch();
		return;
	}
	if (_stricmp(query, "favorites") == 0 || _stricmp(query, "favourites") == 0) {
		set_view_mode(VIEW_FAVORITES);
		SetWindowTextA(filter_edit, "");
		return;
	}
	if (_stricmp(first_word, "favorite") == 0 || _stricmp(first_word, "favourite") == 0) {
		if (remainder[0] == '\0') {
			toggle_selected_favorite();
		} else if (save_favorite_query(remainder)) {
			if (view_mode == VIEW_FAVORITES) refresh_visible_list();
			SetWindowTextA(status_text, "Added to favorites.");
		} else {
			SetWindowTextA(status_text, "Favorites are full.");
		}
		return;
	}
	if (_stricmp(first_word, "notify") == 0) {
		char identifier[64];
		char* separator = strchr(remainder, ' ');
		size_t identifier_length = separator != NULL ? (size_t)(separator - remainder) : strlen(remainder);
		const char* station = separator != NULL ? separator + 1 : "";
		while (*station == ' ') ++station;
		if (identifier_length == 0 || identifier_length >= sizeof(identifier) || station[0] == '\0') {
			SetWindowTextA(status_text, "Usage: notify <train ID> <station or latitude,longitude>");
			return;
		}
		memcpy(identifier, remainder, identifier_length);
		identifier[identifier_length] = '\0';
		if (save_station_alert(identifier, station)) update_favorite_button();
		return;
	}
	if (_stricmp(first_word, "unnotify") == 0) {
		if (remainder[0] == '\0') SetWindowTextA(status_text, "Usage: unnotify <train ID>");
		else remove_station_alert(remainder);
		return;
	}
	if (_stricmp(query, "routes") == 0) {
		set_view_mode(VIEW_ROUTES);
		return;
	}
	if (_stricmp(first_word, "nearest") == 0) {
		double latitude;
		double longitude;
		if (!parse_gui_location(remainder, &latitude, &longitude)) {
			show_command_message("Nearest trains", "Could not understand that location. Try nearest 42.35,-71.06 or nearest Worcester.");
			return;
		}
		snprintf(nearest_location, sizeof(nearest_location), "%s", remainder);
		nearest_latitude = latitude;
		nearest_longitude = longitude;
		build_nearest_matches(latitude, longitude);
		set_view_mode(VIEW_NEAREST);
		return;
	}
	if (_stricmp(first_word, "distance") == 0) {
		char identifier[64];
		char message[512];
		char* id_space = strchr(remainder, ' ');
		size_t id_length = id_space != NULL ? (size_t)(id_space - remainder) : strlen(remainder);
		const char* location_text = id_space != NULL ? id_space + 1 : "";
		double latitude;
		double longitude;
		double distance;
		while (*location_text == ' ') ++location_text;
		if (id_length == 0 || id_length >= sizeof(identifier)) {
			show_command_message("Distance", "Usage: distance <train ID> <location>");
			return;
		}
		memcpy(identifier, remainder, id_length);
		identifier[id_length] = '\0';
		train_index = find_train_index(identifier);
		if (train_index < 0) {
			snprintf(message, sizeof(message), "No active train matches '%s'.", identifier);
		} else if (!trains[train_index].has_location) {
			snprintf(message, sizeof(message), "Train %s does not currently report a position.", identifier);
		} else if (!parse_gui_location(location_text, &latitude, &longitude)) {
			snprintf(message, sizeof(message), "Could not understand location '%s'.", location_text);
		} else {
			distance = haversine_miles(trains[train_index].latitude, trains[train_index].longitude, latitude, longitude);
			snprintf(message, sizeof(message), "Train %s is about %.1f miles from %s.", identifier, distance, location_text);
		}
		show_command_message("Distance result", message);
		return;
	}
	if (_stricmp(first_word, "alerts") == 0) {
		char route_id[32];
		if (remainder[0] == '\0') {
			show_command_message("Service alerts", "Usage: alerts <train ID> or alerts <route ID>");
			return;
		}
		train_index = find_train_index(remainder);
		if (train_index >= 0) {
			if (trains[train_index].source == TRAIN_SOURCE_AMTRAK) {
				show_command_message("Service alerts", "Live alerts are only available for MBTA commuter rail routes.");
				return;
			}
			snprintf(route_id, sizeof(route_id), "%s", trains[train_index].route_id);
		} else if (strlen(remainder) < sizeof(route_id)) {
			snprintf(route_id, sizeof(route_id), "%s", remainder);
		} else {
			show_command_message("Service alerts", "That route ID is too long.");
			return;
		}
		begin_alerts(route_id);
		return;
	}
	if (_stricmp(first_word, "freq") == 0 || _stricmp(first_word, "frequency") == 0 ||
		_stricmp(first_word, "frequencies") == 0) {
		set_view_mode(VIEW_RADIO);
		SetWindowTextA(filter_edit, remainder);
		return;
	}
	if (_stricmp(first_word, "list") == 0 || _stricmp(first_word, "show") == 0 ||
		_stricmp(first_word, "find") == 0 || _stricmp(first_word, "search") == 0) {
		set_view_mode(VIEW_TRAINS);
		SetWindowTextA(filter_edit, remainder);
		return;
	}
	train_index = find_train_index(query);
	if (train_index >= 0) {
		begin_train_details(train_index, 1);
		return;
	}
	show_command_message("Command not recognized",
		"Use help to see the available commands, or type a description in the search field.");
}

static int is_action_command(const char* text)
{
	static const char* commands[] = {
		"routes", "nearest", "distance", "alerts", "freq", "frequency", "frequencies",
		"watch", "favorite", "favourite", "favorites", "favourites", "notify", "unnotify", "help", "quit", "q"
	};
	char first_word[32];
	size_t length = strcspn(text, " \t");
	size_t index;
	if (length == 0 || length >= sizeof(first_word)) return 0;
	memcpy(first_word, text, length);
	first_word[length] = '\0';
	for (index = 0; index < sizeof(commands) / sizeof(commands[0]); ++index) {
		if (_stricmp(first_word, commands[index]) == 0) return 1;
	}
	return 0;
}

static void write_csv_field(FILE* file, const char* value)
{
	const char* character = value != NULL ? value : "";
	fputc('"', file);
	while (*character != '\0') {
		if (*character == '"') fputc('"', file);
		if (*character != '\r') fputc(*character == '\n' ? ' ' : *character, file);
		++character;
	}
	fputc('"', file);
}

static void update_sort_indicator(HWND list, int sorted_column, int ascending)
{
	HWND header = ListView_GetHeader(list);
	int index;
	int count = Header_GetItemCount(header);
	for (index = 0; index < count; ++index) {
		HDITEMA item;
		memset(&item, 0, sizeof(item));
		item.mask = HDI_FORMAT;
		if (!Header_GetItem(header, index, &item)) continue;
		item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
		if (index == sorted_column) item.fmt |= ascending ? HDF_SORTUP : HDF_SORTDOWN;
		Header_SetItem(header, index, &item);
	}
}

static void export_csv(void)
{
	OPENFILENAMEA dialog;
	char path[MAX_PATH] = "MBTA-trains.csv";
	FILE* file;
	int index;
	memset(&dialog, 0, sizeof(dialog));
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = main_window;
	dialog.lpstrFilter = "CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
	dialog.lpstrFile = path;
	dialog.nMaxFile = (DWORD)sizeof(path);
	dialog.lpstrDefExt = "csv";
	dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	if (!GetSaveFileNameA(&dialog)) return;
	if (fopen_s(&file, path, "w") != 0) {
		SetWindowTextA(status_text, "Could not create the CSV file.");
		return;
	}
	if (view_mode == VIEW_RADIO) {
		fprintf(file, "Line,Type,Channel,Frequency MHz,Notes\n");
		for (index = 0; index < radio_count; ++index) {
			write_csv_field(file, radio_channels[index].line); fputc(',', file);
			write_csv_field(file, radio_channels[index].channel_type); fputc(',', file);
			write_csv_field(file, radio_channels[index].aar_channel); fputc(',', file);
			fprintf(file, "%.4f,", radio_channels[index].frequency_mhz);
			write_csv_field(file, radio_channels[index].notes); fputc('\n', file);
		}
	} else if (view_mode == VIEW_ROUTES) {
		fprintf(file, "Line,Active trains,Heritage trains\n");
		for (index = 0; index < route_count; ++index) {
			write_csv_field(file, route_summaries[index].name);
			fprintf(file, ",%d,%d\n", route_summaries[index].train_count, route_summaries[index].heritage_count);
		}
	} else if (view_mode == VIEW_NEAREST) {
		fprintf(file, "Source,Train,Line,Distance miles,Destination\n");
		for (index = 0; index < nearest_count; ++index) {
			const Train* train = &trains[nearest_matches[index].train_index];
			write_csv_field(file, train->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA"); fputc(',', file);
			write_csv_field(file, train->car_label); fputc(',', file);
			write_csv_field(file, train->route_name); fputc(',', file);
			fprintf(file, "%.2f,", nearest_matches[index].distance);
			write_csv_field(file, train->destination); fputc('\n', file);
		}
	} else if (view_mode == VIEW_ALERTS) {
		fprintf(file, "Route,Effect,Alert\n");
		for (index = 0; index < alert_count; ++index) {
			write_csv_field(file, alert_route_id); fputc(',', file);
			write_csv_field(file, route_alerts[index].effect); fputc(',', file);
			write_csv_field(file, route_alerts[index].header); fputc('\n', file);
		}
	} else {
		fprintf(file, "Source,Train,Vehicle ID,Line,Destination,Current stop,Status,Latitude,Longitude,Speed mph,Updated at,Favorite\n");
		for (index = 0; index < (view_mode == VIEW_MAP ? train_count : ListView_GetItemCount(train_list)); ++index) {
			int train_index = index;
			const Train* train;
			LVITEMA item;
			if (view_mode != VIEW_MAP) {
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_PARAM;
				item.iItem = index;
				if (!ListView_GetItem(train_list, &item)) continue;
				train_index = (int)item.lParam;
			}
			train = &trains[train_index];
			write_csv_field(file, train->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA"); fputc(',', file);
			write_csv_field(file, train->car_label); fputc(',', file);
			write_csv_field(file, train->vehicle_id); fputc(',', file);
			write_csv_field(file, train->route_name); fputc(',', file);
			write_csv_field(file, train->destination); fputc(',', file);
			write_csv_field(file, train->current_stop); fputc(',', file);
			write_csv_field(file, train->current_status); fputc(',', file);
			if (train->has_location) fprintf(file, "%.6f,%.6f,", train->latitude, train->longitude);
			else fprintf(file, ",,");
			if (train->has_speed) fprintf(file, "%.1f,", train->speed_mph);
			else fputc(',', file);
			write_csv_field(file, train->updated_at); fputc(',', file);
			write_csv_field(file, is_favorite_train(train) ? "yes" : "no"); fputc('\n', file);
		}
	}
	fclose(file);
	SetWindowTextA(status_text, "CSV export complete.");
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_ERASEBKGND: {
		RECT client;
		if (view_mode == VIEW_MAP) return 1;
		GetClientRect(window, &client);
		FillRect((HDC)wparam, &client, workspace_brush);
		return 1;
	}

	case WM_PAINT: {
		PAINTSTRUCT paint;
		RECT client;
		RECT header;
		RECT accent;
		HDC device_context = BeginPaint(window, &paint);
		GpImage* displayed_logo = easter_egg_active && easter_logo_image != NULL ?
			easter_logo_image : logo_image;
		GetClientRect(window, &client);
		header.left = 0;
		header.top = 0;
		header.right = client.right;
		header.bottom = 72;
		FillRect(device_context, &header, header_brush);
		accent.left = 0;
		accent.top = 72;
		accent.right = client.right;
		accent.bottom = 76;
		FillRect(device_context, &accent, accent_brush);
		if (displayed_logo != NULL && gdip_create_from_hdc != NULL) {
			INT logo_left = easter_egg_active && easter_logo_image != NULL ? 12 : 18;
			INT logo_top = easter_egg_active && easter_logo_image != NULL ? 10 : 8;
			INT logo_width = easter_egg_active && easter_logo_image != NULL ? 68 : 56;
			INT logo_height = easter_egg_active && easter_logo_image != NULL ? 51 : 56;
			FLOAT center_x = 46.0f;
			FLOAT center_y = easter_egg_active && easter_logo_image != NULL ? 35.5f : 36.0f;
			GpGraphics* graphics = NULL;
			if (gdip_create_from_hdc(device_context, &graphics) == 0) {
				if (easter_egg_active) {
					gdip_translate_world(graphics, -center_x, -center_y, 1);
					gdip_rotate_world(graphics, easter_logo_angle, 1);
					gdip_translate_world(graphics, center_x, center_y, 1);
				}
				gdip_draw_image_rect(graphics, displayed_logo, logo_left, logo_top,
					logo_width, logo_height);
				gdip_delete_graphics(graphics);
			}
		}
		if (easter_egg_active) {
			double progress = fmod((double)(GetTickCount64() - easter_start_tick) / 1800.0, 2.0);
			int reverse = progress > 1.0;
			ULONGLONG elapsed = GetTickCount64() - easter_start_tick;
			if (reverse) progress = 2.0 - progress;
			draw_easter_train(device_context, client.right, progress, reverse);
		}
		if (view_mode == VIEW_MAP) {
			RECT map_background;
			map_background.left = 0;
			map_background.top = 130;
			map_background.right = client.right;
			map_background.bottom = client.bottom;
			if (ensure_map_backbuffer(device_context, client.right, client.bottom)) {
				FillRect(map_backbuffer_dc, &map_background, workspace_brush);
				draw_map_view(map_backbuffer_dc, client.right, client.bottom);
				BitBlt(device_context, 0, map_background.top, client.right,
					client.bottom - map_background.top, map_backbuffer_dc, 0, map_background.top, SRCCOPY);
			} else {
				draw_map_view(device_context, client.right, client.bottom);
			}
			if (map_data_dirty && InterlockedCompareExchange(&map_data_in_progress, 0, 0) == 0) {
				map_data_dirty = 0;
				begin_map_data_request(window, client.right, client.bottom, 1);
			}
		}
		if (easter_egg_active) {
			ULONGLONG elapsed = GetTickCount64() - easter_start_tick;
			ULONGLONG wall_hits = elapsed / 3000 +
				(ULONGLONG)((double)elapsed / 2100.0 + 0.31);
			int copy_count = wall_hits >= 31 ? 32 : (int)wall_hits + 1;
			draw_easter_word(device_context, client.right, client.bottom, elapsed, copy_count);
		}
		EndPaint(window, &paint);
		return 0;
	}
	case WM_TRAY_CALLBACK:
		if (lparam == WM_LBUTTONDBLCLK || lparam == NIN_BALLOONUSERCLICK) {
			ShowWindow(window, SW_RESTORE);
			SetForegroundWindow(window);
		}
		return 0;
	case MM_MCINOTIFY:
		if ((wparam == MCI_NOTIFY_SUCCESSFUL || wparam == MCI_NOTIFY_ABORTED ||
			wparam == MCI_NOTIFY_FAILURE) && easter_mp3_open) {
			mciSendStringA("close mbtaEggTrack", NULL, 0, NULL);
			easter_mp3_open = 0;
			finish_train_easter_egg(window);
			SetWindowTextA(status_text, wparam == MCI_NOTIFY_SUCCESSFUL ?
				"The train song clip finished." : "Train song playback stopped.");
		}
		return 0;
	case WM_EASTER_EGG_COMPLETE:
		finish_train_easter_egg(window);
		SetWindowTextA(status_text, "All aboard!");
		return 0;
	case WM_LBUTTONDOWN:
		if (view_mode == VIEW_MAP) {
			map_dragging = 1;
			map_drag_moved = 0;
			map_drag_start_x = GET_X_LPARAM(lparam);
			map_drag_start_y = GET_Y_LPARAM(lparam);
			map_geo_to_world(map_center_latitude, map_center_longitude, map_zoom,
				&map_drag_start_center_x, &map_drag_start_center_y);
			SetCapture(window);
			return 0;
		}
		break;
	case WM_MOUSEMOVE:
		if (view_mode == VIEW_MAP && map_dragging && (wparam & MK_LBUTTON)) {
			int delta_x = GET_X_LPARAM(lparam) - map_drag_start_x;
			int delta_y = GET_Y_LPARAM(lparam) - map_drag_start_y;
			double world_size = 256.0 * (double)(1 << map_zoom);
			double center_x = map_drag_start_center_x - delta_x;
			double center_y = map_drag_start_center_y - delta_y;
			if (abs(delta_x) > 2 || abs(delta_y) > 2) map_drag_moved = 1;
			if (center_x < 0) center_x += world_size;
			if (center_x >= world_size) center_x -= world_size;
			if (center_y < 0) center_y = 0;
			if (center_y > world_size) center_y = world_size;
			map_world_to_geo(center_x, center_y,
				map_zoom, &map_center_latitude, &map_center_longitude);
			{
				RECT map;
				GetClientRect(window, &map);
				get_map_rect(map.right, map.bottom, &map);
				InvalidateRect(window, &map, FALSE);
			}
			return 0;
		}
		break;
	case WM_LBUTTONUP:
		if (view_mode == VIEW_MAP && map_dragging) {
			RECT client;
			map_dragging = 0;
			ReleaseCapture();
			if (GetClientRect(window, &client)) {
				if (map_drag_moved) map_data_dirty = 1;
				else select_map_train(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), client.right, client.bottom);
				get_map_rect(client.right, client.bottom, &client);
				InvalidateRect(window, &client, FALSE);
			}
			return 0;
		}
		break;
	case WM_MOUSEWHEEL:
		if (view_mode == VIEW_MAP) {
			int delta = GET_WHEEL_DELTA_WPARAM(wparam);
			if (delta > 0 && map_zoom < 17) ++map_zoom;
			else if (delta < 0 && map_zoom > 5) --map_zoom;
			map_data_dirty = 1;
			InvalidateRect(window, NULL, FALSE);
			return 0;
		}
		break;
	case WM_CTLCOLORSTATIC: {
		HDC device_context = (HDC)wparam;
		HWND control = (HWND)lparam;
		if (control == title_control || control == subtitle_control) {
			SetTextColor(device_context, COLOR_WHITE);
			SetBkMode(device_context, TRANSPARENT);
			return (LRESULT)header_brush;
		}
		SetTextColor(device_context, COLOR_BRAND_NAVY);
		SetBkMode(device_context, TRANSPARENT);
		return (LRESULT)workspace_brush;
	}
	case WM_CTLCOLOREDIT: {
		HDC device_context = (HDC)wparam;
		SetTextColor(device_context, COLOR_BRAND_NAVY);
		SetBkColor(device_context, COLOR_WHITE);
		return (LRESULT)white_brush;
	}
	case WM_DRAWITEM: {
		const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lparam;
		if (item->CtlID == ID_TRAINS_VIEW || item->CtlID == ID_FAVORITES_VIEW || item->CtlID == ID_RADIO_VIEW ||
			item->CtlID == ID_MAP_VIEW ||
			item->CtlID == ID_RUN_COMMAND || item->CtlID == ID_WATCH || item->CtlID == ID_REFRESH ||
			item->CtlID == ID_SAVE_SEARCH || item->CtlID == ID_EXPORT || item->CtlID == ID_FAVORITE ||
			item->CtlID == ID_MAP_ZOOM_IN || item->CtlID == ID_MAP_ZOOM_OUT ||
			item->CtlID == ID_MAP_FULL_DETAILS ||
			item->CtlID == ID_SELECTED_ALERTS || item->CtlID == ID_NOTIFY ||
				item->CtlID == ID_NEAREST || item->CtlID == ID_DISTANCE) {
			draw_owner_button(item);
			return TRUE;
		}
		break;
	}
		case WM_CREATE: { 
			INITCOMMONCONTROLSEX controls;
			HWND title;
			HWND subtitle;
			main_window = window;
			initialize_tray_icon(window);
			controls.dwSize = sizeof(controls);
			controls.dwICC = ICC_LISTVIEW_CLASSES;
			InitCommonControlsEx(&controls);
			title = CreateWindowExA(0, "STATIC", "MBTA Train Tracker", WS_CHILD | WS_VISIBLE,
				92, 13, 500, 34, window, NULL, NULL, NULL);
			subtitle = CreateWindowExA(0, "STATIC", "Live commuter rail and Amtrak positions", WS_CHILD | WS_VISIBLE,
				92, 45, 500, 20, window, NULL, NULL, NULL);
			title_control = title;
			subtitle_control = subtitle;
			trains_view_button = CreateWindowExA(0, "BUTTON", "Trains",
				WS_CHILD | WS_VISIBLE | WS_GROUP | BS_OWNERDRAW,
				20, 77, 96, 28, window, (HMENU)(INT_PTR)ID_TRAINS_VIEW, NULL, NULL);
			favorites_view_button = CreateWindowExA(0, "BUTTON", "Favorites",
				WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				120, 77, 112, 28, window, (HMENU)(INT_PTR)ID_FAVORITES_VIEW, NULL, NULL);
			radio_view_button = CreateWindowExA(0, "BUTTON", "Radio Frequencies",
				WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				236, 77, 150, 28, window, (HMENU)(INT_PTR)ID_RADIO_VIEW, NULL, NULL);
			map_view_button = CreateWindowExA(0, "BUTTON", "Map",
				WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				392, 77, 78, 28, window, (HMENU)(INT_PTR)ID_MAP_VIEW, NULL, NULL);
			map_zoom_in_button = CreateWindowExA(0, "BUTTON", "+", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 32, 28, window, (HMENU)(INT_PTR)ID_MAP_ZOOM_IN, NULL, NULL);
			map_zoom_out_button = CreateWindowExA(0, "BUTTON", "-", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 32, 28, window, (HMENU)(INT_PTR)ID_MAP_ZOOM_OUT, NULL, NULL);
			map_overview_label = CreateWindowExA(0, "STATIC", "Select a train marker", WS_CHILD | WS_VISIBLE,
				0, 0, 330, 22, window, NULL, NULL, NULL);
			map_overview_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "Train overview will appear here.",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
				0, 0, 330, 86, window, (HMENU)(INT_PTR)ID_MAP_OVERVIEW, NULL, NULL);
			map_full_details_button = CreateWindowExA(0, "BUTTON", "Full details",
				WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 330, 28, window, (HMENU)(INT_PTR)ID_MAP_FULL_DETAILS, NULL, NULL);
			delayed_filter_button = CreateWindowExA(0, "BUTTON", "Known delayed",
				WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
				478, 80, 124, 24, window, (HMENU)(INT_PTR)ID_DELAYED_FILTER, NULL, NULL);
			filter_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
				WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 114, 500, 28, window,
				(HMENU)(INT_PTR)ID_FILTER, NULL, NULL);
			saved_search_combo = CreateWindowExA(0, "COMBOBOX", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
				0, 114, 160, 180, window, (HMENU)(INT_PTR)ID_SAVED_SEARCH, NULL, NULL);
			save_search_button = CreateWindowExA(0, "BUTTON", "Save search", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 114, 72, 28, window, (HMENU)(INT_PTR)ID_SAVE_SEARCH, NULL, NULL);
			run_button = CreateWindowExA(0, "BUTTON", "Run", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 114, 72, 28, window, (HMENU)(INT_PTR)ID_RUN_COMMAND, NULL, NULL);
			watch_button = CreateWindowExA(0, "BUTTON", "Watch: On", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 114, 84, 28, window, (HMENU)(INT_PTR)ID_WATCH, NULL, NULL);
			refresh_button = CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 114, 78, 28, window, (HMENU)(INT_PTR)ID_REFRESH, NULL, NULL);
			export_button = CreateWindowExA(0, "BUTTON", "Export CSV", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 114, 70, 28, window, (HMENU)(INT_PTR)ID_EXPORT, NULL, NULL);
			favorite_button = CreateWindowExA(0, "BUTTON", "Favorite", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 82, 24, window, (HMENU)(INT_PTR)ID_FAVORITE, NULL, NULL);
			selected_alerts_button = CreateWindowExA(0, "BUTTON", "Alerts", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 72, 24, window, (HMENU)(INT_PTR)ID_SELECTED_ALERTS, NULL, NULL);
			notify_button = CreateWindowExA(0, "BUTTON", "Notify", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 84, 24, window, (HMENU)(INT_PTR)ID_NOTIFY, NULL, NULL);
			nearest_button = CreateWindowExA(0, "BUTTON", "Nearest", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 62, 24, window, (HMENU)(INT_PTR)ID_NEAREST, NULL, NULL);
			distance_button = CreateWindowExA(0, "BUTTON", "Distance", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				0, 0, 56, 24, window, (HMENU)(INT_PTR)ID_DISTANCE, NULL, NULL);
			train_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
				20, 153, 700, 400, window, (HMENU)(INT_PTR)ID_TRAIN_LIST, NULL, NULL);
			detail_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
				20, 0, 500, 200, window, (HMENU)(INT_PTR)ID_DETAIL_LIST, NULL, NULL);
			stops_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
				0, 0, 500, 200, window, (HMENU)(INT_PTR)ID_STOPS_LIST, NULL, NULL);
			stops_label = CreateWindowExA(0, "STATIC", "Upcoming stops and times", WS_CHILD | WS_VISIBLE,
				0, 0, 500, 22, window, NULL, NULL, NULL);
			ListView_SetExtendedListViewStyle(train_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
			ListView_SetBkColor(train_list, COLOR_WHITE);
			ListView_SetTextBkColor(train_list, COLOR_WHITE);
			ListView_SetTextColor(train_list, COLOR_BRAND_NAVY);
			ListView_SetExtendedListViewStyle(detail_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
			ListView_SetExtendedListViewStyle(stops_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
			ListView_SetBkColor(detail_list, COLOR_WHITE);
			ListView_SetTextBkColor(detail_list, COLOR_WHITE);
			ListView_SetTextColor(detail_list, COLOR_BRAND_NAVY);
			ListView_SetBkColor(stops_list, COLOR_WHITE);
			ListView_SetTextBkColor(stops_list, COLOR_WHITE);
			ListView_SetTextColor(stops_list, COLOR_BRAND_NAVY);
			add_table_column(detail_list, "Field", 145, 0);
			add_table_column(detail_list, "Value", 320, 1);
			add_table_column(stops_list, "Upcoming stop", 180, 0);
			add_table_column(stops_list, "Arrival", 110, 1);
			add_table_column(stops_list, "Departure", 110, 2);
			add_table_column(stops_list, "Delay", 95, 3);
			{
				details_label = CreateWindowExA(0, "STATIC", "Selected train", WS_CHILD | WS_VISIBLE,
					20, 0, 180, 20, window, NULL, NULL, NULL);
				details_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "Select a train to see its details.",
					WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
					20, 0, 700, 105, window, (HMENU)(INT_PTR)ID_DETAILS, NULL, NULL);
				status_text = CreateWindowExA(0, "STATIC", "Loading train positions...", WS_CHILD | WS_VISIBLE,
					20, 0, 500, 20, window, (HMENU)(INT_PTR)ID_STATUS, NULL, NULL);
				set_control_font(details_label);
			}
			title_font = CreateFontA(23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
				DEFAULT_PITCH | FF_SWISS, "Segoe UI");
			easter_font = CreateFontA(23, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, "Comic Sans MS");
			subtitle_font = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
				DEFAULT_PITCH | FF_SWISS, "Segoe UI");
			if (title_font != NULL) SendMessageA(title, WM_SETFONT, (WPARAM)title_font, TRUE);
			if (subtitle_font != NULL) SendMessageA(subtitle, WM_SETFONT, (WPARAM)subtitle_font, TRUE);
			set_control_font(trains_view_button);
			set_control_font(favorites_view_button);
			set_control_font(radio_view_button);
			set_control_font(map_view_button);
			set_control_font(map_overview_label);
			set_control_font(map_overview_edit);
			set_control_font(map_full_details_button);
			set_control_font(map_zoom_in_button);
			set_control_font(map_zoom_out_button);
			set_control_font(map_overview_label);
			set_control_font(map_overview_edit);
			set_control_font(map_full_details_button);
			set_control_font(delayed_filter_button);
			set_control_font(filter_edit);
			set_control_font(saved_search_combo);
			set_control_font(save_search_button);
			set_control_font(run_button);
			set_control_font(watch_button);
			set_control_font(refresh_button);
			set_control_font(export_button);
			set_control_font(favorite_button);
			set_control_font(selected_alerts_button);
			set_control_font(notify_button);
			set_control_font(nearest_button);
			set_control_font(distance_button);
			set_control_font(train_list);
			set_control_font(detail_list);
			set_control_font(stops_list);
			set_control_font(stops_label);
			set_control_font(details_label);
			set_control_font(details_edit);
			set_control_font(status_text);
			set_view_mode(VIEW_TRAINS);
			refresh_saved_search_combo();
			SetTimer(window, 1, 30000, NULL);
			begin_refresh();
			return 0;
		}
		case WM_SIZE:
			if (view_mode == VIEW_MAP) map_data_dirty = 1;
			layout_controls(LOWORD(lparam), HIWORD(lparam));
			return 0;
		case WM_COMMAND:
			if (LOWORD(wparam) == ID_REFRESH && HIWORD(wparam) == BN_CLICKED) {
				begin_refresh();
				return 0;
			}
			if (LOWORD(wparam) == ID_FILTER && HIWORD(wparam) == EN_CHANGE) {
				char text[256];
				GetWindowTextA(filter_edit, text, (int)sizeof(text));
				if ((view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES || view_mode == VIEW_RADIO) &&
					!is_action_command(text)) {
					refresh_visible_list();
					show_selected_train();
				}
				return 0;
			}
			if (LOWORD(wparam) == ID_SAVED_SEARCH && HIWORD(wparam) == CBN_SELCHANGE) {
				int selected = (int)SendMessageA(saved_search_combo, CB_GETCURSEL, 0, 0);
				if (selected > 0 && selected <= saved_search_count) {
					set_view_mode(VIEW_TRAINS);
					SetWindowTextA(filter_edit, saved_searches[selected - 1]);
					SetFocus(filter_edit);
				}
				return 0;
			}
			if (LOWORD(wparam) == ID_SAVE_SEARCH && HIWORD(wparam) == BN_CLICKED) {
				save_current_search();
				return 0;
			}
			if (LOWORD(wparam) == ID_EXPORT && HIWORD(wparam) == BN_CLICKED) {
				export_csv();
				return 0;
			}
			if (LOWORD(wparam) == ID_DELAYED_FILTER && HIWORD(wparam) == BN_CLICKED) {
				delayed_only = IsDlgButtonChecked(window, ID_DELAYED_FILTER) == BST_CHECKED;
				refresh_visible_list();
				show_selected_train();
				if (view_mode == VIEW_MAP) InvalidateRect(window, NULL, TRUE);
				return 0;
			}
			if (LOWORD(wparam) == ID_RUN_COMMAND && HIWORD(wparam) == BN_CLICKED) {
				execute_command();
				return 0;
			}
			if (LOWORD(wparam) == ID_WATCH && HIWORD(wparam) == BN_CLICKED) {
				toggle_watch();
				return 0;
			}
			if (LOWORD(wparam) == ID_TRAINS_VIEW && HIWORD(wparam) == BN_CLICKED) {
				set_view_mode(VIEW_TRAINS);
				SetWindowTextA(filter_edit, "");
				return 0;
			}
			if (LOWORD(wparam) == ID_FAVORITES_VIEW && HIWORD(wparam) == BN_CLICKED) {
				set_view_mode(VIEW_FAVORITES);
				SetWindowTextA(filter_edit, "");
				return 0;
			}
			if (LOWORD(wparam) == ID_RADIO_VIEW && HIWORD(wparam) == BN_CLICKED) {
				set_view_mode(VIEW_RADIO);
				SetWindowTextA(filter_edit, "");
				return 0;
			}
			if (LOWORD(wparam) == ID_MAP_VIEW && HIWORD(wparam) == BN_CLICKED) {
				set_view_mode(VIEW_MAP);
				InvalidateRect(window, NULL, TRUE);
				return 0;
			}
			if (LOWORD(wparam) == ID_MAP_FULL_DETAILS && HIWORD(wparam) == BN_CLICKED) {
				if (map_selected_train_index >= 0) begin_train_details(map_selected_train_index, 1);
				return 0;
			}
			if ((LOWORD(wparam) == ID_MAP_ZOOM_IN || LOWORD(wparam) == ID_MAP_ZOOM_OUT) &&
				HIWORD(wparam) == BN_CLICKED) {
				if (LOWORD(wparam) == ID_MAP_ZOOM_IN && map_zoom < 17) ++map_zoom;
				if (LOWORD(wparam) == ID_MAP_ZOOM_OUT && map_zoom > 5) --map_zoom;
				map_data_dirty = 1;
				InvalidateRect(window, NULL, TRUE);
				return 0;
			}
			if (LOWORD(wparam) == ID_FAVORITE && HIWORD(wparam) == BN_CLICKED) {
				toggle_selected_favorite();
				return 0;
			}
			if (LOWORD(wparam) == ID_SELECTED_ALERTS && HIWORD(wparam) == BN_CLICKED) {
				int train_index = selected_train_index();
				if (train_index < 0) SetWindowTextA(status_text, "Select a train first.");
				else if (trains[train_index].source == TRAIN_SOURCE_AMTRAK) {
					SetWindowTextA(status_text, "Live service alerts are available for MBTA commuter rail only.");
				} else begin_alerts(trains[train_index].route_id);
				return 0;
			}
			if (LOWORD(wparam) == ID_NOTIFY && HIWORD(wparam) == BN_CLICKED) {
				int train_index = selected_train_index();
				int alert_index;
				char command[128];
				if (train_index < 0) SetWindowTextA(status_text, "Select a train first.");
				else {
					for (alert_index = 0; alert_index < station_alert_count; ++alert_index) {
						if (strcmp(station_alerts[alert_index].vehicle_id, trains[train_index].vehicle_id) == 0) break;
					}
					if (alert_index < station_alert_count) {
						remove_station_alert(trains[train_index].vehicle_id);
						update_favorite_button();
						return 0;
					}
					snprintf(command, sizeof(command), "notify %s ", trains[train_index].vehicle_id);
					SetWindowTextA(filter_edit, command);
					SetFocus(filter_edit);
					SendMessageA(filter_edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
					SetWindowTextA(status_text, "Add a station or latitude,longitude, then press Run.");
				}
				return 0;
			}
			if (LOWORD(wparam) == ID_NEAREST && HIWORD(wparam) == BN_CLICKED) {
				SetWindowTextA(filter_edit, "nearest ");
				SetFocus(filter_edit);
				SendMessageA(filter_edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
				SetWindowTextA(status_text, "Enter a station or latitude,longitude, then press Run.");
				return 0;
			}
			if (LOWORD(wparam) == ID_DISTANCE && HIWORD(wparam) == BN_CLICKED) {
				int train_index = selected_train_index();
				char command[128];
				if (train_index < 0) SetWindowTextA(status_text, "Select a train first.");
				else {
					snprintf(command, sizeof(command), "distance %s ", trains[train_index].vehicle_id);
					SetWindowTextA(filter_edit, command);
					SetFocus(filter_edit);
					SendMessageA(filter_edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
					SetWindowTextA(status_text, "Enter a station or latitude,longitude, then press Run.");
				}
				return 0;
			}
			break;
		case WM_NOTIFY:
			if (((LPNMHDR)lparam)->idFrom == ID_TRAIN_LIST &&
				((LPNMHDR)lparam)->code == LVN_COLUMNCLICK) {
				NMLISTVIEW* clicked = (NMLISTVIEW*)lparam;
				if (sort_column == clicked->iSubItem) sort_ascending = !sort_ascending;
				else {
					sort_column = clicked->iSubItem;
					sort_ascending = 1;
				}
				ListView_SortItems(train_list, compare_main_rows, 0);
				update_sort_indicator(train_list, sort_column, sort_ascending);
				return 0;
			}
			if (((LPNMHDR)lparam)->idFrom == ID_STOPS_LIST &&
				((LPNMHDR)lparam)->code == LVN_COLUMNCLICK) {
				NMLISTVIEW* clicked = (NMLISTVIEW*)lparam;
				if (stop_sort_column == clicked->iSubItem) stop_sort_ascending = !stop_sort_ascending;
				else {
					stop_sort_column = clicked->iSubItem;
					stop_sort_ascending = 1;
				}
				ListView_SortItems(stops_list, compare_stop_rows, 0);
				update_sort_indicator(stops_list, stop_sort_column, stop_sort_ascending);
				return 0;
			}
			if (((LPNMHDR)lparam)->idFrom == ID_TRAIN_LIST &&
				((LPNMHDR)lparam)->code == LVN_ITEMCHANGED) {
				show_selected_train();
			}
			break;
		case WM_TIMER:
			if (wparam == 1 && watch_enabled) begin_refresh();
			if (wparam == EASTER_EGG_TIMER && easter_egg_active) {
				ULONGLONG elapsed = GetTickCount64() - easter_start_tick;
				{
					double phase = (double)elapsed / 1800.0 * 6.283185307179586;
					int offset_x = (int)(24.0 * sin(phase));
					int offset_y = (int)(18.0 * cos(phase));
					easter_logo_angle = (FLOAT)fmod((double)elapsed / 5.0, 360.0);
					SetWindowPos(window, NULL, easter_original_rect.left + offset_x,
						easter_original_rect.top + offset_y, 0, 0,
						SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
					InvalidateRect(window, NULL, TRUE);
				}
				return 0;
			}
			return 0;
		case WM_REFRESH_COMPLETE: {
			RefreshResult* result = (RefreshResult*)lparam;
			InterlockedExchange(&refresh_in_progress, 0);
			SetWindowTextA(refresh_button, "Refresh");
			EnableWindow(refresh_button, TRUE);
			if (result == NULL) {
				SetWindowTextA(status_text, "Refresh failed: not enough memory.");
				return 0;
			}
			if (result->count < 0) {
				char message[320];
				snprintf(message, sizeof(message), "Refresh failed: %s", result->error[0] ? result->error : "unknown error");
				SetWindowTextA(status_text, message);
			} else {
				char message[320];
				SYSTEMTIME now;
				train_count = result->count;
				memcpy(trains, result->trains, sizeof(Train) * train_count);
				update_speed_estimates(trains, train_count);
				selected_details_vehicle[0] = '\0';
				if (view_mode == VIEW_ROUTES) build_route_summaries();
				if (view_mode == VIEW_NEAREST) build_nearest_matches(nearest_latitude, nearest_longitude);
				refresh_visible_list();
				show_selected_train();
				GetLocalTime(&now);
				snprintf(message, sizeof(message), "Feed updated at %d:%02d:%02d %s. %d active train(s).",
					now.wHour % 12 == 0 ? 12 : now.wHour % 12, now.wMinute, now.wSecond,
					now.wHour < 12 ? "AM" : "PM", train_count);
				SetWindowTextA(status_text, message);
				if (result->error[0]) {
					snprintf(message, sizeof(message), "Feed updated at %d:%02d %s. %d active train(s). Warning: %s",
						now.wHour % 12 == 0 ? 12 : now.wHour % 12, now.wMinute,
						now.wHour < 12 ? "AM" : "PM", train_count, result->error);
					SetWindowTextA(status_text, message);
				}
				check_station_alerts();
				if (view_mode == VIEW_MAP) InvalidateRect(window, NULL, TRUE);
			}
			HeapFree(GetProcessHeap(), 0, result);
			return 0;
		}
		case WM_MAP_DATA_COMPLETE: {
			MapDataResult* result = (MapDataResult*)lparam;
			InterlockedExchange(&map_data_in_progress, 0);
			if (result == NULL) {
				SetWindowTextA(status_text, "Map data could not be loaded.");
				return 0;
			}
			if (result->shape_count >= 0) {
				map_route_line_count = result->shape_count;
				if (map_route_line_count > 0) {
					memcpy(map_route_lines, result->lines, sizeof(MapRouteLine) * map_route_line_count);
				}
				map_shapes_loaded = result->error[0] == '\0';
			}
			if (view_mode == VIEW_MAP) {
				if (result->error[0]) {
					SetWindowTextA(status_text, "Street tiles may still load; MBTA route lines are unavailable right now.");
				} else if (result->tiles_loaded > 0 || result->shape_count >= 0) {
					SetWindowTextA(status_text, "Map tiles and MBTA route lines updated.");
				}
				InvalidateRect(window, NULL, TRUE);
			}
			HeapFree(GetProcessHeap(), 0, result);
			return 0;
		}
		case WM_ALERTS_COMPLETE: {
			AlertsResult* result = (AlertsResult*)lparam;
			InterlockedExchange(&alerts_in_progress, 0);
			EnableWindow(run_button, TRUE);
			if (result == NULL) {
				SetWindowTextA(status_text, "Alerts unavailable: not enough memory.");
				return 0;
			}
			if (result->count < 0) {
				char message[320];
				snprintf(message, sizeof(message), "Alerts unavailable: %s", result->error[0] ? result->error : "unknown error");
				SetWindowTextA(status_text, message);
			} else {
				alert_count = result->count;
				memcpy(route_alerts, result->alerts, sizeof(RouteAlert) * alert_count);
				snprintf(alert_route_id, sizeof(alert_route_id), "%s", result->route_id);
				set_view_mode(VIEW_ALERTS);
				if (alert_count == 0) {
					char message[160];
					snprintf(message, sizeof(message), "No active alerts reported for route %s.", alert_route_id);
					SetWindowTextA(details_edit, message);
				}
			}
			HeapFree(GetProcessHeap(), 0, result);
			return 0;
		}
		case WM_DETAILS_COMPLETE: {
			DetailsResult* result = (DetailsResult*)lparam;
			InterlockedExchange(&details_in_progress, 0);
			EnableWindow(run_button, TRUE);
			if (result == NULL) {
				SetWindowTextA(status_text, "Could not load train predictions: not enough memory.");
				return 0;
			}
			if (result->full_view && view_mode == VIEW_DETAIL) {
				SetWindowTextA(details_edit, result->text);
				SetWindowTextA(status_text, "Full train details loaded.");
			} else if (!result->full_view && view_mode == VIEW_MAP) {
				if (map_selected_train_index >= 0 &&
					strcmp(trains[map_selected_train_index].vehicle_id, result->vehicle_id) == 0) {
					Train* selected_train = &trains[map_selected_train_index];
					selected_train->upcoming_stop_count = result->stop_count;
					memcpy(selected_train->upcoming_stops, result->stops,
						sizeof(TrainStopPrediction) * result->stop_count);
					if (result->stop_count > 0 && result->stops[0].has_delay) {
						char trend[96];
						record_delay_sample(result->vehicle_id, result->stops[0].delay_minutes, trend, sizeof(trend));
					}
					update_map_overview(map_selected_train_index);
					SetWindowTextA(status_text, result->stop_count > 0 ?
						"Map overview updated with arrival predictions." :
						"Map overview updated; the feed has no upcoming predictions.");
					InvalidateRect(window, NULL, FALSE);
				} else if (map_selected_train_index >= 0) {
					begin_train_details(map_selected_train_index, 0);
				}
			} else if (!result->full_view && (view_mode == VIEW_TRAINS || view_mode == VIEW_FAVORITES)) {
				int selected = ListView_GetNextItem(train_list, -1, LVNI_SELECTED);
				LVITEMA item;
				memset(&item, 0, sizeof(item));
				item.mask = LVIF_PARAM;
				item.iItem = selected;
				if (selected >= 0 && ListView_GetItem(train_list, &item) &&
					strcmp(trains[(int)item.lParam].vehicle_id, result->vehicle_id) == 0) {
				Train* selected_train = &trains[(int)item.lParam];
					char trend[96] = "No delay history yet";
					selected_train->upcoming_stop_count = result->stop_count;
					memcpy(selected_train->upcoming_stops, result->stops,
						sizeof(TrainStopPrediction) * result->stop_count);
					show_upcoming_stops(result->stops, result->stop_count);
					if (result->stop_count > 0) {
						char delay[32];
						format_delay_cell(&result->stops[0], delay, sizeof(delay));
						add_detail_pair(10, "Next stop delay", delay);
						if (result->stops[0].has_delay) {
							record_delay_sample(result->vehicle_id, result->stops[0].delay_minutes, trend, sizeof(trend));
						}
					} else {
						add_detail_pair(10, "Next stop delay", "Not reported by feed");
					}
					add_detail_pair(12, "Delay trend", trend);
					if (delayed_only) refresh_visible_list();
					SetWindowTextA(status_text, result->stop_count > 0 ?
						"Upcoming stop predictions loaded." :
						"No upcoming predictions reported; check the feed update time.");
				} else {
					selected_details_vehicle[0] = '\0';
					show_selected_train();
				}
			}
			HeapFree(GetProcessHeap(), 0, result);
			return 0;
		}
		case WM_DESTROY:
		{
			int tile_index;
			KillTimer(window, 1);
			dispose_map_backbuffer();
			for (tile_index = 0; tile_index < MAX_CACHED_MAP_TILES; ++tile_index) {
				if (map_tile_cache[tile_index].image != NULL) {
					gdip_dispose_image(map_tile_cache[tile_index].image);
					map_tile_cache[tile_index].image = NULL;
				}
			}
			remove_tray_icon();
			if (easter_mp3_open) {
				mciSendStringA("stop mbtaEggTrack", NULL, 0, NULL);
				mciSendStringA("close mbtaEggTrack", NULL, 0, NULL);
				easter_mp3_open = 0;
			}
			if (title_font != NULL) DeleteObject(title_font);
			if (easter_font != NULL) DeleteObject(easter_font);
			if (subtitle_font != NULL) DeleteObject(subtitle_font);
			if (workspace_brush != NULL) DeleteObject(workspace_brush);
			if (white_brush != NULL) DeleteObject(white_brush);
			if (header_brush != NULL) DeleteObject(header_brush);
			if (accent_brush != NULL) DeleteObject(accent_brush);
			PostQuitMessage(0);
			return 0;
		}
		}
		return DefWindowProcA(window, message, wparam, lparam);
	}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command)
{
	WNDCLASSEXA window_class;
	HWND window;
	MSG message;
	(void)previous;
	(void)command_line;
	initialize_settings();
	workspace_brush = CreateSolidBrush(COLOR_WORKSPACE);
	white_brush = CreateSolidBrush(COLOR_WHITE);
	header_brush = CreateSolidBrush(COLOR_BRAND_NAVY);
	accent_brush = CreateSolidBrush(COLOR_ACCENT_BLUE);
	initialize_logo();
	memset(&window_class, 0, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = window_proc;
	window_class.hInstance = instance;
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	window_class.hbrBackground = workspace_brush;
	window_class.lpszClassName = "MBTATrainTrackerWindow";
	if (!RegisterClassExA(&window_class)) return 1;
	window = CreateWindowExA(0, window_class.lpszClassName, "MBTA Train Tracker",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760,
		NULL, NULL, instance, NULL);
	if (window == NULL) return 1;
	main_window = window;
	ShowWindow(window, show_command);
	UpdateWindow(window);
	while (GetMessageA(&message, NULL, 0, 0) > 0) {
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}
	if (logo_image != NULL && gdip_dispose_image != NULL) gdip_dispose_image(logo_image);
	if (easter_logo_image != NULL && gdip_dispose_image != NULL) gdip_dispose_image(easter_logo_image);
	if (gdiplus_token != 0 && gdiplus_shutdown != NULL) gdiplus_shutdown(gdiplus_token);
	if (gdiplus_module != NULL) FreeLibrary(gdiplus_module);
	return (int)message.wParam;
}