#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <json-c/json.h>

#define WEBSOCKET_PORT 7681
#define DOWNLOAD_PROTOCOL "download-protocol"
#define VIDEO_FILE "./9_122324142316.mp4"  // Path to your video file
#define DOWNLOAD_REQUEST "DOWNLOAD_VIDEO"  // Command to trigger video download
#define BUFFER_SIZE 1024  // Buffer size for sending video chunks

// Mutex for thread-safety with JSON parsing
pthread_mutex_t json_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototype for the callback
static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

// WebSocket protocol setup
static struct lws_protocols protocols[] = {
    {
        .name = DOWNLOAD_PROTOCOL,
        .callback = websocket_callback,
    },
    { NULL, NULL }  // Null terminator
};

// WebSocket context creation information
static struct lws_context_creation_info info = {
    .port = WEBSOCKET_PORT,
    .protocols = protocols,
    .iface = NULL, // Use default interface
    .gid = -1, // Default group ID
    .uid = -1, // Default user ID
    .options = 0,
};

// Callback function for WebSocket events
static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_RECEIVE: {
        printf("Received payload: %.*s\n", (int)len, (const char *)in);

        pthread_mutex_lock(&json_mutex);
        json_object *input_request_json = json_tokener_parse((const char *)in);
        pthread_mutex_unlock(&json_mutex);
        
        if (input_request_json == NULL) {
            unsigned char error_json[] = "{ \"status\": \"error\", \"message\": \"Invalid JSON payload\" }";
            lws_write(wsi, error_json, strlen(error_json), LWS_WRITE_TEXT);
            return 0;
        }

        // Parse the command from JSON
        json_object *command_json;
        if (json_object_object_get_ex(input_request_json, "command", &command_json)) {
            const char *command = json_object_get_string(command_json);
            
            if (strcmp(command, DOWNLOAD_REQUEST) == 0) {
                // Trigger video download handling
                FILE *file = fopen(VIDEO_FILE, "rb");
                if (file == NULL) {
                    unsigned char error_file[] = "{ \"status\": \"error\", \"message\": \"Video file not found\" }";
                    lws_write(wsi, error_file, strlen(error_file), LWS_WRITE_TEXT);
                    break;
                }

                // Send video file in chunks to the client
                unsigned char buffer[BUFFER_SIZE];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                    int ret = lws_write(wsi, buffer, bytes_read, LWS_WRITE_BINARY);
                    if (ret < 0) {
                        fprintf(stderr, "Failed to send video data to client\n");
                        break;
                    }
                }
                fclose(file);
                printf("Video sent to client.\n");

            } else {
                unsigned char error_command[] = "{ \"status\": \"error\", \"message\": \"Unknown command\" }";
                lws_write(wsi, error_command, strlen(error_command), LWS_WRITE_TEXT);
            }
        } else {
            unsigned char error_json[] = "{ \"status\": \"error\", \"message\": \"Missing 'command' key in JSON\" }";
            lws_write(wsi, error_json, strlen(error_json), LWS_WRITE_TEXT);
        }

        pthread_mutex_lock(&json_mutex);
        json_object_put(input_request_json);
        pthread_mutex_unlock(&json_mutex);

        break;
    }

    case LWS_CALLBACK_ESTABLISHED: {
        // New connection established
        printf("WebSocket connection established\n");
        break;
    }

    case LWS_CALLBACK_CLOSED: {
        // Connection closed
        printf("WebSocket connection closed\n");
        break;
    }

    default:
        break;
    }

    return 0;
}

int main() {
    // Create the WebSocket context (server)
    struct lws_context *context =  lws_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "Failed to create WebSocket context\n");
        return 1;
    }

    printf("WebSocket server is running on ws://localhost:%d\n", WEBSOCKET_PORT);

    // Enter the event loop (this will run indefinitely, handling WebSocket events)
    while (1) {
        // The `libwebsocket_service` function is the event loop that handles WebSocket events.
        // The second parameter is the timeout (in milliseconds) between events.
        int ret = lws_service(context, 100);
        if (ret < 0) {
            fprintf(stderr, "libwebsocket_service failed\n");
            break;
        }
    }

    // Clean up and close the WebSocket context when done
    lws_context_destroy(context);
    return 0;
}

