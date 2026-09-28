#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include "TrainTracker.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define JSON_TOKEN_LIMIT 200000
#define EARTH_RADIUS_MILES 3958.8
#define PI_CONSTANT 3.14159265358979323846

typedef enum JsonType {
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_PRIMITIVE
} JsonType;

typedef struct JsonToken {
	JsonType type;
	size_t start;
	size_t end;
	int next;
} JsonToken;

typedef struct JsonParser {
	const char* text;
	size_t length;
	size_t position;
	JsonToken* tokens;
	int count;
} JsonParser;

static void set_error(char* error, size_t error_size, const char* message)
{
	if (error != NULL && error_size > 0) {
		snprintf(error, error_size, "%s", message);
	}
}

static double timestamp_to_minutes(const char* timestamp)
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	if (timestamp == NULL || timestamp[0] == '\0') return -1.0;
	if (sscanf_s(timestamp, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day,
		&hour, &minute, &second) != 6) {
		return -1.0;
	}
	return ((double)(year * 372 + month * 31 + day)) * 1440.0 + hour * 60.0 + minute + second / 60.0;
}

typedef struct Station {
	const char* name;
	double latitude;
	double longitude;
} Station;

/* Approximate city/station coordinates for distance estimates only, covering the cities and
   towns served by the MBTA commuter rail lines and Amtrak New England routes this app tracks. */
static const Station known_stations[] = {
	{"Boston South Station", 42.3519, -71.0552},
	{"Boston Back Bay", 42.3477, -71.0754},
	{"Boston North Station", 42.3654, -71.0623},
	{"Boston, MA", 42.3601, -71.0589},
	{"Cambridge, MA", 42.3736, -71.1097},
	{"Somerville, MA", 42.3876, -71.0995},
	{"Quincy, MA", 42.2529, -71.0023},
	{"Braintree, MA", 42.2079, -71.0011},
	{"Brockton, MA", 42.0834, -71.0184},
	{"Middleborough, MA", 41.8934, -70.9161},
	{"Plymouth, MA", 41.9584, -70.6673},
	{"Kingston, MA", 41.9959, -70.7273},
	{"Weymouth, MA", 42.2181, -70.9395},
	{"Hingham, MA", 42.2418, -70.8898},
	{"Scituate, MA", 42.1959, -70.7245},
	{"New Bedford, MA", 41.6362, -70.9342},
	{"Fall River, MA", 41.7015, -71.1550},
	{"Attleboro, MA", 41.9445, -71.2856},
	{"Mansfield, MA", 42.0334, -71.2184},
	{"Foxborough, MA", 42.0654, -71.2478},
	{"Franklin, MA", 42.0834, -71.3967},
	{"Norwood, MA", 42.1946, -71.1995},
	{"Walpole, MA", 42.1418, -71.2495},
	{"Dedham, MA", 42.2436, -71.1663},
	{"Needham, MA", 42.2807, -71.2377},
	{"Stoughton, MA", 42.1246, -71.1017},
	{"Canton, MA", 42.1584, -71.1465},
	{"Providence, RI", 41.8240, -71.4128},
	{"Warwick, RI", 41.7326, -71.4295},
	{"Wickford Junction, RI", 41.5762, -71.4531},
	{"Worcester, MA", 42.2626, -71.7997},
	{"Westborough, MA", 42.2695, -71.6132},
	{"Framingham, MA", 42.2793, -71.4162},
	{"Natick, MA", 42.2834, -71.3495},
	{"Ashland, MA", 42.2612, -71.4620},
	{"Fitchburg, MA", 42.5834, -71.8022},
	{"Leominster, MA", 42.5251, -71.7598},
	{"Ayer, MA", 42.5606, -71.5898},
	{"Concord, MA", 42.4603, -71.3489},
	{"Lowell, MA", 42.6334, -71.3162},
	{"Andover, MA", 42.6584, -71.1367},
	{"Lawrence, MA", 42.7070, -71.1631},
	{"Haverhill, MA", 42.7762, -71.0773},
	{"Newburyport, MA", 42.8095, -70.8967},
	{"Ipswich, MA", 42.6793, -70.8412},
	{"Gloucester, MA", 42.6159, -70.6620},
	{"Beverly, MA", 42.5584, -70.8800},
	{"Salem, MA", 42.5237, -70.8967},
	{"Lynn, MA", 42.4668, -70.9495},
	{"Woburn, MA", 42.4793, -71.1523},
	{"Winchester, MA", 42.4526, -71.1370},
	{"Malden, MA", 42.4251, -71.0662},
	{"Reading, MA", 42.5256, -71.0954},
	{"Wilmington, MA", 42.5462, -71.1729},
	{"Springfield, MA", 42.1155, -72.5900},
	{"Hartford, CT", 41.7637, -72.6851},
	{"New Haven, CT", 41.2967, -72.9265},
	{"New London, CT", 41.3557, -72.0995},
	{"Mystic, CT", 41.3543, -71.9636},
	{"Old Saybrook, CT", 41.2918, -72.3773},
	{"Kingston, RI", 41.4779, -71.5495},
	{"Portland, ME", 43.6746, -70.2929},
	{"New York, NY", 40.7506, -73.9935},
	{"Route 128, MA", 42.2088, -71.1473}
};

double haversine_miles(double lat1, double lon1, double lat2, double lon2)
{
	double delta_lat = (lat2 - lat1) * PI_CONSTANT / 180.0;
	double delta_lon = (lon2 - lon1) * PI_CONSTANT / 180.0;
	double a = sin(delta_lat / 2) * sin(delta_lat / 2) +
		cos(lat1 * PI_CONSTANT / 180.0) * cos(lat2 * PI_CONSTANT / 180.0) *
		sin(delta_lon / 2) * sin(delta_lon / 2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	return EARTH_RADIUS_MILES * c;
}

int find_station(const char* text, double* latitude, double* longitude)
{
	size_t index;
	size_t needle_length = strlen(text);
	if (needle_length == 0) return 0;
	for (index = 0; index < sizeof(known_stations) / sizeof(known_stations[0]); ++index) {
		size_t search_index;
		const char* name = known_stations[index].name;
		for (search_index = 0; name[search_index] != '\0'; ++search_index) {
			if (_strnicmp(name + search_index, text, needle_length) == 0) {
				*latitude = known_stations[index].latitude;
				*longitude = known_stations[index].longitude;
				return 1;
			}
		}
	}
	return 0;
}

#define SPEED_HISTORY_SIZE 500

typedef struct SpeedRecord {
	char vehicle_id[64];
	double latitude;
	double longitude;
	unsigned long long ticks;
	int valid;
} SpeedRecord;

static SpeedRecord speed_history[SPEED_HISTORY_SIZE];

void update_speed_estimates(Train* trains, int count)
{
	int index;
	unsigned long long now = GetTickCount64();
	for (index = 0; index < count; ++index) {
		int slot;
		int free_slot = -1;
		if (!trains[index].has_location) continue;
		for (slot = 0; slot < SPEED_HISTORY_SIZE; ++slot) {
			if (!speed_history[slot].valid) {
				if (free_slot < 0) free_slot = slot;
				continue;
			}
			if (strcmp(speed_history[slot].vehicle_id, trains[index].vehicle_id) == 0) {
				double elapsed_hours = (double)(now - speed_history[slot].ticks) / 3600000.0;
				if (elapsed_hours > 0.0002) {
					double distance = haversine_miles(speed_history[slot].latitude, speed_history[slot].longitude,
						trains[index].latitude, trains[index].longitude);
					trains[index].speed_mph = distance / elapsed_hours;
					trains[index].has_speed = 1;
				}
				speed_history[slot].latitude = trains[index].latitude;
				speed_history[slot].longitude = trains[index].longitude;
				speed_history[slot].ticks = now;
				free_slot = -1;
				break;
			}
		}
		if (free_slot >= 0) {
			snprintf(speed_history[free_slot].vehicle_id, sizeof(speed_history[free_slot].vehicle_id),
				"%s", trains[index].vehicle_id);
			speed_history[free_slot].latitude = trains[index].latitude;
			speed_history[free_slot].longitude = trains[index].longitude;
			speed_history[free_slot].ticks = now;
			speed_history[free_slot].valid = 1;
		}
	}
}

static void format_timestamp(const char* timestamp, char* output, size_t output_size)
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int hour_12;
	int offset_hour;
	int offset_minute;
	char meridiem[3];
	char timezone[16] = "";
	const char* zone;

	if (output_size == 0) return;
	if (timestamp == NULL || timestamp[0] == '\0') {
		output[0] = '\0';
		return;
	}
	if (sscanf_s(timestamp, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day,
		&hour, &minute, &second) != 6 || hour < 0 || hour > 23 ||
		minute < 0 || minute > 59 || second < 0 || second > 60) {
		snprintf(output, output_size, "%s", timestamp);
		return;
	}

	zone = timestamp + 19;
	while (*zone != '\0' && *zone != 'Z' && *zone != '+' && *zone != '-') ++zone;
	if (*zone == 'Z') {
		snprintf(timezone, sizeof(timezone), "UTC");
	} else if ((*zone == '+' || *zone == '-') &&
		sscanf_s(zone + 1, "%2d:%2d", &offset_hour, &offset_minute) == 2) {
		snprintf(timezone, sizeof(timezone), "UTC%c%02d:%02d", *zone, offset_hour, offset_minute);
	}

	hour_12 = hour % 12;
	if (hour_12 == 0) hour_12 = 12;
	snprintf(meridiem, sizeof(meridiem), "%s", hour < 12 ? "AM" : "PM");
	snprintf(output, output_size, "%04d-%02d-%02d %d:%02d:%02d %s%s%s",
		year, month, day, hour_12, minute, second, meridiem,
		timezone[0] == '\0' ? "" : " ", timezone);
}

