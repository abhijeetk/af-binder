#!/bin/sh

cc test-thread.c ../afb-thread.c ../verbose.c ../sig-monitor.c ../jobs.c -o test-thread -lrt -lpthread -lsystemd I../../include -g 
./test-thread
