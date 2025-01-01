#include <libsoup-2.4/libsoup/soup.h>
#include <stdio.h>
#include <stdlib.h>

#define VIDEO_FILE_PATH "./9_122324142316.mp4"  // Specify the full path to your video file

// Callback to handle HTTP requests
static void on_request(SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext *client, gpointer user_data) {
    if (g_strcmp0(path, "/download") == 0) {  // Check if the requested path is "/download"
        FILE *video_file = fopen(VIDEO_FILE_PATH, "rb");
        if (video_file) {
            // Get the file size
            fseek(video_file, 0, SEEK_END);
            long file_size = ftell(video_file);
            fseek(video_file, 0, SEEK_SET);

            // Set headers for downloading the file
            soup_message_headers_set_content_type(msg->response_headers, "video/mp4", NULL);
            soup_message_headers_set_content_disposition(msg->response_headers, "attachment", NULL);

            // Set the Content-Length header
            char content_length[32];
            snprintf(content_length, sizeof(content_length), "%ld", file_size);
            soup_message_headers_append(msg->response_headers, "Content-Length", content_length);

            // Send the file in chunks
            char buffer[1024];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), video_file)) > 0) {
                soup_message_body_append(msg->response_body, SOUP_MEMORY_STATIC, buffer, bytes_read);
            }

            fclose(video_file);
        } else {
            // If the file doesn't exist, send a 404 Not Found error
            soup_message_set_status(msg, SOUP_STATUS_NOT_FOUND);
        }
    } else {
        // Default response for other paths
        soup_message_set_status(msg, SOUP_STATUS_NOT_FOUND);
        soup_message_body_append(msg->response_body, SOUP_MEMORY_STATIC, "Not Found", 9);
    }
}

int main(int argc, char *argv[]) {
    // Initialize libsoup
    SoupServer *server = soup_server_new(SOUP_SERVER_SERVER_HEADER, "video-server", NULL);

    // Set up the request handler
    soup_server_add_handler(server, NULL, on_request, NULL, NULL);

    // Start the server on port 5003
    if (soup_server_listen_all(server, 5003, SOUP_SERVER_LISTEN_IPV4_ONLY, NULL)) {
        printf("Server running on http://localhost:5003\n");

        // Run the server event loop
        GMainLoop *loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(loop);
    } else {
        fprintf(stderr, "Failed to start server.\n");
        return 1;
    }

    return 0;
}

