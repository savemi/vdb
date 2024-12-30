#include <json-c/json.h>
#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include "http_media_server.h"
#include "media_management_service.h"

volatile bool shutdown_requested;

// Declare a global mutex for thread safety
pthread_mutex_t csv_mutex;

// Thread-safe access to csvArray
#define LOCK_CSV() pthread_mutex_lock(&csv_mutex)
#define UNLOCK_CSV() pthread_mutex_unlock(&csv_mutex)

// Thread function for the WebSocket server
void *start_websocket_server(void *arg) {
    json_object **csvArray = (json_object **)arg;  // Cast the void pointer back to json_object**

    if (media_server_init(csvArray) != 0) {  // media_server_init error code handle
        fprintf(stderr, "Failed to initialize WebSocket server\n");
        return NULL;
    }

    printf("WebSocket server is running\n");

    return NULL;
}

// Command Handlers
const char *handle_command(json_object *payload, json_object **csvArray) {
    static char response[8196];

    const char *command = json_object_get_string(json_object_object_get(payload, "command"));
    
    if (strcmp(command, "GET_MEDIA_LIST") == 0) {
        // Extract filters
        json_object *filter_obj, *filtered_array; 

        if (json_object_object_get_ex(payload, "filter", &filter_obj)) {
            const char *date_from = json_object_get_string(json_object_object_get(filter_obj, "date_from"));
            const char *date_to = json_object_get_string(json_object_object_get(filter_obj, "date_to"));
            int limit = json_object_get_int(json_object_object_get(filter_obj, "limit"));
            filter_events_by_date(date_from, date_to, limit,&filtered_array, csvArray);

            snprintf(response, sizeof(response), "{ \"status\": \"success\", \"media_list\": %s }", json_object_to_json_string(filtered_array));
            return strdup(response);
        }
        return "{ \"status\": \"error\", \"message\": \"Invalid payload format\" }";
    }

    if (strcmp(command, "MEDIA_DELETE") == 0) {
        json_object *recordings;
        if (json_object_object_get_ex(payload, "recordings", &recordings)) {
            delete_recordings(recordings,csvArray);
            snprintf(response, sizeof(response), "{ \"status\": \"success\", \"recordings\": %s }", json_object_to_json_string(recordings));
            return strdup(response);
        }
        return "{ \"status\": \"error\", \"message\": \"Invalid payload: recordings not provided\" }";
    }

	
    if (strcmp(command, "MEDIA_RECORDING_UPDATE") == 0) {
    json_object *recording_id, *filename, *size, *duration, *created_at, *type, *thumbnail_url;

    // Validate and extract fields from the payload
    if (json_object_object_get_ex(payload, "recording_id", &recording_id) &&
        json_object_object_get_ex(payload, "filename", &filename) &&
        json_object_object_get_ex(payload, "size", &size) &&
        json_object_object_get_ex(payload, "duration", &duration) &&
        json_object_object_get_ex(payload, "created_at", &created_at) &&
        json_object_object_get_ex(payload, "type", &type) &&
        json_object_object_get_ex(payload, "thumbnail_url", &thumbnail_url)) {


        // Send response payload
        snprintf(response, sizeof(response), "{ \"status\": \"OK\" }");
        return strdup(response);
    }

    // If any field is missing, return an error
       return "{ \"status\": \"error\", \"message\": \"Invalid payload: required fields missing\" }";
    }

    if (strcmp(command, "MEDIA_MANAGEMENT_STOP") == 0) {
        send_mq_data(MEDIA_MANAGEMENT_SERVICE, 11, COMMAND_RESPONSE, media_management, MEDIA_MANAGEMENT_STOP, NULL, 0);
        snprintf(response, sizeof(response), "{ \"status\": \"OK\" }");
        return strdup(response);
    }

    if (strcmp(command, "MEDIA_PLAYBACK") == 0) {
        json_object *recording_obj;
        if (json_object_object_get_ex(payload, "recording", &recording_obj)) {
            const char *recording_id = json_object_get_string(recording_obj);
            snprintf(response, sizeof(response), "{ \"status\": \"success\", \"download_url\": \"rtsps://IP:8554/%s.mp4\" }", recording_id);
	    send_mq_data(MEDIA_MANAGEMENT_SERVICE, 11, COMMAND_RESPONSE, media_playback, MEDIA_PLAYBACK_START, recording_id, strlen(recording_id));
            return strdup(response);
        }
        return "{ \"status\": \"error\", \"message\": \"Invalid payload: recording not provided\" }";
    }

    if (strcmp(command, "MEDIA_DOWNLOAD") == 0) {
        json_object *recording_obj;
        if (json_object_object_get_ex(payload, "recording", &recording_obj)) {
            const char *recording_id = json_object_get_string(recording_obj);
            snprintf(response, sizeof(response), "{ \"status\": \"success\", \"download_url\": \"https://vdb.local/media/%s.mp4\" }", recording_id);
	    send_mq_data(MEDIA_MANAGEMENT_SERVICE, 13, COMMAND_RESPONSE, media_download, MEDIA_DOWNLOAD_START, recording_id, strlen(recording_id));
            return strdup(response);
        }
        return "{ \"status\": \"error\", \"message\": \"Invalid payload: recording not provided\" }";
    }

    return "{ \"status\": \"error\", \"message\": \"Unknown command\" }";
}

