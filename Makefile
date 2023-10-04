PREFIX = /usr/local
EXECUTABLE = wadptr
OBJECTS = main.o waddir.o errors.o wadmerge.o lumps.o sha1.o
DELETE = rm -f
STRIP = strip
CFLAGS = -Wall -O3

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

sha1.o: sha1.c sha1.h
	$(CC) $(CFLAGS) -c sha1.c

wadmerge.o: wadmerge.c wadmerge.h waddir.h errors.h
	$(CC) $(CFLAGS) -c wadmerge.c

install:
	install -D wadptr $(DESTDIR)$(PREFIX)/bin/wadptr
	install -D wadptr.1 $(DESTDIR)$(PREFIX)/share/man/man1/wadptr.1

clean:
	$(DELETE) $(EXECUTABLE)
	$(DELETE) *.o

windist:
	rm -rf dist
	mkdir dist
	cp wadptr.exe dist/
	$(STRIP) dist/wadptr.exe
	pandoc -f gfm -o dist/NEWS.html -s NEWS.md
	pandoc -f gfm -o dist/COPYING.html -s COPYING.md
	pandoc --template=default.html5 -f man wadptr.1 -o dist/wadptr.html
	rm -f wadptr-$(VERSION).zip
	zip -X -j -r wadptr-$(VERSION).zip dist

.PHONY: install clean dist

