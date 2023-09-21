# **********************************************************************
# *                        The WADPTR Project                          *
# *                                                                    *
# *                     MAKEFILE for the project                       *
# *                                                                    *
# **********************************************************************

# install prefix:
PREFIX          = /usr/local

# Objects for Unix / DOS
OBJECTSO	= main.o waddir.o errors.o wadmerge.o lumps.o

# default
OBJECTS		= $(OBJECTSO)


DOSFLAGS	= CC=gcc EXECUTABLE=wadptr.exe DELETE=del CFLAGS=-O3

LINUXFLAGS      = CC=gcc EXECUTABLE=wadptr DELETE=rm CFLAGS='-O3 -DANSILIBS -DNORMALUNIX'


SUNFLAGS	= CC=cc EXECUTABLE=wadptr DELETE=rm CFLAGS='-xO4 -DANSILIBS -DNORMALUNIX'

HPFLAGS		= CC=cc EXECUTABLE=wadptr DELETE=rm CFLAGS='+O3 -Ae -DANSILIBS -DNORMALUNIX'



all : $(EXECUTABLE)

dos:
	make $(DOSFLAGS)

dos_clean:
	make $(DOSFLAGS) clean

sun:
	make $(SUNFLAGS)

sun_clean:
	make $(SUNFLAGS) clean

hp:
	make $(HPFLAGS)

hp_clean:
	make $(HPFLAGS) clean

linux:
	make $(LINUXFLAGS)

linux_clean:
	make $(LINUXFLAGS) clean


$(EXECUTABLE) : $(OBJECTS)
	$(PURIFY) $(CC) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)

######### C source files ##########

main.o : main.c wadptr.h errors.h waddir.h wadmerge.h lumps.h
	$(CC) $(CFLAGS) -c main.c

waddir.o : waddir.c waddir.h errors.h
	$(CC) $(CFLAGS) -c waddir.c

errors.o : errors.c errors.h
	$(CC) $(CFLAGS) -c errors.c

lumps.o : lumps.c lumps.h waddir.h errors.h
	$(CC) $(CFLAGS) -c lumps.c

wadmerge.o : wadmerge.c wadmerge.h waddir.h errors.h
	$(CC) $(CFLAGS) -c wadmerge.c


########## Functions ############

install :
	install -D wadptr $(DESTDIR)$(PREFIX)/bin/wadptr
	install -D wadptr.txt $(DESTDIR)$(PREFIX)/share/doc/wadptr/wadptr.txt

clean : 
	-$(DELETE) $(EXECUTABLE)
	-$(DELETE) *.o