void filter_events_by_date(const char *date_from, const char *date_to, int limit, json_object **filtered_array,json_object **csvArray ) {
    time_t start_date = parse_date(date_from);
    time_t end_date = parse_date(date_to);
    int num_events = json_object_array_length(*csvArray);
    
    printf("number of events = %d\n",num_events);
    *filtered_array = json_object_new_array();
    for (int i = 0; i < num_events && json_object_array_length(*filtered_array) < limit; i++) {
        json_object *event_obj = json_object_array_get_idx(*csvArray, i);
        const char *event_date_str = json_object_get_string(json_object_object_get(event_obj, "created_at"));
        if(!event_date_str) continue;
	time_t event_date = convert_to_timestamp(event_date_str); //convert_to_timestamp is used for csv timestamp
	
        
	if (event_date >= start_date && event_date <= end_date) {
	   
	    printf("added event \n");
            //json_object_array_add(*filtered_array, json_object_get(event_obj));
	    //
	    json_object *recording_id = json_object_object_get(event_obj, "created_at");
	    json_object *size = json_object_new_object();
    	    json_object *duration = json_object_new_object();
	    json_object *filename = json_object_object_get(event_obj, "video_path");
	    json_object *created_at = json_object_object_get(event_obj, "created_at");
	    json_object *thumbnail_url = json_object_object_get(event_obj, "thumbnail_url");
	    json_object *type = json_object_object_get(event_obj, "type");


        //videofilename
     	char file[64];

        snprintf(file, sizeof(file), "%s", json_object_get_string(filename));
	    // Get file size and duration
        long file_size = get_file_size(file);
        
    	//Duration info from the video
        double video_duration = get_video_duration(file);
          

    	// Add the extracted fields and computed info to the filtered object
    	   json_object *filtered_obj = json_object_new_object();
    	   json_object_object_add(filtered_obj, "recording_id", recording_id);
	   json_object_object_add(filtered_obj, "filename", filename);
	   if(file_size != -1){
    	   json_object_object_add(filtered_obj, "size", json_object_new_int(file_size));
	   }
	   if (video_duration != -1){
    	   json_object_object_add(filtered_obj, "duration", json_object_new_double(video_duration));
	   }
	   json_object_object_add(filtered_obj, "created_at", created_at);
       	   json_object_object_add(filtered_obj, "type", thumbnail_url);

	   

    	// Add filtered object to the array
    	   json_object_array_add(*filtered_array, filtered_obj);
        }
    }
}

void delete_recordings(json_object *recordings, json_object **csvArray) {

    int num_to_delete = json_object_array_length(recordings);
    int total_events = json_object_array_length(*csvArray);

    // Store indices of events to delete
    int *delete_indices = malloc(total_events * sizeof(int));
    int delete_count = 0;
    LOCK_CSV();
    // Iterate over all recordings to delete
    for (int i = 0; i < num_to_delete; i++) {
        // Get the recording ID to delete
        json_object *recording_obj = json_object_array_get_idx(recordings, i);
        if (!recording_obj) {
            fprintf(stderr, "Error: Null recording object at index %d\n", i);
            continue;
        }

        const char *recording_id = json_object_get_string(recording_obj);
        if (!recording_id) {
            fprintf(stderr, "Error: Null recording ID at index %d\n", i);
            continue;
        }

        // Iterate over the events array
        for (int j = 0; j < total_events; j++) {
            json_object *event_obj = json_object_array_get_idx(*csvArray, j);
            if (!event_obj) {
                fprintf(stderr, "Error: Null event object at index %d\n", j);
                continue;
            }

            // Get the "created_at" value (used as the recording ID)
            json_object *created_at_obj = NULL;
            if (!json_object_object_get_ex(event_obj, "created_at", &created_at_obj)) {
                fprintf(stderr, "Warning: Missing \"created_at\" in event at index %d\n", j);
                continue;
            }

            const char *current_id = json_object_get_string(created_at_obj);
            if (!current_id) {
                fprintf(stderr, "Warning: Null \"created_at\" value in event at index %d\n", j);
                continue;
            }

            printf("current id = %s, recording id = %s\n", current_id, recording_id);

            // Compare and mark for deletion if IDs match
            if (strcmp(recording_id, current_id) == 0) {
                fprintf(stdout, "Marking for deletion: recording with ID: %s\n", current_id);
                delete_indices[delete_count++] = j;
            }
        }
    }

    // Now, delete the events in reverse order to avoid shifting issues
    for (int i = delete_count - 1; i >= 0; i--) {
        int idx_to_delete = delete_indices[i];

        // Remove from array, which automatically frees the object and handles reference counting
        json_object_array_del_idx(*csvArray, idx_to_delete, 1);
        total_events--; // Adjust total_events after deletion
    }

    // Free the temporary delete indices array
    if(delete_indices){
    free(delete_indices);
    }
    UNLOCK_CSV();
    
}

