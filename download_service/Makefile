CC = gcc
CFLAGS = $(shell pkg-config --cflags gstreamer-1.0 glib-2.0 libsoup-2.4)
LIBS = $(shell pkg-config --libs gstreamer-1.0 glib-2.0 libsoup-2.4)

video_server: video_server.o
	$(CC) -o video_server video_server.o $(LIBS)

video_server.o: video_server.c
	$(CC) $(CFLAGS) -c video_server.c

clean:
	rm -f video_server video_server.o

