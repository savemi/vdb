CC = gcc
CFLAGS = $(shell pkg-config --cflags gstreamer-1.0 glib-2.0 libsoup-2.4 libwebsockets json-c)
LIBS = $(shell pkg-config --libs gstreamer-1.0 glib-2.0 libsoup-2.4 libwebsockets json-c)

websocket_download_server: websocket_download_server.o
	$(CC) -o websocket_download_server websocket_download_server.o $(LIBS) -lpthread -lcjson -g

websocket_download_server.o: websocket_download_server.c
	$(CC) $(CFLAGS) -c websocket_download_server.c -lpthread -g

clean:
	rm -f websocket_download_server websocket_download_server.o

