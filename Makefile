PREFIX = /usr/local
MANPATH = $(PREFIX)/share/man
EXECUTABLE = wadptr
OBJECTS = main.o waddir.o errors.o wadmerge.o sort.o \
          graphics.o sidedefs.o blockmap.o sha1.o
DELETE = rm -f
STRIP = strip
IWYU = iwyu
CFLAGS = -Wall -O3 -Wextra
IWYU = iwyu
IWYU_FLAGS = --error --mapping_file=.iwyu-overrides.imp
IWYU_TRANSFORMED_FLAGS = $(patsubst %,-Xiwyu %,$(IWYU_FLAGS)) $(CFLAGS)
PANDOC_FLAGS = -s --template=default.html5 -H style.html

all: $(EXECUTABLE)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

fixincludes:
	for d in $(patsubst %.o,%.c,$(OBJECTS)); do \
		$(IWYU) $(IWYU_TRANSFORMED_FLAGS) 2>&1 $$d | fix_include; \
	done

blockmap.o: blockmap.c blockmap.h waddir.h errors.h sort.h wadptr.h
errors.o: errors.c errors.h
graphics.o: graphics.c graphics.h waddir.h errors.h sort.h wadptr.h
main.o: main.c blockmap.h graphics.h sidedefs.h errors.h waddir.h \
        wadmerge.h wadptr.h
sha1.o: sha1.c sha1.h
sort.o: sort.c sort.h wadptr.h
sidedefs.o: sidedefs.c sidedefs.h errors.h sort.h waddir.h wadptr.h
waddir.o: waddir.c waddir.h errors.h wadptr.h
wadmerge.o: wadmerge.c sha1.h waddir.h errors.h sort.h wadmerge.h wadptr.h

ifdef WINDRES
resource.o: resource.rc
	$(WINDRES) $< -o $@

OBJECTS += resource.o
endif

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPATH)/man1
	install wadptr $(DESTDIR)$(PREFIX)/bin/wadptr
	install wadptr.1 $(DESTDIR)$(MANPATH)/man1/wadptr.1

clean:
	$(DELETE) $(EXECUTABLE)
	$(DELETE) $(OBJECTS)

windist:
	rm -rf dist
	mkdir dist
	cp wadptr.exe dist/
	$(STRIP) dist/wadptr.exe
	pandoc $(PANDOC_FLAGS) -f gfm -o dist/NEWS.html -s NEWS.md
	pandoc $(PANDOC_FLAGS) -f gfm -o dist/COPYING.html -s COPYING.md
	pandoc $(PANDOC_FLAGS) -f man wadptr.1 -o dist/wadptr.html
	rm -f wadptr-$(VERSION).zip
	zip -X -j -r wadptr-$(VERSION).zip dist

check: $(EXECUTABLE)
	./run_tests.sh

quickcheck: $(EXECUTABLE)
	git submodule update --checkout
	$(MAKE) -C quickcheck clean
	$(MAKE) -C quickcheck wads
	./wadptr -q -c quickcheck/extract/*.wad
	$(MAKE) -C quickcheck check
	# We check the demos play back if we uncompress the WADs again.
	./wadptr -q -u quickcheck/extract/*.wad
	$(MAKE) -C quickcheck check

.PHONY: all install clean dist quickcheck check windist fixincludes
