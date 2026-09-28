#include "TrainTracker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>

static int is_filler_word(const char* word)
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

static int parse_location(const char* text, double* latitude, double* longitude)
{
	char buffer[64];
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

static void show_help(void)
{
	printf("\nAvailable commands:\n");
	printf("----------------------------------------------------------------------------------------------\n");
	printf("  <vehicle ID, car number, or Amtrak train/run number>\n");
	printf("      Show full details for one train.\n");
	printf("      Example: 1234\n\n");
	printf("  list | show | find | search <description>\n");
	printf("      Filter the active train list by keyword(s): 'heritage', 'amtrak', 'mbta'/'commuter',\n");
	printf("      or any text matched against line, destination, stop, or ID. Filler words like\n");
	printf("      'all', 'active', 'trains', 'currently' are ignored.\n");
	printf("      Examples: list all heritage trains active\n");
	printf("                find Providence\n\n");
	printf("  routes\n");
	printf("      Show a summary of active train counts (and heritage counts) grouped by line.\n\n");
	printf("  nearest <lat,lon or city/station name>\n");
	printf("      List the 10 closest active trains to a location.\n");
	printf("      Examples: nearest 42.35,-71.06\n");
	printf("                nearest Worcester\n\n");
	printf("  distance <train id> <lat,lon or city/station name>\n");
	printf("      Show how far a specific train currently is from a location.\n");
	printf("      Example: distance 1234 Providence, RI\n\n");
	printf("  alerts <train id or route id>\n");
	printf("      Show active MBTA service alerts for a train's route (MBTA only).\n");
	printf("      Example: alerts 1234\n\n");
	printf("  freq <line and/or road/dispatch/yard keywords>\n");
	printf("      Search the radio channel reference table by line name and/or channel type.\n");
	printf("      Filler words like 'for', 'the', 'line', 'channel' are ignored.\n");
	printf("      Examples: freq road for Fitchburg Line\n");
	printf("                freq dispatch Old Colony\n\n");
	printf("  watch\n");
	printf("      Refresh the active train list every 30 seconds; press any key to stop.\n\n");
	printf("  help | ?\n");
	printf("      Show this list of commands.\n\n");
	printf("  Q\n");
	printf("      Quit.\n");
}

static void run_watch_mode(Train* trains, int* count)
{
	printf("\nEntering watch mode. Refreshing every 30 seconds. Press any key to stop watching.\n");
	for (;;) {
		int ticks = 0;
		while (ticks < 60) {
			if (_kbhit()) {
				_getch();
				printf("\nStopped watching.\n");
				return;
			}
			Sleep(500);
			++ticks;
		}
		{
			Train updated[MAX_TRAINS];
			char error[256];
			int new_count = fetch_trains(updated, MAX_TRAINS, error, sizeof(error));
			if (new_count < 0) {
				printf("Refresh failed: %s\n", error);
				continue;
			}
			update_speed_estimates(updated, new_count);
			memcpy(trains, updated, sizeof(Train) * new_count);
			*count = new_count;
			printf("\n--- Refreshed: %d active train(s) ---\n", new_count);
			show_train_list(trains, *count);
			printf("(Watching... press any key to stop)\n");
		}
	}
}

static void run_descriptive_search(const Train* trains, int count, char* query)
{
	char* keywords[10];
	int keyword_count = 0;
	char* token;
	char* context = NULL;
	Train matches[MAX_TRAINS];
	int match_count = 0;
	int index;
	int keyword_index;

	token = strtok_s(query, " \t", &context);
	while (token != NULL && keyword_count < 10) {
		if (!is_filler_word(token)) {
			keywords[keyword_count++] = token;
		}
		token = strtok_s(NULL, " \t", &context);
	}
	if (keyword_count == 0) {
		printf("Please describe what kind of train you're looking for (e.g. 'list heritage trains').\n");
		return;
	}
	for (index = 0; index < count; ++index) {
		int matched_all = 1;
		for (keyword_index = 0; keyword_index < keyword_count; ++keyword_index) {
			if (!train_matches_keyword(&trains[index], keywords[keyword_index])) {
				matched_all = 0;
				break;
			}
		}
		if (matched_all) {
			matches[match_count++] = trains[index];
		}
	}
	printf("\nSearch results for");
	for (keyword_index = 0; keyword_index < keyword_count; ++keyword_index) {
		printf(" '%s'", keywords[keyword_index]);
	}
	printf(":\n");
	show_train_list(matches, match_count);
}

int main(void)
{
	Train trains[MAX_TRAINS];
	char error[256];
	char query[160];
	int count = fetch_trains(trains, MAX_TRAINS, error, sizeof(error));
	int index;

	if (count < 0) {
		fprintf(stderr, "Could not load MBTA train data: %s\n", error);
		return 1;
	}
	if (error[0] != '\0') {
		fprintf(stderr, "Warning: %s\n", error);
	}
	update_speed_estimates(trains, count);

	printf("MBTA commuter rail and Amtrak tracker\n");
	printf("MBTA commuter rail: MBTA V3 API. Amtrak: Amtraker API v3 (amtraker.com, ODC-By 1.0).\n");
	printf("No free live freight train location feed was available; freight trains are not listed.\n");
	printf("Type 'help' at any prompt to see all available commands.\n\n");
	show_train_list(trains, count);

	for (;;) {
		char first_word[16];
		char* space;
		size_t word_length;
		char* remainder;

		printf("\nEnter a command ('help' for the full list, Q to quit): ");
		if (fgets(query, sizeof(query), stdin) == NULL) {
			break;
		}
		query[strcspn(query, "\r\n")] = '\0';
		if (query[0] == '\0' || strcmp(query, "Q") == 0 || strcmp(query, "q") == 0) {
			break;
		}

		space = strchr(query, ' ');
		word_length = space != NULL ? (size_t)(space - query) : strlen(query);
		if (word_length >= sizeof(first_word)) word_length = sizeof(first_word) - 1;
		memcpy(first_word, query, word_length);
		first_word[word_length] = '\0';
		remainder = query + word_length;
		while (*remainder == ' ') ++remainder;

		if (_stricmp(query, "help") == 0 || _stricmp(query, "?") == 0) {
			show_help();
			continue;
		}

		if (_stricmp(first_word, "list") == 0 || _stricmp(first_word, "show") == 0 ||
			_stricmp(first_word, "find") == 0 || _stricmp(first_word, "search") == 0) {
			run_descriptive_search(trains, count, query);
			continue;
		}

		if (_stricmp(query, "routes") == 0) {
			show_route_summary(trains, count);
			continue;
		}

		if (_stricmp(query, "watch") == 0) {
			run_watch_mode(trains, &count);
			continue;
		}

		if (_stricmp(first_word, "nearest") == 0) {
			double latitude;
			double longitude;
			if (parse_location(remainder, &latitude, &longitude)) {
				show_nearest_trains(trains, count, latitude, longitude, 10);
			} else {
				printf("Could not understand location '%s'. Try 'nearest 42.35,-71.06' or 'nearest Providence'.\n",
					remainder);
			}
			continue;
		}

		if (_stricmp(first_word, "distance") == 0) {
			char id_buffer[64];
			char* id_space = strchr(remainder, ' ');
			size_t id_length = id_space != NULL ? (size_t)(id_space - remainder) : strlen(remainder);
			if (id_length == 0 || id_length >= sizeof(id_buffer)) {
				printf("Usage: distance <train id> <lat,lon or station name>\n");
				continue;
			}
			memcpy(id_buffer, remainder, id_length);
			id_buffer[id_length] = '\0';
			for (index = 0; index < count; ++index) {
				if (strcmp(id_buffer, trains[index].vehicle_id) == 0 ||
					strcmp(id_buffer, trains[index].car_label) == 0) {
					break;
				}
			}
			if (index == count) {
				printf("No active train matches '%s'.\n", id_buffer);
			} else if (!trains[index].has_location) {
				printf("Train %s does not currently report a position.\n", id_buffer);
			} else {
				double latitude;
				double longitude;
				const char* location_text = id_space != NULL ? id_space + 1 : "";
				while (*location_text == ' ') ++location_text;
				if (parse_location(location_text, &latitude, &longitude)) {
					double distance = haversine_miles(trains[index].latitude, trains[index].longitude,
						latitude, longitude);
					printf("Train %s is about %.1f miles from that location.\n", id_buffer, distance);
				} else {
					printf("Could not understand location '%s'.\n", location_text);
				}
			}
			continue;
		}

		if (_stricmp(first_word, "alerts") == 0) {
			if (remainder[0] == '\0') {
				printf("Usage: alerts <train id> or alerts <route id>\n");
			} else {
				const char* route_id = remainder;
				for (index = 0; index < count; ++index) {
					if (strcmp(remainder, trains[index].vehicle_id) == 0 ||
						strcmp(remainder, trains[index].car_label) == 0) {
						if (trains[index].source == TRAIN_SOURCE_AMTRAK) {
							printf("Live alerts are only available for MBTA commuter rail routes.\n");
							route_id = NULL;
						} else {
							route_id = trains[index].route_id;
						}
						break;
					}
				}
				if (route_id != NULL && route_id[0] != '\0') {
					show_alerts(route_id);
				}
			}
			continue;
		}

		if (_stricmp(first_word, "freq") == 0 || _stricmp(first_word, "frequency") == 0 ||
			_stricmp(first_word, "frequencies") == 0) {
			show_radio_frequencies(remainder);
			continue;
		}

		for (index = 0; index < count; ++index) {
			if (strcmp(query, trains[index].vehicle_id) == 0 ||
				strcmp(query, trains[index].car_label) == 0) {
				show_train_details(&trains[index]);
				break;
			}
		}
		if (index == count) {
			printf("No active train matches '%s'. Check the number and try again.\n", query);
		}
	}

	return 0;
}

