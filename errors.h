/**************************************************************************
 *                                ERRORS.H                                *
 *                         Error-message routines                         *
 *                                                                        *
 **************************************************************************/

#ifndef __ERRORS_H_INCLUDED__
#define __ERRORS_H_INCLUDED__

/****************************** Includes **********************************/

#include <stdio.h>
#include <stdarg.h>
#ifdef ANSILIBS
#define SIGNOFP	SIGFPE
#else
#include <pc.h>
#endif
#include <signal.h>

/***************************** Prototypes *********************************/

void errorexit(char *s, ...);
void sig_func(int signalnum);

#endif
