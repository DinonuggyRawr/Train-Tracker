#ifndef TRAIN_TRACKER_H
#define TRAIN_TRACKER_H

#define MAX_TRAINS 500
#define MAX_RADIO_CHANNELS 64
#define MAX_ROUTE_ALERTS 64
#define MAX_UPCOMING_STOPS 5
#define MAX_MAP_ROUTE_LINES 128
#define MAX_MAP_ROUTE_POINTS 1500
#define TRAIN_SOURCE_MBTA 0
#define TRAIN_SOURCE_AMTRAK 1

typedef struct TrainStopPrediction {
	char name[96];
	char arrival[48];
	char departure[48];
	char scheduled_arrival[48];
	double delay_minutes;
	int has_delay;
} TrainStopPrediction;

typedef struct Train {
	int source;
	char vehicle_id[64];
	char car_label[32];
	char route_id[32];
	char route_name[96];
	char destination[96];
	char current_status[32];
	char current_stop[96];
	char updated_at[48];
	char trip_id[64];
	char next_stop[96];
	char next_arrival[48];
	char next_departure[48];
	int upcoming_stop_count;
	TrainStopPrediction upcoming_stops[MAX_UPCOMING_STOPS];
	int route_type;
	int current_stop_sequence;
	int has_location;
	double latitude;
	double longitude;
	double speed_mph;
	int has_speed;
} Train;

typedef struct RadioChannel {
	const char* line;
	const char* channel_type;
	const char* aar_channel;
	double frequency_mhz;
	const char* notes;
} RadioChannel;

typedef struct RouteAlert {
	char effect[32];
	char header[256];
} RouteAlert;

typedef struct MapRouteLine {
	char route_id[32];
	char route_name[96];
	int point_count;
	double latitude[MAX_MAP_ROUTE_POINTS];
	double longitude[MAX_MAP_ROUTE_POINTS];
} MapRouteLine;

int fetch_trains(Train* trains, int capacity, char* error, unsigned long error_size);
void show_train_list(const Train* trains, int count);
void show_train_details(const Train* train);
void format_train_details(const Train* train, char* output, unsigned long output_size);
void format_train_details_with_stops(const Train* train, char* output, unsigned long output_size,
	TrainStopPrediction* stops, int stop_capacity, int* stop_count);
int train_matches_keyword(const Train* train, const char* keyword);
double haversine_miles(double lat1, double lon1, double lat2, double lon2);
int find_station(const char* text, double* latitude, double* longitude);
void update_speed_estimates(Train* trains, int count);
void show_nearest_trains(const Train* trains, int count, double latitude, double longitude, int limit);
void show_route_summary(const Train* trains, int count);
int fetch_route_alerts(const char* route_id, RouteAlert* alerts, int capacity, char* error, unsigned long error_size);
int fetch_map_route_lines(MapRouteLine* lines, int capacity, char* error, unsigned long error_size);
void show_alerts(const char* route_id);
int search_radio_frequencies(const char* keyword, RadioChannel* results, int capacity);
void show_radio_frequencies(const char* keyword);

#endif