static void skip_space(JsonParser* parser)
{
	while (parser->position < parser->length &&
		isspace((unsigned char)parser->text[parser->position])) {
		++parser->position;
	}
}

static int parse_value(JsonParser* parser)
{
	int token_index;
	char marker;

	skip_space(parser);
	if (parser->position >= parser->length || parser->count >= JSON_TOKEN_LIMIT) {
		return -1;
	}

	token_index = parser->count++;
	marker = parser->text[parser->position];
	if (marker == '{' || marker == '[') {
		JsonType type = marker == '{' ? JSON_OBJECT : JSON_ARRAY;
		char closing = marker == '{' ? '}' : ']';
		parser->tokens[token_index].type = type;
		parser->tokens[token_index].start = ++parser->position;
		for (;;) {
			skip_space(parser);
			if (parser->position >= parser->length) {
				return -1;
			}
			if (parser->text[parser->position] == closing) {
				parser->tokens[token_index].end = parser->position++;
				break;
			}
			if (parser->text[parser->position] == ',' || parser->text[parser->position] == ':') {
				++parser->position;
				continue;
			}
			if (parse_value(parser) < 0) {
				return -1;
			}
		}
	} else if (marker == '"') {
		parser->tokens[token_index].type = JSON_STRING;
		parser->tokens[token_index].start = ++parser->position;
		while (parser->position < parser->length) {
			char current = parser->text[parser->position++];
			if (current == '\\' && parser->position < parser->length) {
				++parser->position;
			} else if (current == '"') {
				parser->tokens[token_index].end = parser->position - 1;
				break;
			}
		}
		if (parser->position > parser->length ||
			(parser->position == parser->length && parser->text[parser->position - 1] != '"')) {
			return -1;
		}
	} else {
		parser->tokens[token_index].type = JSON_PRIMITIVE;
		parser->tokens[token_index].start = parser->position;
		while (parser->position < parser->length &&
			parser->text[parser->position] != ',' &&
			parser->text[parser->position] != ']' &&
			parser->text[parser->position] != '}' &&
			!isspace((unsigned char)parser->text[parser->position])) {
			++parser->position;
		}
		parser->tokens[token_index].end = parser->position;
	}
	parser->tokens[token_index].next = parser->count;
	return token_index;
}

static int token_equals(const char* text, const JsonToken* token, const char* value)
{
	size_t length = strlen(value);
	return token->end - token->start == length &&
		memcmp(text + token->start, value, length) == 0;
}

static int object_get(const char* text, const JsonToken* tokens, int object, const char* key)
{
	int index;
	if (object < 0 || tokens[object].type != JSON_OBJECT) {
		return -1;
	}
	index = object + 1;
	while (index < tokens[object].next) {
		int value = index + 1;
		if (token_equals(text, &tokens[index], key)) {
			return value;
		}
		index = tokens[value].next;
	}
	return -1;
}

static int array_first(const JsonToken* tokens, int array)
{
	return array >= 0 && tokens[array].type == JSON_ARRAY ? array + 1 : -1;
}

static int token_to_string(const char* text, const JsonToken* token, char* output, size_t output_size)
{
	size_t source = token->start;
	size_t target = 0;
	if (output_size == 0) {
		return 0;
	}
	while (source < token->end && target + 1 < output_size) {
		char current = text[source++];
		if (current == '\\' && source < token->end) {
			char escaped = text[source++];
			switch (escaped) {
			case 'n': current = '\n'; break;
			case 'r': current = '\r'; break;
			case 't': current = '\t'; break;
			case 'b': current = '\b'; break;
			case 'f': current = '\f'; break;
			case 'u':
				if (source + 4 <= token->end) source += 4;
				current = '?';
				break;
			default: current = escaped; break;
			}
		}
		output[target++] = current;
	}
	output[target] = '\0';
	return 1;
}

static int object_string(const char* text, const JsonToken* tokens, int object,
	const char* key, char* output, size_t output_size)
{
	int value = object_get(text, tokens, object, key);
	if (value < 0 || tokens[value].type != JSON_STRING) {
		if (output_size > 0) output[0] = '\0';
		return 0;
	}
	return token_to_string(text, &tokens[value], output, output_size);
}

static int object_number(const char* text, const JsonToken* tokens, int object,
	const char* key, double* number)
{
	int value = object_get(text, tokens, object, key);
	char buffer[64];
	char* end;
	if (value < 0 || tokens[value].type != JSON_PRIMITIVE ||
		token_equals(text, &tokens[value], "null")) {
		return 0;
	}
	if (tokens[value].end - tokens[value].start >= sizeof(buffer)) {
		return 0;
	}
	memcpy(buffer, text + tokens[value].start, tokens[value].end - tokens[value].start);
	buffer[tokens[value].end - tokens[value].start] = '\0';
	*number = strtod(buffer, &end);
	return end != buffer;
}

static int relationship_id(const char* text, const JsonToken* tokens, int object,
	const char* relationship, char* output, size_t output_size)
{
	int relationships = object_get(text, tokens, object, "relationships");
	int item = object_get(text, tokens, relationships, relationship);
	int data = object_get(text, tokens, item, "data");
	return object_string(text, tokens, data, "id", output, output_size);
}

static int decode_map_polyline(const char* encoded, MapRouteLine* line)
{
	size_t position = 0;
	long latitude = 0;
	long longitude = 0;
	while (encoded[position] != '\0' && line->point_count < MAX_MAP_ROUTE_POINTS) {
		long values[2];
		int coordinate;
		for (coordinate = 0; coordinate < 2; ++coordinate) {
			long result = 0;
			int shift = 0;
			int chunk;
			int continued;
			do {
				unsigned char byte;
				if (encoded[position] == '\0' || shift > 30) return line->point_count;
				byte = (unsigned char)encoded[position++] - 63;
				chunk = byte & 0x1f;
				result |= (long)chunk << shift;
				shift += 5;
				continued = byte >= 0x20;
			} while (continued);
			values[coordinate] = (result & 1) ? ~(result >> 1) : (result >> 1);
		}
		latitude += values[0];
		longitude += values[1];
		line->latitude[line->point_count] = latitude * 0.00001;
		line->longitude[line->point_count] = longitude * 0.00001;
		++line->point_count;
	}
	return line->point_count;
}

