#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>

#define HTTP_PORT 8080
#define VIDEO_FILE "./9_122324142316.mp4"  // Path to your video file

// HTTP response for the video file
/*
static int http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    if (reason == LWS_CALLBACK_HTTP) {
        // Check if it's a request for the video file
            char uri[1024];
        
        // Retrieve the URI from the request using WSI_TOKEN_GET_URI
        int uri_len = lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);

        // Check if the URI was properly copied
        if (uri_len <= 0) {
            lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
            return -1;
        }

        // Debugging output
        printf("Received request for URI: %s\n", uri);

        // Check if the request is for the video file
        if (strcmp(uri, "/video") == 0) {
            printf("Serving video file...\n");  // Debugging output

            // Serve the video file
            FILE *file = fopen(VIDEO_FILE, "rb");
            if (!file) {
                printf("File not found: %s\n", VIDEO_FILE);  // Debugging output
                lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
                return -1;
            }

            // Prepare the HTTP headers to send
	     // Prepare the HTTP headers to send
            unsigned char *header_buffer = malloc(1024);  // Allocate memory for headers buffer
            unsigned char *p = header_buffer;  // Pointer to the current position in the buffer
            unsigned char *end = header_buffer + 1024;  // End of the buffer

	    int ret = lws_add_http_header_by_name(wsi, "Content-Type", "video/mp4", strlen("video/mp4"), &p, end);
            
	     if (ret < 0) {
                printf("Error adding Content-Type header\n");  // Debugging output
                free(p);
                fclose(file);
                lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
                return -1;
            }

	    ret = lws_add_http_header_by_name(wsi, "Content-Disposition", "attachment; filename=\"download.mp4\"", strlen("attachment; filename=\"download.mp4\""), &p, end);
            if (ret < 0) {
                printf("Error adding Content-Disposition header\n");  // Debugging output
                free(header_buffer);
                fclose(file);
                lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
                return -1;
            }

            // Send the HTTP status code
	    // Send the video file in chunks
            unsigned char buffer[1024];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                ret = lws_write(wsi, buffer, bytes_read, LWS_WRITE_BINARY);
                if (ret < 0) {
                    printf("Error sending video data\n");  // Debugging output
                    free(header_buffer);
                    fclose(file);
                    lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
                    return -1;
                }
            }

            // Cleanup
            fclose(file);
            free(header_buffer);
            printf("Video file sent successfully\n");  // Debugging output
        } else {
            lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
        }
    }
    return 0;
}
*/

static int http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    if (reason == LWS_CALLBACK_HTTP) {
        char uri[1024];
        int uri_len = lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);
        if (uri_len <= 0) {
            lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
            return -1;
        }

        if (strcmp(uri, "/video") == 0) {
            FILE *file = fopen(VIDEO_FILE, "rb");
            if (!file) {
                lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
                return -1;
            }

            unsigned char buffer[LWS_PRE + 1024];
            unsigned char *p = &buffer[LWS_PRE];
            unsigned char *end = &buffer[sizeof(buffer) - LWS_PRE];
            if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end) ||
                lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                    (unsigned char *)"video/mp4", 9, &p, end) ||
                lws_finalize_http_header(wsi, &p, end)) {
                fclose(file);
                return -1;
            }

            if (lws_write(wsi, &buffer[LWS_PRE], p - &buffer[LWS_PRE], LWS_WRITE_HTTP_HEADERS) < 0) {
                fclose(file);
                return -1;
            }

            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer) - LWS_PRE, file)) > 0) {
                if (lws_write(wsi, buffer, bytes_read, LWS_WRITE_HTTP) < 0) {
                    fclose(file);
                    return -1;
                }
            }
            fclose(file);
            lws_http_transaction_completed(wsi);
        } else {
            lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
        }
    }
    return 0;
}

// HTTP protocol setup
static struct lws_protocols protocols[] = {
    {
        .name = "http",
        .callback = http_callback,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
    },
    { NULL, NULL, 0, 0 }  // Null terminator
};

int main() {
    // Setup and create the HTTP context (server)
    struct lws_context_creation_info info = {
        .port = HTTP_PORT,
        .protocols = protocols,
        .iface = NULL,
        .gid = -1,
        .uid = -1,
        .options = 0,
    };

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Failed to create HTTP context\n");
        return 1;
    }

    printf("HTTP server is running on http://localhost:%d\n", HTTP_PORT);

    // Enter the event loop
    while (1) {
        int ret = lws_service(context, 100);
        if (ret < 0) {
            fprintf(stderr, "libwebsocket_service failed\n");
            break;
        }
    }

    lws_context_destroy(context);
    return 0;
}

