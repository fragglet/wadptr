PREFIX = /usr/local
EXECUTABLE = wadptr
OBJECTS = main.o waddir.o errors.o wadmerge.o lumps.o
DELETE = rm -f
STRIP = strip
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

windist:
	rm -rf dist
	mkdir dist
	cp wadptr.exe wadptr.txt dist/
	$(STRIP) dist/wadptr.exe
	cp COPYING.md dist/COPYING.txt
	unix2dos -f dist/COPYING.txt dist/wadptr.txt
	rm -f wadptr-$(VERSION).zip
	zip -X -j -r wadptr-$(VERSION).zip dist

.PHONY: install clean dist