static int find_included(const char* text, const JsonToken* tokens, int root,
	const char* type, const char* id)
{
	int included = object_get(text, tokens, root, "included");
	int item = array_first(tokens, included);
	while (item >= 0 && item < tokens[included].next) {
		char found_type[32];
		char found_id[64];
		if (object_string(text, tokens, item, "type", found_type, sizeof(found_type)) &&
			object_string(text, tokens, item, "id", found_id, sizeof(found_id)) &&
			strcmp(found_type, type) == 0 && strcmp(found_id, id) == 0) {
			return item;
		}
		item = tokens[item].next;
	}
	return -1;
}

static char* http_get(const wchar_t* host, const wchar_t* path, char* error, size_t error_size)
{
	HINTERNET session = NULL;
	HINTERNET connection = NULL;
	HINTERNET request = NULL;
	char* response = NULL;
	size_t used = 0;
	size_t capacity = 0;
	DWORD bytes_read;
	DWORD status_code = 0;
	DWORD status_size = sizeof(status_code);

	session = WinHttpOpen(L"MBTA-Train-Tracker/1.1 (https://amtraker.com)",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (session == NULL) goto network_error;
	WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
	connection = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (connection == NULL) goto network_error;
	request = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (request == NULL || !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, NULL)) {
		goto network_error;
	}
	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) ||
		status_code < 200 || status_code >= 300) {
		set_error(error, error_size, "Remote train API returned an unsuccessful HTTP status.");
		goto cleanup;
	}
	for (;;) {
		char chunk[8192];
		if (!WinHttpReadData(request, chunk, sizeof(chunk), &bytes_read)) goto network_error;
		if (bytes_read == 0) break;
		if (used + bytes_read + 1 > capacity) {
			size_t new_capacity = capacity == 0 ? 16384 : capacity * 2;
			char* grown;
			while (new_capacity < used + bytes_read + 1) new_capacity *= 2;
			grown = (char*)realloc(response, new_capacity);
			if (grown == NULL) {
				set_error(error, error_size, "Not enough memory for the MBTA response.");
				goto cleanup;
			}
			response = grown;
			capacity = new_capacity;
		}
		memcpy(response + used, chunk, bytes_read);
		used += bytes_read;
	}
	if (response == NULL) {
		response = (char*)malloc(1);
		if (response == NULL) goto cleanup;
	}
	response[used] = '\0';
	goto cleanup;

network_error:
	set_error(error, error_size, "Network request to a train data API failed.");
cleanup:
	if (request != NULL) WinHttpCloseHandle(request);
	if (connection != NULL) WinHttpCloseHandle(connection);
	if (session != NULL) WinHttpCloseHandle(session);
	if (error != NULL && error_size > 0 && error[0] != '\0') {
		free(response);
		return NULL;
	}
	return response;
}

static int parse_json(const char* text, JsonToken** output, int* count)
{
	JsonParser parser;
	int root;
	parser.text = text;
	parser.length = strlen(text);
	parser.position = 0;
	parser.count = 0;
	parser.tokens = (JsonToken*)calloc(JSON_TOKEN_LIMIT, sizeof(JsonToken));
	if (parser.tokens == NULL) return 0;
	root = parse_value(&parser);
	skip_space(&parser);
	if (root < 0 || parser.position != parser.length) {
		free(parser.tokens);
		return 0;
	}
	*output = parser.tokens;
	*count = parser.count;
	return 1;
}

static int is_new_england_station(const char* code)
{
	static const char* codes[] = {
		"BOS", "BBY", "RTE", "BON", "SPG", "HFD", "WNL", "PVD", "KIN", "WLY",
		"NLC", "MYS", "OSB", "NHV", "BRP", "STM", "NRO", "HHL", "EXR", "DHM",
		"DOV", "WEM", "SAO", "ORB", "POR", "FRE", "BRK", "BTV", "RUT", "MID",
		"BTN", "MPR", "ESX", "PIT"
	};
	size_t index;
	for (index = 0; index < sizeof(codes) / sizeof(codes[0]); ++index) {
		if (strcmp(code, codes[index]) == 0) return 1;
	}
	return 0;
}

static int amtrak_route_serves_new_england(const char* text, const JsonToken* tokens, int train_object)
{
	int stations = object_get(text, tokens, train_object, "stations");
	int station = array_first(tokens, stations);
	if (stations < 0) return 0;
	while (station >= 0 && station < tokens[stations].next) {
		char code[16];
		if (object_string(text, tokens, station, "code", code, sizeof(code)) &&
			is_new_england_station(code)) {
			return 1;
		}
		station = tokens[station].next;
	}
	return 0;
}

static void append_amtrak_trains(Train* trains, int capacity, int* count,
	char* warning, size_t warning_size)
{
	char error[256] = "";
	char* response = http_get(L"api.amtraker.com", L"/v3/trains", error, sizeof(error));
	JsonToken* tokens = NULL;
	int token_count = 0;
	int group;
	if (response == NULL) {
		snprintf(warning, warning_size, "Amtraker unavailable: %s", error);
		return;
	}
	if (!parse_json(response, &tokens, &token_count)) {
		free(response);
		set_error(warning, warning_size, "Amtraker returned data that could not be parsed.");
		return;
	}

	group = 1;
	while (group < tokens[0].next && *count < capacity) {
		int train_array = group + 1;
		if (tokens[train_array].type == JSON_ARRAY) {
			int item = array_first(tokens, train_array);
			while (item >= 0 && item < tokens[train_array].next && *count < capacity) {
				char provider[24] = "";
				char train_state[24] = "";
				if (object_string(response, tokens, item, "provider", provider, sizeof(provider)) &&
					strcmp(provider, "Amtrak") == 0 &&
					object_string(response, tokens, item, "trainState", train_state, sizeof(train_state)) &&
					strcmp(train_state, "Active") == 0 &&
					amtrak_route_serves_new_england(response, tokens, item)) {
					Train train;
					int stations = object_get(response, tokens, item, "stations");
					int station = array_first(tokens, stations);
					memset(&train, 0, sizeof(train));
					train.source = TRAIN_SOURCE_AMTRAK;
					object_string(response, tokens, item, "trainID", train.vehicle_id, sizeof(train.vehicle_id));
					object_string(response, tokens, item, "trainNumRaw", train.car_label, sizeof(train.car_label));
					if (train.car_label[0] == '\0') {
						object_string(response, tokens, item, "trainNum", train.car_label, sizeof(train.car_label));
					}
					object_string(response, tokens, item, "trainNum", train.route_id, sizeof(train.route_id));
					object_string(response, tokens, item, "trainID", train.trip_id, sizeof(train.trip_id));
					object_string(response, tokens, item, "routeName", train.route_name, sizeof(train.route_name));
					object_string(response, tokens, item, "destName", train.destination, sizeof(train.destination));
					object_string(response, tokens, item, "eventName", train.current_stop, sizeof(train.current_stop));
					object_string(response, tokens, item, "trainState", train.current_status, sizeof(train.current_status));
					if (!object_string(response, tokens, item, "lastValTS", train.updated_at, sizeof(train.updated_at))) {
						object_string(response, tokens, item, "updatedAt", train.updated_at, sizeof(train.updated_at));
					}
					if (object_number(response, tokens, item, "lat", &train.latitude) &&
						object_number(response, tokens, item, "lon", &train.longitude)) {
						train.has_location = 1;
					}
					while (station >= 0 && station < tokens[stations].next) {
						char status[24] = "";
						if (object_string(response, tokens, station, "status", status, sizeof(status)) &&
							strcmp(status, "Enroute") == 0 && train.upcoming_stop_count < MAX_UPCOMING_STOPS) {
						TrainStopPrediction* prediction = &train.upcoming_stops[train.upcoming_stop_count];
						object_string(response, tokens, station, "name", prediction->name, sizeof(prediction->name));
						object_string(response, tokens, station, "arr", prediction->arrival, sizeof(prediction->arrival));
						object_string(response, tokens, station, "dep", prediction->departure, sizeof(prediction->departure));
						if (train.upcoming_stop_count == 0) {
							snprintf(train.next_stop, sizeof(train.next_stop), "%s", prediction->name);
							snprintf(train.next_arrival, sizeof(train.next_arrival), "%s", prediction->arrival);
							snprintf(train.next_departure, sizeof(train.next_departure), "%s", prediction->departure);
						}
						++train.upcoming_stop_count;
						}
						station = tokens[station].next;
					}
					train.route_type = -1;
					if (train.vehicle_id[0] != '\0' && train.car_label[0] != '\0') {
						trains[(*count)++] = train;
					}
				}
				item = tokens[item].next;
			}
		}
		group = tokens[train_array].next;
	}
	free(tokens);
	free(response);
}

