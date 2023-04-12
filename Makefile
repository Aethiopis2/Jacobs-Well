CC = g++
CFLAGS = -O2 -Wall

all: bin/local-buddy bin/remote-buddy

bin/local-buddy: src/local-buddy.cpp src/net-wrappers.cpp include/net-wrappers.h include/utils.h
	$(CC) $(CFLAGS) -Iinclude src/local-buddy.cpp src/net-wrappers.cpp src/utils.cpp -o bin/local-buddy

bin/remote-buddy: src/remote-buddy.cpp src/net-wrappers.cpp include/net-wrappers.h include/utils.h
	$(CC) $(CFLAGS) -Iinclude src/remote-buddy.cpp src/net-wrappers.cpp src/utils.cpp -o bin/remote-buddy