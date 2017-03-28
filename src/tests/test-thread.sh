#!/bin/sh

cc test-thread.c ../afb-thread.c ../verbose.c ../sig-monitor.c ../jobs.c -o test-thread -lrt -lpthread -I../../include -g -O2
gdb ./test-thread