int fetch_trains(Train* trains, int capacity, char* error, unsigned long error_size)
{
	static const wchar_t path[] = L"/vehicles?filter%5Broute_type%5D=2&include=route,trip,stop&page%5Blimit%5D=500";
	char* response;
	JsonToken* tokens = NULL;
	int token_count = 0;
	int root;
	int data;
	int item;
	int count = 0;

	if (error != NULL && error_size > 0) error[0] = '\0';
	response = http_get(L"api-v3.mbta.com", path, error, error_size);
	if (response == NULL) return -1;
	if (!parse_json(response, &tokens, &token_count)) {
		free(response);
		set_error(error, error_size, "Could not parse the MBTA vehicle response.");
		return -1;
	}
	root = 0;
	data = object_get(response, tokens, root, "data");
	item = array_first(tokens, data);
	while (item >= 0 && item < tokens[data].next && count < capacity) {
		Train train;
		int attributes = object_get(response, tokens, item, "attributes");
		char route_id[32];
		char trip_id[64];
		char stop_id[32];
		char label[32];
		int route_object;
		int trip_object;
		int stop_object;
		double value = 0;
		memset(&train, 0, sizeof(train));
		train.source = TRAIN_SOURCE_MBTA;
		object_string(response, tokens, item, "id", train.vehicle_id, sizeof(train.vehicle_id));
		object_string(response, tokens, attributes, "label", label, sizeof(label));
		object_string(response, tokens, attributes, "current_status", train.current_status, sizeof(train.current_status));
		object_string(response, tokens, attributes, "updated_at", train.updated_at, sizeof(train.updated_at));
		value = 0;
		object_number(response, tokens, attributes, "current_stop_sequence", &value);
		train.current_stop_sequence = (int)value;
		if (object_number(response, tokens, attributes, "latitude", &train.latitude) &&
			object_number(response, tokens, attributes, "longitude", &train.longitude)) {
			train.has_location = 1;
		}
		relationship_id(response, tokens, item, "route", route_id, sizeof(route_id));
		relationship_id(response, tokens, item, "trip", trip_id, sizeof(trip_id));
		relationship_id(response, tokens, item, "stop", stop_id, sizeof(stop_id));
		snprintf(train.car_label, sizeof(train.car_label), "%s", label);
		snprintf(train.route_id, sizeof(train.route_id), "%s", route_id);
		snprintf(train.trip_id, sizeof(train.trip_id), "%s", trip_id);
		route_object = find_included(response, tokens, root, "route", route_id);
		trip_object = find_included(response, tokens, root, "trip", trip_id);
		stop_object = find_included(response, tokens, root, "stop", stop_id);
		if (route_object < 0 || trip_object < 0) {
			item = tokens[item].next;
			continue;
		}
		{
			int route_attributes = object_get(response, tokens, route_object, "attributes");
			object_string(response, tokens, route_attributes, "long_name", train.route_name, sizeof(train.route_name));
			if (train.route_name[0] == '\0') {
				object_string(response, tokens, route_attributes, "short_name", train.route_name, sizeof(train.route_name));
			}
			value = 0;
			object_number(response, tokens, route_attributes, "type", &value);
			train.route_type = (int)value;
		}
		{
			int trip_attributes = object_get(response, tokens, trip_object, "attributes");
			object_string(response, tokens, trip_attributes, "headsign", train.destination, sizeof(train.destination));
		}
		if (stop_object >= 0) {
			int stop_attributes = object_get(response, tokens, stop_object, "attributes");
			object_string(response, tokens, stop_attributes, "name", train.current_stop, sizeof(train.current_stop));
		}
		if (train.route_type == 2 && train.vehicle_id[0] != '\0') {
			snprintf(train.car_label, sizeof(train.car_label), "%s", label[0] ? label : train.vehicle_id);
			trains[count++] = train;
		}
		item = tokens[item].next;
	}
	free(tokens);
	free(response);
	{
		char warning[256] = "";
		append_amtrak_trains(trains, capacity, &count, warning, sizeof(warning));
		if (warning[0] != '\0') set_error(error, error_size, warning);
	}
	return count;
}

static int is_heritage_locomotive(const Train* train)
{
	return train->source == TRAIN_SOURCE_MBTA &&
		(strcmp(train->car_label, "1030") == 0 ||
			strcmp(train->car_label, "1036") == 0 ||
			strcmp(train->car_label, "1071") == 0 ||
			strcmp(train->car_label, "1072") == 0 ||
			strcmp(train->car_label, "1128") == 0 ||
			strcmp(train->car_label, "1129") == 0 ||
			strcmp(train->car_label, "1130") == 0 ||
			strcmp(train->car_label, "1824") == 0 ||
			strcmp(train->car_label, "1825") == 0 ||
			strcmp(train->car_label, "1806") == 0 ||
			strcmp(train->car_label, "1700") == 0 ||
			strcmp(train->car_label, "1776") == 0);
}

static int str_contains_ci(const char* haystack, const char* needle)
{
	size_t needle_length = strlen(needle);
	size_t index;
	if (needle_length == 0) return 1;
	for (index = 0; haystack[index] != '\0'; ++index) {
		if (_strnicmp(haystack + index, needle, needle_length) == 0) return 1;
	}
	return 0;
}

int train_matches_keyword(const Train* train, const char* keyword)
{
	if (_stricmp(keyword, "heritage") == 0) {
		return is_heritage_locomotive(train);
	}
	if (_stricmp(keyword, "amtrak") == 0) {
		return train->source == TRAIN_SOURCE_AMTRAK;
	}
	if (_stricmp(keyword, "mbta") == 0 || _stricmp(keyword, "commuter") == 0) {
		return train->source == TRAIN_SOURCE_MBTA;
	}
	return str_contains_ci(train->route_name, keyword) ||
		str_contains_ci(train->destination, keyword) ||
		str_contains_ci(train->current_stop, keyword) ||
		str_contains_ci(train->route_id, keyword) ||
		str_contains_ci(train->vehicle_id, keyword) ||
		str_contains_ci(train->car_label, keyword);
}

void show_nearest_trains(const Train* trains, int count, double latitude, double longitude, int limit)
{
	int order[MAX_TRAINS];
	double distance[MAX_TRAINS];
	int located = 0;
	int i;
	int j;

	for (i = 0; i < count; ++i) {
		if (trains[i].has_location) {
			order[located] = i;
			distance[located] = haversine_miles(latitude, longitude, trains[i].latitude, trains[i].longitude);
			++located;
		}
	}
	for (i = 1; i < located; ++i) {
		int key_order = order[i];
		double key_distance = distance[i];
		j = i - 1;
		while (j >= 0 && distance[j] > key_distance) {
			distance[j + 1] = distance[j];
			order[j + 1] = order[j];
			--j;
		}
		distance[j + 1] = key_distance;
		order[j + 1] = key_order;
	}
	if (located == 0) {
		printf("No trains currently have a reported position.\n");
		return;
	}
	if (limit > located) limit = located;
	printf("\nNearest %d train(s):\n", limit);
	printf("%-8s %-10s %-22s %-10s %s\n", "Source", "Train #", "Line", "Distance", "Destination");
	printf("----------------------------------------------------------------------------------------------\n");
	for (i = 0; i < limit; ++i) {
		const Train* train = &trains[order[i]];
		printf("%-8s %-10s %-22.22s %6.1f mi  %s\n",
			train->source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA",
			train->car_label, train->route_name, distance[i],
			train->destination[0] ? train->destination : "Unknown");
	}
}

