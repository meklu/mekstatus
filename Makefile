CC = gcc
CFLAGS = -Wall -Wextra -pedantic -Os
LDFLAGS = -lrt

TARGETS = mekstatus

all : $(TARGETS)

fall : clean $(TARGETS)

mekstatus : mekstatus.c
	$(CC) $(CFLAGS) $(LDFLAGS) mekstatus.c -o mekstatus

clean :
	rm -f $(TARGETS)
