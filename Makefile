# **********************************************************************
# *                        The WADPTR Project                          *
# *                                                                    *
# *                     MAKEFILE for the project                       *
# *                                                                    *
# **********************************************************************

all : wadptr.exe

wadptr.exe : main.o waddir.o errors.o wadmerge.o lumps.o
	gcc main.o waddir.o errors.o wadmerge.o lumps.o -o wadptr.exe

######### C source files ##########

main.o : main.c wadptr.h errors.h waddir.h wadmerge.h lumps.h
	gcc -c main.c

waddir.o : waddir.c waddir.h errors.h
	gcc -c waddir.c

errors.o : errors.c errors.h
	gcc -c errors.c

lumps.o : lumps.c lumps.h waddir.h errors.h
	gcc -c lumps.c

wadmerge.o : wadmerge.c wadmerge.h waddir.h errors.h
	gcc -c wadmerge.c

########## Functions ############

clean : 
	del wadptr.exe 
	del *.o
