CC = gcc

CLFAGS += -std=c11 -I/usr/include/fuse3 -lfuse3 -lpthread -Wall -W -D_FILE_OFFSET_BITS=64

TARGET = fuse-upperfs

SOURCE = fuse-upperfs.c

fuse-upperfs:
	$(CC) -o $(TARGET) $(CLFAGS) $(SOURCE)
install:
	sudo cp fuse-upperfs /usr/bin
.PHONY: clean
clean:
	rm -f fuse-upperfs
