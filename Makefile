CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
LDFLAGS =

SRCS    = phone_db.c test_phone_db.c
OBJS    = phone_db.o test_phone_db.o
TU_OBJS = phone_db.o test_units.o

.PHONY: all test test-units clean

all: all_tests

phone_db_test: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

phone_db_units: $(TU_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

phone_db.o: phone_db.c phone_db.h
	$(CC) $(CFLAGS) -c -o $@ $<

test_phone_db.o: test_phone_db.c phone_db.h test_framework.h
	$(CC) $(CFLAGS) -c -o $@ $<

test_units.o: test_units.c phone_db.h test_framework.h
	$(CC) $(CFLAGS) -c -o $@ $<

all_tests: phone_db_test phone_db_units
	mkdir -p test_data
	./phone_db_test
	./phone_db_units || true

test: phone_db_test
	mkdir -p test_data
	./phone_db_test

test-units: phone_db_units
	./phone_db_units

clean:
	rm -f phone_db_test phone_db_units $(OBJS) test_units.o test_data/*.csv
