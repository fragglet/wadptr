/**************************************************************************
 *                          The WADPTR project                            *
 *                                                                        *
 * WAD loading and reading routines: by me, me, me!                       *
 *                                                                        *
 **************************************************************************/

/************************ INCLUDES ****************************************/

#include "waddir.h"

enum { false, true };

/************************ Globals ******************************************/

FILE *wadfp;
long numentries, diroffset;
entry_t wadentry[MAXENTRIES];
wadtype wad;

/* Read the WAD ************************************************************/

int readwad()
{
        char a[5];
        memset(a,0,5);

        rewind(wadfp);     // read wad header
        fread(a,4,1,wadfp);   // iwad/pwad
        fread(&numentries,4,1,wadfp);
        fread(&diroffset,4,1,wadfp);
        wad=NONWAD;                 // default
        if(!strcmp(a,"PWAD")) wad=PWAD;
        if(!strcmp(a,"IWAD")) wad=IWAD;
        if(wad==NONWAD)
        {
               // errorexit("readwad: File not IWAD or PWAD!\n");
               printf("File not IWAD or PWAD!\n");
               return true;
        }

        fsetpos(wadfp,&diroffset);

        if(numentries>MAXENTRIES)
        {
      // errorexit("readwad: Cannot handle > %i entry wads!\n",MAXENTRIES);
              printf("Cannot handle > %i entry wads!\n", MAXENTRIES);
              return true;
        }
                                    // read the wad entries
        fread(wadentry,sizeof(entry_t),numentries,wadfp);

        return false;
}

/* Write the WAD header and directory *************************************/

void writewad()
{
        char a[5];
        rewind(wadfp);

        strcpy(a,"SFSF"); // :)
        if(wad==IWAD) strcpy(a,"IWAD");
        if(wad==PWAD) strcpy(a,"PWAD");
        fwrite(a,1,4,wadfp);
        fwrite(&numentries,1,4,wadfp);
        fwrite(&diroffset,1,4,wadfp);

        fsetpos(wadfp,&diroffset);
        fwrite(wadentry,sizeof(entry_t),numentries,wadfp);
}


/* Takes a string8 in an entry type and returns a valid string *************/

char *convert_string8(entry_t entry)
{
        static char temp[100][10];
        static int tempnum=1;

        tempnum++;
        if(tempnum==100) tempnum=1;

        memset(temp[tempnum],0,9);
        memcpy(temp[tempnum],entry.name,8);

        return temp[tempnum];
}

/* Finds if an entry exists ************************************************/

int entry_exist(char *entrytofind)
{
        int count;
        for(count=0;count<numentries;count++)
        {
               if(!strncmp(wadentry[count].name,entrytofind,8 ))
                        return count;
        }
        return -1;
}


/* Finds an entry and returns information about it *************************/

entry_t findinfo(char *entrytofind)
{
        int count;
        static entry_t entry;
        char buffer[10];

        for(count=0;count<numentries;count++)
        {
               if(!strncmp(wadentry[count].name,entrytofind,8 ))
                        return wadentry[count];
        }
        memset(&entry,0,sizeof(entry_t));
        return entry;
}

/* Adds an entry to the WAD ************************************************/

void addentry(entry_t entry)
{
        char buffer[10];
        long temp;

        strcpy(buffer,convert_string8(entry)); // copying to temp does have a
        if(entry_exist(buffer)!=-1)                // point, incidentally.
                printf("\tWarning! Resource %s already exists!\n",
                                               convert_string8(entry));
        memcpy(&wadentry[numentries],&entry,sizeof(entry_t));
        numentries++;
        writewad();
}

/* Load a lump to memory **************************************************/

void *cachelump(int entrynum)
{
       char *working;

       working = malloc(wadentry[entrynum].length);
       if(!working) errorexit("cachelump: Couldn't malloc %i bytes\n",
                                 wadentry[entrynum].length);

       fsetpos(wadfp, &wadentry[entrynum].offset);
       fread(working,wadentry[entrynum].length,1,wadfp);

       return working;
}

/* Copy a WAD ( make a backup ) *******************************************/

void copywad(char *newfile)
{
         FILE *newwad;
         char a;

         newwad=fopen(newfile,"wb");
         if(!newwad) errorexit("copywad: Couldn't copy a wad (filename:%s)\n",newfile);

         rewind(wadfp);   // I just love this function
         while(!feof(wadfp))
         {
               fread(&a,1,1,wadfp);
               fwrite(&a,1,1,newwad);
         }
         fclose(newwad);
}

/* Various WAD-related is??? functions ************************************/

int islevel(int entry)
{
        if(entry >= numentries) return false;

                // 9/9/99: generalised support: if the next entry is a
                // things resource then its a level
        return !strncmp(wadentry[entry+1].name, "THINGS", 8);
}

int islevelentry(char *s)
{
      if(!strcmp(s,"LINEDEFS")) return true;
      if(!strcmp(s,"SIDEDEFS")) return true;
      if(!strcmp(s,"SECTORS"))  return true;
      if(!strcmp(s,"VERTEXES")) return true;
      if(!strcmp(s,"REJECT"))   return true;
      if(!strcmp(s,"BLOCKMAP")) return true;
      if(!strcmp(s,"NODES"))    return true;
      if(!strcmp(s,"THINGS"))   return true;
      if(!strcmp(s,"SEGS"))     return true;
      if(!strcmp(s,"SSECTORS")) return true;

      if(!strcmp(s,"BEHAVIOR")) return true; // hexen "behavior" lump
      return false;
}

/* Find the total size of a level ******************************************/

int findlevelsize(char *s)
{
         int entrynum, count, sizecount=0;

         entrynum=entry_exist(s);

         for(count=entrynum+1;count<entrynum+11;count++)
                sizecount+=wadentry[count].length;

         return sizecount;
}