void show_route_summary(const Train* trains, int count)
{
	char seen_routes[64][96];
	int seen_count[64];
	int seen_heritage[64];
	int route_total = 0;
	int i;
	int j;

	if (count == 0) {
		printf("No active trains to summarize.\n");
		return;
	}
	for (i = 0; i < count; ++i) {
		const char* name = trains[i].route_name[0] ? trains[i].route_name : trains[i].route_id;
		int found = -1;
		for (j = 0; j < route_total; ++j) {
			if (strcmp(seen_routes[j], name) == 0) {
				found = j;
				break;
			}
		}
		if (found < 0 && route_total < 64) {
			found = route_total++;
			snprintf(seen_routes[found], sizeof(seen_routes[found]), "%s", name);
			seen_count[found] = 0;
			seen_heritage[found] = 0;
		}
		if (found >= 0) {
			++seen_count[found];
			if (is_heritage_locomotive(&trains[i])) ++seen_heritage[found];
		}
	}
	printf("\nActive train counts by line (%d line(s)):\n", route_total);
	printf("%-30s %-8s %s\n", "Line", "Trains", "Heritage");
	printf("----------------------------------------------------------------------------------------------\n");
	for (j = 0; j < route_total; ++j) {
		printf("%-30.30s %-8d %s\n", seen_routes[j], seen_count[j], seen_heritage[j] > 0 ? "yes" : "");
	}
}

int fetch_route_alerts(const char* route_id, RouteAlert* alerts, int capacity, char* error, unsigned long error_size)
{
	char path[256];
	wchar_t wide_path[512];
	char* response;
	JsonToken* tokens = NULL;
	int token_count = 0;
	int root;
	int data;
	int item;
	int count = 0;
	if (error != NULL && error_size > 0) error[0] = '\0';
	if (route_id == NULL || route_id[0] == '\0') {
		set_error(error, error_size, "An MBTA route ID is required.");
		return -1;
	}

	snprintf(path, sizeof(path),
		"/alerts?filter%%5Broute%%5D=%s&filter%%5Blifecycle%%5D=NEW,ONGOING,ONGOING_UPCOMING",
		route_id);
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
		set_error(error, error_size, "Could not build the alerts request.");
		return -1;
	}
	response = http_get(L"api-v3.mbta.com", wide_path, error, error_size);
	if (response == NULL) {
		return -1;
	}
	if (!parse_json(response, &tokens, &token_count)) {
		set_error(error, error_size, "Alerts unavailable: could not parse the response.");
		free(response);
		return -1;
	}
	root = 0;
	data = object_get(response, tokens, root, "data");
	item = array_first(tokens, data);
	while (item >= 0 && item < tokens[data].next && count < capacity && alerts != NULL) {
		int attributes = object_get(response, tokens, item, "attributes");
		alerts[count].header[0] = '\0';
		alerts[count].effect[0] = '\0';
		object_string(response, tokens, attributes, "header", alerts[count].header, sizeof(alerts[count].header));
		object_string(response, tokens, attributes, "effect", alerts[count].effect, sizeof(alerts[count].effect));
		if (alerts[count].effect[0] == '\0') snprintf(alerts[count].effect, sizeof(alerts[count].effect), "ALERT");
		if (alerts[count].header[0] == '\0') snprintf(alerts[count].header, sizeof(alerts[count].header), "(no description)");
		++count;
		item = tokens[item].next;
	}
	free(tokens);
	free(response);
	return count;
}

void show_alerts(const char* route_id)
{
	RouteAlert alerts[MAX_ROUTE_ALERTS];
	char error[256] = "";
	int count = fetch_route_alerts(route_id, alerts, MAX_ROUTE_ALERTS, error, sizeof(error));
	int index;
	if (count < 0) {
		printf("Alerts unavailable: %s\n", error[0] ? error : "unknown error");
		return;
	}
	if (count == 0) {
		printf("No active alerts reported for route %s.\n", route_id);
		return;
	}
	printf("\nActive alerts for route %s:\n", route_id);
	for (index = 0; index < count; ++index) {
		printf("- [%s] %s\n", alerts[index].effect, alerts[index].header);
	}
}

