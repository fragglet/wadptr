PREFIX = /usr/local
EXECUTABLE = wadptr
OBJECTS = main.o waddir.o errors.o wadmerge.o lumps.o
DELETE = rm -f
CFLAGS = -Wall -O3 -DNORMALUNIX

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

main.o: main.c wadptr.h errors.h waddir.h wadmerge.h lumps.h
	$(CC) $(CFLAGS) -c main.c

waddir.o: waddir.c waddir.h errors.h
	$(CC) $(CFLAGS) -c waddir.c

errors.o: errors.c errors.h
	$(CC) $(CFLAGS) -c errors.c

lumps.o: lumps.c lumps.h waddir.h errors.h
	$(CC) $(CFLAGS) -c lumps.c

wadmerge.o: wadmerge.c wadmerge.h waddir.h errors.h
	$(CC) $(CFLAGS) -c wadmerge.c

install:
	install -D wadptr $(DESTDIR)$(PREFIX)/bin/wadptr
	install -D wadptr.txt $(DESTDIR)$(PREFIX)/share/doc/wadptr/wadptr.txt

clean:
	$(DELETE) $(EXECUTABLE)
	$(DELETE) *.o

.PHONY: install clean

