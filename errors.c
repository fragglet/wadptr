/**************************************************************************
 *                          The WADPTR project                            *
 *                                                                        *
 * Error handling routines:                                               *
 *                                                                        *
 **************************************************************************/

#include "errors.h"

/* Display an error (Last remnant of the DMWAD heritage) ******************/

void errorexit(char *s, ...)
{

        va_list args;
        va_start(args, s);

        sound( 640);                    // thanks to the deu authors!
        delay( 100);
        nosound();

        vfprintf(stderr, s, args);

        exit(0xff);
}

/* Signal handling stuff **************************************************/

void sig_func(int signalnum)
{
        printf("\n\n");
        switch(signalnum)
        {
                default:        errorexit("Bizarre signal error??!!\n");
                case SIGINT:    errorexit("User Interrupted\n");
                case SIGNOFP:   errorexit("Error:no FPU\n");
                case SIGSEGV:   errorexit("Segment violation error(memory fault)\n");
        }
}