int fetch_map_route_lines(MapRouteLine* lines, int capacity, char* error, unsigned long error_size)
{
	wchar_t wide_path[1024];
	char* response;
	JsonToken* tokens = NULL;
	int token_count = 0;
	int route_data;
	int route_item;
	int route_catalog_count = 0;
	char route_names[MAX_MAP_ROUTE_LINES][96] = { 0 };
	char route_ids[MAX_MAP_ROUTE_LINES][32] = { 0 };
	int route_index;
	int total_shapes = 0;
	int failed_routes = 0;
	if (error != NULL && error_size > 0) error[0] = '\0';
	if (lines == NULL || capacity <= 0) return 0;
	if (MultiByteToWideChar(CP_UTF8, 0,
		"/routes?filter%5Btype%5D=2&page%5Blimit%5D=100", -1, wide_path,
		(int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
		set_error(error, error_size, "Could not build the MBTA rail-route request.");
		return -1;
	}
	{
		char route_error[256] = "";
		response = http_get(L"api-v3.mbta.com", wide_path, route_error, sizeof(route_error));
		if (response == NULL) {
			set_error(error, error_size, route_error[0] ? route_error : "Could not fetch the MBTA rail routes.");
			return -1;
		}
	}
	if (!parse_json(response, &tokens, &token_count)) {
		free(response);
		set_error(error, error_size, "Could not parse the MBTA rail-route response.");
		return -1;
	}
	route_data = object_get(response, tokens, 0, "data");
	route_item = array_first(tokens, route_data);
	while (route_item >= 0 && route_item < tokens[route_data].next &&
		route_catalog_count < MAX_MAP_ROUTE_LINES) {
		char route_id[32] = "";
		if (object_string(response, tokens, route_item, "id", route_id, sizeof(route_id)) && route_id[0]) {
			int attributes = object_get(response, tokens, route_item, "attributes");
			snprintf(route_ids[route_catalog_count], sizeof(route_ids[route_catalog_count]), "%s", route_id);
			object_string(response, tokens, attributes, "long_name", route_names[route_catalog_count],
				sizeof(route_names[route_catalog_count]));
			if (route_names[route_catalog_count][0] == '\0') {
				object_string(response, tokens, attributes, "short_name", route_names[route_catalog_count],
					sizeof(route_names[route_catalog_count]));
			}
			++route_catalog_count;
		}
		route_item = tokens[route_item].next;
	}
	free(tokens);
	free(response);
	if (route_catalog_count == 0) {
		set_error(error, error_size, "No MBTA commuter-rail routes were returned.");
		return -1;
	}
	for (route_index = 0; route_index < route_catalog_count && total_shapes < capacity; ++route_index) {
		char path[256];
		wchar_t route_path[512];
		char route_error[256] = "";
		int data;
		int item;
		snprintf(path, sizeof(path), "/shapes?filter%%5Broute%%5D=%s&page%%5Blimit%%5D=500", route_ids[route_index]);
		if (MultiByteToWideChar(CP_UTF8, 0, path, -1, route_path,
			(int)(sizeof(route_path) / sizeof(route_path[0]))) == 0) {
			++failed_routes;
			continue;
		}
		response = http_get(L"api-v3.mbta.com", route_path, route_error, sizeof(route_error));
		if (response == NULL) {
			++failed_routes;
			continue;
		}
		if (!parse_json(response, &tokens, &token_count)) {
			free(response);
			++failed_routes;
			continue;
		}
		data = object_get(response, tokens, 0, "data");
		item = array_first(tokens, data);
		while (item >= 0 && item < tokens[data].next && total_shapes < capacity) {
			int attributes = object_get(response, tokens, item, "attributes");
			char encoded[24000];
			MapRouteLine* line = &lines[total_shapes];
			memset(line, 0, sizeof(*line));
			if (object_string(response, tokens, attributes, "polyline", encoded, sizeof(encoded))) {
				snprintf(line->route_id, sizeof(line->route_id), "%s", route_ids[route_index]);
				snprintf(line->route_name, sizeof(line->route_name), "%s", route_names[route_index]);
				if (decode_map_polyline(encoded, line) >= 2) ++total_shapes;
			}
			item = tokens[item].next;
		}
		free(tokens);
		free(response);
	}
	if (failed_routes > 0 && error != NULL && error_size > 0) {
		snprintf(error, error_size, "%d commuter-rail route shape request(s) were unavailable.", failed_routes);
	}
	if (total_shapes == 0 && failed_routes > 0) return -1;
	return total_shapes;
}

/* Sourced from the Scan New England Wiki "Railroads" page (community scanner database,
   dated/attributed entries; snewiki.com/wiki/index.php/Railroads), retrieved 2026-09-28.
   Several MBTA lines share a single dispatcher/road channel by design (grouped below as
   published); channel assignments can be reused or reassigned over time, so cross-check
   RadioReference.com or the FCC ULS before relying on these for anything important. */
static const RadioChannel radio_channels[] = {
	{"Fairmount Line", "Dispatch/Yard", "63", 161.0550, "Southampton Street & Fairmount Line - SNE Wiki 2026.08.27"},
	{"Franklin/Foxboro Line", "Dispatch", "92", 161.4900, "Branch Line Dispatcher (shared w/ Dorchester/Needham/Stoughton) - SNE Wiki 2026.05.04"},
	{"Needham Line", "Dispatch", "92", 161.4900, "Branch Line Dispatcher (shared w/ Dorchester/Foxboro/Franklin/Stoughton) - SNE Wiki 2026.05.04"},
	{"Providence/Stoughton Line", "Road", "54", 160.9200, "Amtrak NEC Road, South Station to New Haven CT (shared trackage) - SNE Wiki 2026.08.27"},
	{"Providence/Stoughton Line", "Dispatch", "92", 161.4900, "Stoughton branch: Branch Line Dispatcher - SNE Wiki 2026.05.04"},
	{"Framingham/Worcester Line", "Road", "20", 160.4100, "Keolis Worcester Line road channel, east of CP-45 - SNE Wiki 2024.10.12"},
	{"Framingham/Worcester Line", "Dispatch", "3838", 160.6800, "CSX Boston Subdivision dispatcher, west of CP-45 to Wilbraham - SNE Wiki 2022.10.21"},
	{"Framingham/Worcester Line", "Yard", "5050", 160.8600, "CSX Framingham Yard - SNE Wiki 2022.10.21"},
	{"Fitchburg Line", "Road", "32", 160.5900, "Boston West and Fitchburg dispatchers, Northside - SNE Wiki 2026.01.09"},
	{"Lowell Line", "Road", "14", 160.3200, "Boston East and Valley dispatchers, Northside - SNE Wiki 2026.08.08"},
	{"Haverhill Line", "Road", "14", 160.3200, "Boston East and Valley dispatchers, Northside - SNE Wiki 2026.08.08"},
	{"Newburyport/Rockport Line", "Road", "14", 160.3200, "Boston East and Valley dispatchers, Northside - SNE Wiki 2026.08.08"},
	{"Amtrak Downeaster", "Road", "070", 161.1600, "CSX New England Div. Road, Portland/Middlesex/Nashua Subs (Lowell Line route) - SNE Wiki 2026.08.08"},
	{"Amtrak Downeaster", "Dispatch", "094", 161.5200, "CSX EB Dispatcher, Portland/Middlesex/Nashua Subs - SNE Wiki 2026.08.08"},
	{"Amtrak Northeast Corridor", "Road", "54", 160.9200, "NEC Road, South Station to New Haven CT - SNE Wiki 2026.08.27"},
	{"Amtrak Northeast Corridor", "Dispatch", "42", 160.7400, "Southampton Service & Inspection (S&I), Boston - SNE Wiki 2024.09.05"},
	{"Amtrak Northeast Corridor", "Yard", "23", 160.4550, "Boston South Yard - SNE Wiki 2026.08.27"},
	{"Amtrak Lake Shore Limited", "Road", "4646", 160.8000, "CSX Boston Line Road, west of Worcester - SNE Wiki 2022.10.21"},
	{"Amtrak Lake Shore Limited", "Dispatch", "3838", 160.6800, "CSX Boston Subdivision dispatcher, east of Wilbraham - SNE Wiki 2022.10.21"},
	{"Greenbush Line", "Road", "41", 160.7250, "Old Colony dispatcher (Greenbush/Kingston/Middleborough) - SNE Wiki 2026.08.27"},
	{"Kingston Line", "Road", "41", 160.7250, "Old Colony dispatcher (Greenbush/Kingston/Middleborough) - SNE Wiki 2026.08.27"},
	{"Middleborough/Lakeville Line", "Road", "41", 160.7250, "Old Colony dispatcher (Greenbush/Kingston/Middleborough) - SNE Wiki 2026.08.27"},
	{"Middleborough/Lakeville Line", "Road (South)", "91", 161.4750, "Southcoast - Middleboro South road channel - SNE Wiki 2026.08.27"},
	{"MBTA Commuter Rail (system-wide)", "Yard", "07", 160.2150, "Readville Yard - SNE Wiki 2015.12.19"},
	{"MBTA Commuter Rail (system-wide)", "Yard", "51", 160.8750, "North Side Yard - SNE Wiki 2015.12.19"},
	{"MBTA Commuter Rail (system-wide)", "Terminal", "87", 161.4150, "Northside Terminal (Boston Engine Terminal) - SNE Wiki 2026.08.08"},
	{"MBTA Commuter Rail (system-wide)", "Utility", "24", 160.4700, "System-wide utility channel - SNE Wiki 2015.12.19"},
	{"Amtrak (system-wide)", "Police", "79", 161.2950, "Amtrak Police, Boston/Attleboro/Springfield repeaters - SNE Wiki 2026.05.27"}
};

int search_radio_frequencies(const char* keyword, RadioChannel* results, int capacity)
{
	static const char* filler_words[] = {
		"for", "the", "of", "on", "in", "near", "line", "lines", "channel", "channels",
		"frequency", "frequencies", "freq", "please", "show", "list", "find", "search"
	};
	char buffer[160];
	char* keywords[10];
	int keyword_count = 0;
	char* token;
	char* context = NULL;
	size_t index;
	int found = 0;

	snprintf(buffer, sizeof(buffer), "%s", keyword != NULL ? keyword : "");
	token = strtok_s(buffer, " \t", &context);
	while (token != NULL && keyword_count < 10) {
		size_t filler_index;
		int is_filler = 0;
		for (filler_index = 0; filler_index < sizeof(filler_words) / sizeof(filler_words[0]); ++filler_index) {
			if (_stricmp(token, filler_words[filler_index]) == 0) {
				is_filler = 1;
				break;
			}
		}
		if (!is_filler) keywords[keyword_count++] = token;
		token = strtok_s(NULL, " \t", &context);
	}

	for (index = 0; index < sizeof(radio_channels) / sizeof(radio_channels[0]); ++index) {
		const RadioChannel* channel = &radio_channels[index];
		int matched_all = 1;
		int keyword_index;
		for (keyword_index = 0; keyword_index < keyword_count; ++keyword_index) {
			const char* word = keywords[keyword_index];
			if (!str_contains_ci(channel->line, word) && !str_contains_ci(channel->channel_type, word) &&
				!str_contains_ci(channel->aar_channel, word) && !str_contains_ci(channel->notes, word)) {
				matched_all = 0;
				break;
			}
		}
		if (matched_all && results != NULL && found < capacity) results[found++] = *channel;
	}
	return found;
}

void show_radio_frequencies(const char* keyword)
{
	RadioChannel results[MAX_RADIO_CHANNELS];
	int count = search_radio_frequencies(keyword, results, MAX_RADIO_CHANNELS);
	int index;
	if (count > 0) {
		printf("\n%-32s %-14s %-4s %-9s %s\n", "Line", "Type", "Ch", "Freq MHz", "Notes");
		printf("----------------------------------------------------------------------------------------------\n");
		for (index = 0; index < count; ++index) {
			printf("%-32.32s %-14s %-4s %-9.4f %s\n", results[index].line, results[index].channel_type,
				results[index].aar_channel, results[index].frequency_mhz, results[index].notes);
		}
	} else {
		printf("No radio channel entries match '%s'.\n", keyword != NULL ? keyword : "");
	}
	printf("\nSource: Scan New England Wiki (snewiki.com), a community-maintained scanner database; entries are "
		"dated/attributed there. Channel assignments can be reused or reassigned over time, so cross-check "
		"RadioReference.com or the FCC ULS before relying on these.\n");
}

void show_train_list(const Train* trains, int count)
{
	int index;
	if (count == 0) {
		printf("No active trains are currently listed by the available feeds.\n");
		return;
	}
	printf("Active trains: %d\n", count);
	printf("%-8s %-14s %-10s %-22s %-9s %-8s %s\n",
		"Source", "Run ID", "Train #", "Line", "Mark", "Speed", "Destination");
	printf("----------------------------------------------------------------------------------------------\n");
	for (index = 0; index < count; ++index) {
		char speed_text[16] = "";
		if (trains[index].has_speed) {
			snprintf(speed_text, sizeof(speed_text), "%.0f mph", trains[index].speed_mph);
		}
		printf("%-8s %-14s %-10s %-22.22s %-9s %-8s %s\n",
			trains[index].source == TRAIN_SOURCE_AMTRAK ? "Amtrak" : "MBTA",
			trains[index].vehicle_id,
			trains[index].car_label,
			trains[index].route_name,
			is_heritage_locomotive(&trains[index]) ? "HERITAGE" : "",
			speed_text,
			trains[index].destination[0] ? trains[index].destination : "Unknown");
	}
}

typedef struct DetailBuffer {
	char* text;
	size_t capacity;
	size_t length;
} DetailBuffer;

static void append_train_detail(DetailBuffer* output, const char* format, ...)
{
	va_list arguments;
	int written;
	size_t available;
	if (output->length >= output->capacity) return;
	available = output->capacity - output->length;
	va_start(arguments, format);
	written = vsnprintf(output->text + output->length, available, format, arguments);
	va_end(arguments);
	if (written < 0) return;
	if ((size_t)written >= available) output->length = output->capacity - 1;
	else output->length += (size_t)written;
}

#define printf(...) append_train_detail(&detail_writer, __VA_ARGS__)
void format_train_details_with_stops(const Train* train, char* output, unsigned long output_size,
	TrainStopPrediction* stops, int stop_capacity, int* stop_count)
{
	DetailBuffer detail_writer;
	char path[256];
	wchar_t wide_path[512];
	char error[256] = "";
	char* response;
	JsonToken* tokens = NULL;
	int token_count = 0;
	int root;
	int data;
	int item;
	int best = -1;
	int best_sequence = 2147483647;
	int upcoming_items[5];
	int upcoming_sequences[5];
	int upcoming_count = 0;
	int upcoming_index;
	char confirmed_vehicle[64];
	char confirmed_trip[64];
	char confirmed_route[32];
	char formatted_time[64];
	if (output == NULL || output_size == 0) return;
	if (stop_count != NULL) *stop_count = 0;
	detail_writer.text = output;
	detail_writer.capacity = (size_t)output_size;
	detail_writer.length = 0;
	output[0] = '\0';

	if (train->source == TRAIN_SOURCE_AMTRAK) {
		if (stops != NULL && stop_count != NULL && stop_capacity > 0) {
			int count = train->upcoming_stop_count;
			if (count > stop_capacity) count = stop_capacity;
			if (count > 0) memcpy(stops, train->upcoming_stops, sizeof(TrainStopPrediction) * count);
			*stop_count = count;
		}
		printf("\nAmtrak train %s (run ID %s)\n", train->car_label, train->vehicle_id);
		printf("Line: %s\n", train->route_name[0] ? train->route_name : "Unknown");
		printf("Destination: %s\n", train->destination[0] ? train->destination : "Unknown");
		printf("Status: %s\n", train->current_status[0] ? train->current_status : "Unknown");
		if (train->current_stop[0] != '\0') printf("Latest reported station/event: %s\n", train->current_stop);
		if (train->has_location) printf("Reported position: %.5f, %.5f\n", train->latitude, train->longitude);
		if (train->updated_at[0] != '\0') {
			format_timestamp(train->updated_at, formatted_time, sizeof(formatted_time));
			printf("Position update: %s\n", formatted_time);
		}
		if (train->next_stop[0] != '\0') {
			printf("Next stop: %s\n", train->next_stop);
			if (train->next_arrival[0] != '\0') {
				format_timestamp(train->next_arrival, formatted_time, sizeof(formatted_time));
				printf("Estimated arrival: %s\n", formatted_time);
			} else {
				printf("Estimated arrival: not provided\n");
			}
			if (train->next_departure[0] != '\0') {
				format_timestamp(train->next_departure, formatted_time, sizeof(formatted_time));
				printf("Estimated departure: %s\n", formatted_time);
			}
		} else {
			printf("Next stop prediction is not currently available.\n");
		}
		if (train->upcoming_stop_count > 1) {
			int stop_index;
			printf("Following stops and ETAs:\n");
			for (stop_index = 1; stop_index < train->upcoming_stop_count; ++stop_index) {
				char eta[64] = "ETA not provided";
				if (train->upcoming_stops[stop_index].arrival[0] != '\0') {
					format_timestamp(train->upcoming_stops[stop_index].arrival, eta, sizeof(eta));
				}
				printf("- %s: %s\n", train->upcoming_stops[stop_index].name, eta);
			}
		}
		printf("Source cross-check: Amtraker live run %s\n", train->vehicle_id);
		return;
	}

	printf("\nVehicle: %s (car/train number %s)\n", train->vehicle_id, train->car_label);
	printf("Line: %s\n", train->route_name[0] ? train->route_name : train->route_id);
	printf("Destination: %s\n", train->destination[0] ? train->destination : "Unknown");
	printf("Status: %s\n", train->current_status[0] ? train->current_status : "Unknown");
	if (train->current_stop[0] != '\0') {
		if (strcmp(train->current_status, "IN_TRANSIT_TO") == 0 ||
			strcmp(train->current_status, "INCOMING_AT") == 0) {
			printf("Approaching stop: %s\n", train->current_stop);
		} else if (strcmp(train->current_status, "STOPPED_AT") == 0) {
			printf("At stop: %s\n", train->current_stop);
		} else {
			printf("Reported stop: %s\n", train->current_stop);
		}
	}
	if (train->has_location) {
		printf("Current position: %.5f, %.5f\n", train->latitude, train->longitude);
	} else if (train->current_stop[0] == '\0') {
		printf("Current location: not reported\n");
	}
	if (train->updated_at[0] != '\0') {
		format_timestamp(train->updated_at, formatted_time, sizeof(formatted_time));
		printf("Vehicle update: %s\n", formatted_time);
	}

	snprintf(path, sizeof(path), "/predictions?filter%%5Btrip%%5D=%s&include=stop,route,trip&page%%5Blimit%%5D=100",
		train->trip_id);
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
		printf("Next stop and arrival: unavailable (could not form API request).\n");
		return;
	}
	response = http_get(L"api-v3.mbta.com", wide_path, error, sizeof(error));
	if (response == NULL) {
		printf("Next stop and arrival: unavailable (%s)\n", error);
		return;
	}
	if (!parse_json(response, &tokens, &token_count)) {
		printf("Next stop and arrival: unavailable (invalid prediction response).\n");
		free(response);
		return;
	}
	root = 0;
	data = object_get(response, tokens, root, "data");
	item = array_first(tokens, data);
	while (item >= 0 && item < tokens[data].next) {
		int attributes = object_get(response, tokens, item, "attributes");
		double sequence = 0;
		char prediction_trip[64] = "";
		if (object_number(response, tokens, attributes, "stop_sequence", &sequence) &&
			relationship_id(response, tokens, item, "trip", prediction_trip, sizeof(prediction_trip)) &&
			strcmp(prediction_trip, train->trip_id) == 0 &&
			(sequence > train->current_stop_sequence ||
				((strcmp(train->current_status, "IN_TRANSIT_TO") == 0 ||
					strcmp(train->current_status, "INCOMING_AT") == 0) &&
					sequence == train->current_stop_sequence))) {
			int sequence_number = (int)sequence;
			int position;
			if (sequence_number < best_sequence) {
				best = item;
				best_sequence = sequence_number;
			}
			if (upcoming_count < 5 || sequence_number < upcoming_sequences[4]) {
				position = upcoming_count < 5 ? upcoming_count++ : 4;
				while (position > 0 && upcoming_sequences[position - 1] > sequence_number) {
					upcoming_sequences[position] = upcoming_sequences[position - 1];
					upcoming_items[position] = upcoming_items[position - 1];
					--position;
				}
				upcoming_sequences[position] = sequence_number;
				upcoming_items[position] = item;
			}
		}
		item = tokens[item].next;
	}
	if (stops != NULL && stop_count != NULL && stop_capacity > 0) {
		int output_index;
		int count = upcoming_count < stop_capacity ? upcoming_count : stop_capacity;
		for (output_index = 0; output_index < count; ++output_index) {
			int prediction = upcoming_items[output_index];
			int attributes = object_get(response, tokens, prediction, "attributes");
			char stop_id[32] = "";
			int stop_object;
			memset(&stops[output_index], 0, sizeof(stops[output_index]));
			relationship_id(response, tokens, prediction, "stop", stop_id, sizeof(stop_id));
			object_string(response, tokens, attributes, "arrival_time", stops[output_index].arrival,
				sizeof(stops[output_index].arrival));
			object_string(response, tokens, attributes, "departure_time", stops[output_index].departure,
				sizeof(stops[output_index].departure));
			stop_object = find_included(response, tokens, root, "stop", stop_id);
			if (stop_object >= 0) {
				int stop_attributes = object_get(response, tokens, stop_object, "attributes");
				object_string(response, tokens, stop_attributes, "name", stops[output_index].name,
					sizeof(stops[output_index].name));
			}
			if (stops[output_index].name[0] == '\0') {
				snprintf(stops[output_index].name, sizeof(stops[output_index].name), "%s",
					stop_id[0] ? stop_id : "Unknown stop");
			}
		}
		*stop_count = count;
	}
	if (best < 0) {
		printf("Next stop and arrival: no upcoming prediction is currently available.\n");
	} else {
		int attributes = object_get(response, tokens, best, "attributes");
		char stop_id[32] = "";
		char arrival[48] = "";
		char departure[48] = "";
		char formatted_arrival[64];
		char formatted_departure[64];
		int stop_object;
		relationship_id(response, tokens, best, "stop", stop_id, sizeof(stop_id));
		object_string(response, tokens, attributes, "arrival_time", arrival, sizeof(arrival));
		object_string(response, tokens, attributes, "departure_time", departure, sizeof(departure));
		stop_object = find_included(response, tokens, root, "stop", stop_id);
		printf("Next stop: ");
		if (stop_object >= 0) {
			int stop_attributes = object_get(response, tokens, stop_object, "attributes");
			char stop_name[96] = "Unknown";
			object_string(response, tokens, stop_attributes, "name", stop_name, sizeof(stop_name));
			printf("%s\n", stop_name);
		} else {
			printf("stop %s\n", stop_id[0] ? stop_id : "unknown");
		}
		if (arrival[0] != '\0') {
			format_timestamp(arrival, formatted_arrival, sizeof(formatted_arrival));
			printf("Predicted arrival: %s\n", formatted_arrival);
		} else {
			printf("Predicted arrival: not provided\n");
		}
		if (departure[0] != '\0') {
			format_timestamp(departure, formatted_departure, sizeof(formatted_departure));
			printf("Predicted departure: %s\n", formatted_departure);
		}
		{
			char schedule_path[256];
			wchar_t schedule_wide[512];
			char schedule_error[256] = "";
			char* schedule_response = NULL;
			JsonToken* schedule_tokens = NULL;
			int schedule_token_count = 0;
			int schedule_data;
			int schedule_item;
			int schedule_attributes;
			char scheduled_arrival[48] = "";
			double delay_minutes = 0.0;

			snprintf(schedule_path, sizeof(schedule_path),
				"/schedules?filter%%5Btrip%%5D=%s&filter%%5Bstop_sequence%%5D=%d",
				train->trip_id, best_sequence);
			if (MultiByteToWideChar(CP_UTF8, 0, schedule_path, -1, schedule_wide,
				(int)(sizeof(schedule_wide) / sizeof(schedule_wide[0]))) != 0) {
				schedule_response = http_get(L"api-v3.mbta.com", schedule_wide, schedule_error, sizeof(schedule_error));
			}
			if (schedule_response != NULL && parse_json(schedule_response, &schedule_tokens, &schedule_token_count)) {
				schedule_data = object_get(schedule_response, schedule_tokens, 0, "data");
				schedule_item = array_first(schedule_tokens, schedule_data);
				if (schedule_item >= 0 && schedule_item < schedule_tokens[schedule_data].next) {
					schedule_attributes = object_get(schedule_response, schedule_tokens, schedule_item, "attributes");
					object_string(schedule_response, schedule_tokens, schedule_attributes, "arrival_time",
						scheduled_arrival, sizeof(scheduled_arrival));
					if (stops != NULL && stop_count != NULL && *stop_count > 0) {
						snprintf(stops[0].scheduled_arrival, sizeof(stops[0].scheduled_arrival), "%s", scheduled_arrival);
					}
					if (scheduled_arrival[0] != '\0' && arrival[0] != '\0') {
						delay_minutes = timestamp_to_minutes(arrival) - timestamp_to_minutes(scheduled_arrival);
						if (stops != NULL && stop_count != NULL && *stop_count > 0) {
							stops[0].delay_minutes = delay_minutes;
							stops[0].has_delay = 1;
						}
						if (delay_minutes > 0.5) {
							printf("Delay: about %.0f minute(s) late\n", delay_minutes);
						} else if (delay_minutes < -0.5) {
							printf("Delay: about %.0f minute(s) early\n", -delay_minutes);
						} else {
							printf("Delay: on time\n");
						}
					}
				}
			}
			free(schedule_tokens);
			free(schedule_response);
		}
		if (upcoming_count > 1) {
			printf("Following stops and ETAs:\n");
			for (upcoming_index = 1; upcoming_index < upcoming_count; ++upcoming_index) {
				int prediction = upcoming_items[upcoming_index];
				int prediction_attributes = object_get(response, tokens, prediction, "attributes");
				char stop_id[32] = "";
				char arrival_time[48] = "";
				char stop_name[96] = "Unknown";
				char eta[64] = "ETA not provided";
				int stop_object;
				relationship_id(response, tokens, prediction, "stop", stop_id, sizeof(stop_id));
				object_string(response, tokens, prediction_attributes, "arrival_time", arrival_time, sizeof(arrival_time));
				stop_object = find_included(response, tokens, root, "stop", stop_id);
				if (stop_object >= 0) {
					int stop_attributes = object_get(response, tokens, stop_object, "attributes");
					object_string(response, tokens, stop_attributes, "name", stop_name, sizeof(stop_name));
				} else if (stop_id[0] != '\0') {
					snprintf(stop_name, sizeof(stop_name), "Stop %s", stop_id);
				}
				if (arrival_time[0] != '\0') format_timestamp(arrival_time, eta, sizeof(eta));
				printf("- %s: %s\n", stop_name, eta);
			}
		} else {
			printf("No additional upcoming stop predictions are currently available.\n");
		}
		printf("Cross-check: ");
		if (relationship_id(response, tokens, best, "vehicle", confirmed_vehicle, sizeof(confirmed_vehicle)) &&
			strcmp(confirmed_vehicle, train->vehicle_id) == 0) {
			printf("prediction matches vehicle ID %s", train->vehicle_id);
		} else if (relationship_id(response, tokens, best, "trip", confirmed_trip, sizeof(confirmed_trip)) &&
			strcmp(confirmed_trip, train->trip_id) == 0) {
			printf("prediction matches trip ID %s", train->trip_id);
		} else {
			printf("vehicle association was not confirmed");
		}
		if (relationship_id(response, tokens, best, "route", confirmed_route, sizeof(confirmed_route)) &&
			strcmp(confirmed_route, train->route_id) != 0) {
			printf("; route mismatch reported by feed");
		}
		printf("\n");
	}
	free(tokens);
	free(response);
}
#undef printf

void format_train_details(const Train* train, char* output, unsigned long output_size)
{
	format_train_details_with_stops(train, output, output_size, NULL, 0, NULL);
}

void show_train_details(const Train* train)
{
	char output[8192];
	format_train_details(train, output, sizeof(output));
	fputs(output, stdout);
}
