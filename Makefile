# **********************************************************************
# *                        The WADPTR Project                          *
# *                                                                    *
# *                     MAKEFILE for the project                       *
# *                                                                    *
# **********************************************************************

# for PC:
#CC		= gcc
#EXECUTABLE	= wadptr.exe
#DELETE		= del
#CFLAGS		= -O3

# for Unix (Solaris)
CC		= cc
EXECUTABLE	= wadptr
DELETE		= rm
CFLAGS		= -xO4 -DANSILIBS -DNORMALUNIX

# for RISC OS
#CC		= gcc
#EXECUTABLE	= wadptr
#DELETE		= wipe
#WIMPLIBPATH	= scsi::5.$.vice1_1.src.arch.riscos.wimplib
#CFLAGS		= -O3 -DANSILIBS -DSCL_VA_HACK -I$(WIMPLIBPATH)
#LDFLAGS		= -mstubs -l$(WIMPLIBPATH).o.libwimp



# Objects for Unix / DOS
OBJECTS	= main.o waddir.o errors.o wadmerge.o lumps.o


# Objects for RISC OS
#OBJECTS		= o.main o.waddir o.errors o.wadmerge o.lumps



all : $(EXECUTABLE)


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


# RISC OS filenames
o.main:	c.main h.wadptr h.errors h.waddir h.wadmerge h.lumps
	$(CC) $(CFLAGS) -c c.main

o.waddir: c.waddir h.waddir h.errors
	$(CC) $(CFLAGS) -c c.waddir

o.errors: c.errors h.errors
	$(CC) $(CFLAGS) -c c.errors

o.lumps: c.lumps h.lumps h.waddir h.errors
	$(CC) $(CFLAGS) -c c.lumps

o.wadmerge: c.wadmerge h.wadmerge h.waddir h.errors
	$(CC) $(CFLAGS) -c c.wadmerge


########## Functions ############

clean : 
	-$(DELETE) $(EXECUTABLE)
	-$(DELETE) *.o
