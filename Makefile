all: snap

snap: snap.c
	cc -g -Wall -O3 -o snap snap.c -lexpat
