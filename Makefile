PREFIX = /usr/local
EXECUTABLE = wadptr
OBJECTS = main.o waddir.o errors.o wadmerge.o \
          graphics.o sidedefs.o blockmap.o sha1.o
DELETE = rm -f
STRIP = strip
CFLAGS = -Wall -O3

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

main.o: main.c wadptr.h errors.h waddir.h wadmerge.h \
        blockmap.h graphics.h sidedefs.h
	$(CC) $(CFLAGS) -c main.c

waddir.o: waddir.c waddir.h errors.h
	$(CC) $(CFLAGS) -c waddir.c

errors.o: errors.c errors.h
	$(CC) $(CFLAGS) -c errors.c

graphics.o: graphics.h wadptr.h
	$(CC) $(CFLAGS) -c graphics.c

sidedefs.o: sidedefs.h wadptr.h
	$(CC) $(CFLAGS) -c sidedefs.c

blockmap.o: blockmap.h wadptr.h
	$(CC) $(CFLAGS) -c blockmap.c

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

quickcheck: $(EXECUTABLE)
	git submodule update --checkout
	make -C quickcheck clean
	make -C quickcheck wads
	./wadptr -q -c quickcheck/extract/*.wad
	make -C quickcheck check
	# We check the demos play back if we uncompress the WADs again.
	./wadptr -q -u quickcheck/extract/*.wad
	make -C quickcheck check

.PHONY: install clean dist quickcheck

