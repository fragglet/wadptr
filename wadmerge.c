/**************************************************************************
 *                     The WADPTR Project: WADMERGE.C                     *
 *                                                                        *
 *  Compresses WAD files by merging identical lumps in a WAD file and     *
 *  sharing them between multiple wad directory entries.                  *
 *                                                                        *
 **************************************************************************/

#include "wadmerge.h"

/***************************** Prototypes *********************************/

int suggest();

/***************************** Globals ************************************/

short sameas[4000];

/* Suggest ****************************************************************/

int suggest()
{
        int count, count2, linkcnt=0;
        short links[MAXLINKS][2];     // possible links
        char *check1, *check2;
        long shrink=0;   // count on how much would be saved
        char filen[30];
        int px, py;
        int perctime=20;

        px=wherex(); py=wherey(); // for % count

               // find similar entries
        for(count=0;count<numentries;count++) // each lump in turn
        {
               for(count2=0;count2<count;count2++) // check against previous
               {
                      if((wadentry[count].length == wadentry[count2].length)
                          && wadentry[count].length!=0)
                      {                   // same length, might be same lump
                          links[linkcnt][0]=count;
                          links[linkcnt][1]=count2;
                          ++linkcnt;
                          if(linkcnt>MAXLINKS)
                            errorexit("suggest: Upper limit of possible links hit!\n");
                      }
               }
        }

        // now check 'em out + see if they really are the same
        memset(sameas,-1,2*4000);

        for(count=0;count<linkcnt;count++)
        {
               if(sameas[links[count][0]]!=-1) continue; //already done that
                                                          // one
               check1=cachelump(links[count][0]); // cache both lumps
               check2=cachelump(links[count][1]);

                              // compare them
               if(!memcmp(check1,check2,wadentry[links[count][0]].length))
               {                 // they are the same !
                       sameas[links[count][0]]=links[count][1];
               }

               free(check1); // free back both lumps
               free(check2);

               perctime--;    // % count
               if(!perctime)
               {
                     gotoxy(px,py); printf("%i%%",(50*count)/linkcnt);
                     fflush(stdout);
                     perctime=50;
               }
        }
        gotoxy(px,py);
}

/* Rebuild the WAD, making it smaller in the process ***********************/

void rebuild(char *newname)
{
       int num_suggest=0, count, count2;
       char a;
       char tempstr[5];
       char *tempchar;
       FILE *newwad;
       long along=0 ,filepos;
       int nextperc=50, px, py;

            // first run suggest mode to find how to make it smaller
       suggest();

       newwad=fopen(newname,"wb+");   // open the new wad

       if(!newwad) errorexit("rebuild: Couldn't open to %s\n",newname);

       fwrite("IWAD",1,4,newwad);   // temp header
       fwrite(&along,4,1,newwad);
       fwrite(&along,4,1,newwad);

       px=wherex(); py=wherey(); // for % count

       for(count=0;count<numentries;count++)  // add each entry in turn
       {

               // first check if it's a compressed(linked)
               if(sameas[count]!=-1)
               {
                      wadentry[count].offset=wadentry[sameas[count]].offset;
                      goto next;
               }
               fgetpos(newwad, &filepos); // find where this lump will be
               tempchar=cachelump(count); // cache the lump
                               //write to new file
               fwrite(tempchar,1,wadentry[count].length,newwad);
               free(tempchar); // free lump back again
               wadentry[count].offset=filepos; // update the wad directory
                                               // with new lump location
               next:
               nextperc--;       // % count
               if(!nextperc)
               {
                      gotoxy(px,py);
                      printf("%i%%",50+(count*50)/numentries); fflush(stdout);
                      nextperc=50;
               }
       }
       fgetpos(newwad,&diroffset);  // write the wad directory
       fwrite(wadentry,sizeof(entry_t),numentries,newwad);

       rewind(newwad); // now write the real header
       strcpy(tempstr,"PWAD");
       if(wad==IWAD) strcpy(tempstr,"IWAD");
       fwrite(tempstr,1,4,newwad);
       fwrite(&numentries,1,4,newwad);
       fwrite(&diroffset,1,4,newwad);

       fclose(newwad);     // close the new wad

       gotoxy(px,py); printf("         "); fflush(stdout); // remove % count
       gotoxy(px,py); 
}