static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    json_object **csvArray = (json_object **)lws_context_user(lws_get_context(wsi)); // Retrieve global csvArray

    switch (reason) {
    case LWS_CALLBACK_RECEIVE: {
        printf("Received payload: %.*s\n", (int)len, (const char *)in);

    json_object *input_request_json = json_tokener_parse((const char *)in);
    if (!input_request_json) {
        const char *error = "{ \"status\": \"error\", \"message\": \"Invalid JSON payload\" }";
        lws_write(wsi, (unsigned char *)error, strlen(error), LWS_WRITE_TEXT);
        return 0;
    }

    //const char *command = json_object_get_string(json_object_object_get(input_request_json, "command"));
    
   
    const char *response = handle_command(input_request_json, csvArray);
    if (response) {
        lws_write(wsi, (unsigned char *)response, strlen(response), LWS_WRITE_TEXT);
        json_object_put(input_request_json);
    }
    
            break;
        }

    default:
        break;
    }

    return 0;
}



// WebSocket Callback

static struct lws_protocols protocols[] = {
    {
        "websocket-protocol",
        websocket_callback,
        0,
        8192,
    },
    { NULL, NULL, 0, 0 } // Terminating entry
};

int media_server_init(json_object **csvArray)  {
	shutdown_requested = false;
    struct lws_context_creation_info info = {0};
    info.port = HTTP_MEDIA_PORT;
    info.protocols = protocols; 
    info.user = csvArray;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Failed to create WebSocket context\n");
        return ENOMEM;  // Memory allocation failure for context creation
    }

    printf("WebSocket server running on ws://localhost:%d\n",info.port);

    while (!shutdown_requested && lws_service(context, 1000) >= 0);

    // Destroy the WebSocket context, although it normally should not fail
    lws_context_destroy(context);

    return 0;
}

int media_management_start(pthread_t *ws_thread, json_object **csvArray) {
    FILE *file;

    // Load the CSV log file if csvArray is NULL
    if (!*csvArray) {
        char line[1024];

        // Open CSV file for reading
        file = fopen(csv_file, "r");
        if (file == NULL) {
            perror("Error opening file");
            return ENOENT;  // Return error code for file not found (No such file or directory)
        }

        // Read first line (assuming it's a header or needs to be skipped)
        if (fgets(line, sizeof(line), file) == NULL) {
            perror("Error reading the first line of the file");
            fclose(file);
            return EIO;  // Return error code for I/O error
        }

        // Create a new JSON array to store the CSV data
        *csvArray = json_object_new_array();
        if (!*csvArray) {
            fprintf(stderr, "Error creating events array\n");
            fclose(file);
            return ENOMEM;  // Return error code for memory allocation failure
        }

        // Read and parse each subsequent line from the CSV file
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0;  // Remove newline character

            json_object *eventJson = csv_to_jsonArray(line);
            if (eventJson) {
                json_object_array_add(*csvArray, eventJson);  // Add parsed event to the array
            }
        }

        fclose(file);  // Close the file after reading
    }

    printf("Before starting WebSocket server thread...\n");

    // Start the WebSocket server in a new thread
    if (pthread_create(ws_thread, NULL, start_websocket_server, csvArray) != 0) {
        perror("Error creating WebSocket server thread");
        return EAGAIN;  // Return error code for resource temporarily unavailable (thread creation failure)
    }

    // Optionally, detach the thread if not joining
    pthread_detach(*ws_thread);

    return 0;  // Return 0 on success
}

int media_management_stop(json_object **csvArray){

    printf("Stopping WebSocket server thread\n");
    volatile bool shutdown_requested;
  /*
    if (pthread_cancel(ws_thread) != 0) {
        perror("Error cancelling WebSocket server thread");
    	} else {
        	printf("WebSocket server thread stopped successfully.\n");
   	}
    */

    update_csv_from_json(csvArray);

    return NULL;
}

