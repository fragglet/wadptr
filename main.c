/**************************************************************************
 *                          The WADPTR project                            *
 *                                                                        *
 * Compresses Doom WAD files through several methods:                     *
 *                                                                        *
 * - Merges identical lumps (see wadmerge.c)                              *
 * - 'Squashes' graphics (see lumps.c)                                    *
 * - Packs the sidedefs in levels (see lumps.c)                           *
 *                                                                        *
 **************************************************************************/

/******************************* INCLUDES **********************************/

#include "wadptr.h"

/******************************* GLOBALS ***********************************/

int g_argc;                          // global cmd-line list
char **g_argv;
char filespec[50]="";                // file spec on command line eg. *.wad
char wadname[50]="";                 // WAD file name
int action;                          // list, compress, uncompress
int allowpack=1;                     // level packing on
int allowsquash=1;                   // picture squashing on
int allowmerge=1;                    // lump merging on

/* Main ********************************************************************/

int main(int argc, char *argv[])
{
        int action=0;

        g_argc=argc;                            // Set global cmd-line list
        g_argv=argv;

        printf(
          "\n"                             // Display startup message
          "WADPTR - WAD Compressor  Version " VERSION "\n"
          "Copyright (c)1997,1998 Simon Howard.\n"
        );

                // set error signals
        signal(SIGINT,sig_func);
        signal(SIGSEGV,sig_func);
        signal(SIGNOFP,sig_func);

        parsecmdline();         // Check cmd-lines

        eachwad(filespec);     // do each wad
}

/****************** Command line handling, open wad etc. *******************/

/* Parse cmd-line options **************************************************/

int parsecmdline()
{
        int count;

        action=HELP;         // default to help if not told what to do

        for(count=1; count<g_argc; count++)
        {
                if((!strcmp(g_argv[count],"-help"))
                  |(!strcmp(g_argv[count],"-h")) )
                        action=HELP;

                if((!strcmp(g_argv[count],"-list"))
                  |(!strcmp(g_argv[count],"-l")) )
                        action=LIST;

                if((!strcmp(g_argv[count],"-compress"))
                  |(!strcmp(g_argv[count],"-c")) )
                        action=COMPRESS;

                if((!strcmp(g_argv[count],"-uncompress"))
                 | (!strcmp(g_argv[count],"-u")) )
                        action=UNCOMPRESS;

                           // specific disabling
                if(!strcmp(g_argv[count],"-nomerge"))
                        allowmerge=0;
                if(!strcmp(g_argv[count],"-nosquash"))
                        allowsquash=0;
                if(!strcmp(g_argv[count],"-nopack"))
                        allowpack=0;

                if(g_argv[count][0]!='-')
                        if(!strcmp(filespec,""))
                                strcpy(filespec,g_argv[count]);
        }

        if(!strcmp(filespec,"")) // no wad file given
        {
               if(action==HELP)
               {
                     doaction();
                     exit(0);
               }
               else
                    errorexit("No input WAD file specified.\n");
        }

        if(action==UNCOMPRESS && allowmerge==0)
              errorexit(
                 "Sorry, uncompressing will undo any lump merging on WADs.\n"
                 "The -nomerge command is not available with the "
                 "-u(uncompress) option.\n");
}

/* Do each wad specified by the wildcard in turn **************************/

void eachwad(char *filespec)
{
        DIR *directory;          // the directory
        struct dirent *direntry; // directory entry
        char *dirname, *filename;   // directory/file names
        int wadcount=0;

        dirname=filespec;     // get the directory name from the filespec

        filename=find_filename(dirname);  // find the filename

        if(dirname==filename) dirname=".";   // use the current directory
                                             // if none specified
        directory=opendir(dirname);          // open the directory
        while(1)                             // go through entries
        {
             direntry=readdir(directory); // read the next entry
             if(!direntry) break;         // no more entries
             if(filecmp(direntry->d_name,filename))
             {                 // see if it conforms to the wildcard
                  // build the wad name from dirname and filename
                  sprintf(wadname,"%s\\%s",dirname,direntry->d_name);
                  if(!strcmp(dirname,"."))   // don't bother with dirname "."
                       strcpy(wadname,direntry->d_name);
                  if(!(openwad(wadname)))
                  {      // no problem with wad.. do whatever
                      doaction();   // do whatever(compress, uncompress etc)
                  }
                  wadcount++;   // another one done..
                  fclose(wadfp);                        // close the wad
             }
        }
        closedir(directory);

        if(!wadcount)
                errorexit("\neachwad: No files found!\n");
}


/* Open the original WAD ***************************************************/

int openwad(char *filename)
{
        char tempstr[50];
        int a;

        if(!action) action=LIST; // no action but i've got a wad..
                                 // whats in it? default to list
                        // open the wad
        wadfp=fopen(filename,"rb+");
        if(!wadfp)       // can't
        {
              printf("%s does not exist\n",wadname);
              return 1;
        }

        printf("\nSearching WAD: %s\n",filename);
        a=readwad();       // read the directory
        if(a) return 1;    // problem with wad

        printf("\n");   // leave a space if wads ok :)
        return 0;
}

/* Does an action based on command line ************************************/

void doaction()
{
        switch(action)
        {
                case HELP:      help();
                                break;

                case LIST:      list_entries();
                                break;

                case UNCOMPRESS:uncompress();
                                break;

                case COMPRESS:  compress();
                                break;
        }
}

/************************ Command line functions ***************************/

/* Display help ************************************************************/

void help()
{
        printf(
          "\n"
          "Usage:  WADPTR inputwad [outputwad] options\n"
          "\n"
          " -c        :   Compress WAD\n"
          " -u        :   Uncompress WAD\n"
          " -l        :   List WAD\n"
          " -h        :   Help\n"
          "\n"
          " -nomerge  :   Disable lump merging\n"
          " -nosquash :   Disable graphic squashing\n"
          " -nopack   :   Disable sidedef packing\n "
        );
}

/* Compress a WAD **********************************************************/

void compress()
{
         int count, findshrink;
         long wadsize;         // wad size(to find % smaller)
         FILE *fstream;
         int written=0; // if 0:write 1:been written 2:write silently
         char *temp, resname[10], a[50];

         if(wad==IWAD)
               if(!iwad_warning()) return;

         wadsize=diroffset+(sizeof(entry_t)*numentries); // find wad size

         fstream=fopen("~wptmp.wad","wb+");
         if(!fstream) errorexit("compress: Couldn't write a temporary file\n");

         fwrite(a,12,1,fstream);    // temp header.

         for(count=0;count<numentries;count++)  // add each wad entry in turn
         {
                strcpy(resname,convert_string8(wadentry[count])); // find
                                                           // resource name
                written=0;      // reset written
                if(!islevelentry(resname))   // hide individual level entries
                {
                     printf("Adding: %s       ",resname);
                     fflush (stdout);
                }
                else
                     written=2; // silently write entry: level entry

                if(allowpack) // sidedef packing disabling
                {
                  if(islevel(count))        // level name
                  {
                        printf("\tPacking "); fflush(stdout);
                        findshrink=findlevelsize(resname);

                        p_pack(resname);        // pack the level

                        findshrink=findperc(findshrink,    // % shrunk
                                  findlevelsize(resname));
                        printf("(%i%%), done.\n", findshrink);

                        written=2; // silently write this lump (if any)
                  }
                  if(!strcmp(resname, "SIDEDEFS"))
                  {
                        // write the pre-packed sidedef entry
                        fgetpos(fstream,&wadentry[count].offset);
                        fwrite(p_sidedefres,wadentry[count].length,1,fstream);
                        free(p_sidedefres);// sidedefs no longer needed
                        written=1; // now written
                  }
                  if(!strcmp(resname, "LINEDEFS"))
                  {
                        // write the pre-packed linedef entry
                        fgetpos(fstream,&wadentry[count].offset);
                        fwrite(p_linedefres,wadentry[count].length,1,fstream);
                        free(p_linedefres);
                        written=1; // now written
                  }
                }

                if(allowsquash)  // squash disabling
                  if(s_isgraphic(resname))          // graphic
                  {
                        printf("\tSquashing "); fflush(stdout);
                        findshrink=wadentry[count].length;

                        temp=s_squash(resname);   // get the squashed graphic
                        fgetpos(fstream,&wadentry[count].offset); //update dir
                                        // write it
                        fwrite(temp,wadentry[count].length,1,fstream);

                        free(temp); // graphic no longer needed: free it
                                // % shrink 
                        findshrink=findperc(findshrink,wadentry[count].length);
                        printf("(%i%%), done.\n", findshrink);
                        written=1; // now written
                  }

                if(written==0 | written==2) // write or silently
                {
                        if(written==0) // only if not silent
                             { printf("\tStoring "); fflush(stdout); }
                        temp=cachelump(count); // get the lump
                        fgetpos(fstream,&wadentry[count].offset); //update dir
                                // write lump
                        fwrite(temp,wadentry[count].length,1,fstream);
                        free(temp); // now free the lump
                        if(written==0)  // always 0%
                              printf("(0%%), done.\n");
                }
         }
         fgetpos(fstream,&diroffset);   // update the directory location
         fwrite(wadentry,numentries,sizeof(entry_t),fstream); //write dir
         rewind(fstream);       // back to the start to write the header

         strcpy(a,"PWAD");              // write the header
         if(wad==IWAD) strcpy(a,"IWAD");
         fwrite(a,1,4,fstream);
         fwrite(&numentries,1,4,fstream);
         fwrite(&diroffset,1,4,fstream);

         fclose(fstream);
         fclose(wadfp);

         if(allowmerge)
         {
                  wadfp=fopen("~wptmp.wad","rb+");  // reload the temp file as the wad
                  printf("\nMerging identical lumps.. "); fflush(stdout);

                  remove(wadname);      // delete the old wad

                  rebuild(wadname);// run rebuild() to remove identical lumps:
                                   // rebuild them back to the original filename
                  printf("done.\n"); // all done!
                  fclose(wadfp);
                  remove("~wptmp.wad");  // delete the temp file
         }
         else
         {
                  remove(wadname);
                  rename("~wptmp.wad",wadname);
         }

         wadfp=fopen(wadname,"rb+"); // so there is something to close

         findshrink=findperc(wadsize,diroffset+(numentries*sizeof(entry_t)));

         printf("*** %s is %i%% smaller ***\n",wadname,findshrink);
}

/* Uncompress a WAD ********************************************************/

void uncompress()
{
         char tempstr[50];
         FILE *fstream;
         char *tempres;
         char resname[10];
         int written;     // see compress()
         long fileloc;    // file location
         int count;

         if(wad==IWAD)
               if(!iwad_warning()) return;

         fstream=fopen("~wptmp.wad","wb+");  // open wad
         fwrite(tempstr,12,1,fstream);    // temp header

         for(count=0;count<numentries;count++)  // each entry
         {
                  strcpy(resname,convert_string8(wadentry[count]));

                  written=0;

                  if(islevelentry(resname))
                         written=2;   // silently write (level entry)
                  else
                  {
                         printf("Adding: %s       ",resname); fflush(stdout);
                  }

                  if(allowpack)
                  {
                    if(islevel(count))
                    {
                          printf("\tUnpacking"); fflush(stdout);
                          p_unpack(resname);     // unpack the level
                          printf(", done.\n");
                          written=2;    // silently write this lump
                    }
                    if(!strcmp(resname,"SIDEDEFS"))
                    {
                          fgetpos(fstream,&(wadentry[count].offset)); //dir
                          fwrite(p_sidedefres,wadentry[count].length,1,fstream);
                          free(p_sidedefres); // sidedefs no longer needed
                          written=1;
                    }
                    if(!strcmp(resname,"LINEDEFS"))
                    {
                          fgetpos(fstream,&(wadentry[count].offset));   // dir
                          fwrite(p_linedefres,wadentry[count].length,1,fstream);
                          free(p_linedefres); // linedefs no longer needed
                          written=1;
                    }
                  }
                  if(allowsquash)
                    if(s_isgraphic(resname))
                    {
                          printf("\tUnsquashing"); fflush(stdout);
                          tempres=s_unsquash(resname); // get the new lump
                          fgetpos(fstream,&(wadentry[count].offset)); //dir
                                              //write it
                          fwrite(tempres,wadentry[count].length,1,fstream);
                          free(tempres); // free the lump
                          printf(", done\n");
                          written=1;
                    }
                  if(written==0 | written==2) // not written or write silently
                  {
                          if(!written) // 0
                          {
                                 printf("\tStoring", resname); fflush(stdout);
                          }
                          tempres=cachelump(count);
                          fgetpos(fstream,&fileloc);
                          fwrite(tempres,wadentry[count].length,1,fstream);
                          free(tempres);
                          wadentry[count].offset=fileloc;
                          if(!written)
                                 printf(", done.\n");
                  }
         }
         fgetpos(fstream,&diroffset);   // update the directory location
         fwrite(wadentry,numentries,sizeof(entry_t),fstream); //write dir
         rewind(fstream);       // back to the start to write the header

         strcpy(tempstr,"PWAD");              // write the header
         if(wad==IWAD) strcpy(tempstr,"IWAD");
         fwrite(tempstr,1,4,fstream);
         fwrite(&numentries,1,4,fstream);
         fwrite(&diroffset,1,4,fstream);

         fclose(fstream);
         fclose(wadfp);

         remove(wadname);              // delete the old wad
         rename("~wptmp.wad",wadname); // replace the original wad with the
                                       // new one
         wadfp=fopen(wadname,"rb+");

}

/* List WAD entries ********************************************************/

void list_entries()
{
        int count, count2;
        int ypos;
        char resname[10];

        printf(
        " Number Length  Offset          Method      Name        Shared\n"
        " ------ ------  ------          ------      ----        ------\n");

        for(count=0;count<numentries;count++)
        {
               strcpy(resname,convert_string8(wadentry[count]));
               if(islevelentry(resname)) continue;
               ypos=wherey();

               // wad entry number
               printf(" %i  \t",count+1);

               // size
               if(islevel(count))
               {    // the whole level not just the id lump
                      printf("%i\t",findlevelsize(resname));
               }
               else  //not a level, doesn't matter
                      printf("%i\t",wadentry[count].length);

               // file offset
               printf("0x%08x     \t",wadentry[count].offset);

               // compression method
               if(islevel(count))
               {                  // this is a level
                     if(p_ispacked(resname))
                                 printf("Packed      "); // packed
                     else
                                 printf("Unpacked    "); // not
               }
               else
               {
                     if(s_isgraphic(resname))
                     {           // this is a graphic
                           if(s_is_squashed(resname))
                                 printf("Squashed    "); // squashed
                           else
                                 printf("Unsquashed  "); // not
                     }
                     else        // ordinary lump w/no compression
                                 printf("Stored      ");
               }

               // resource name
               printf("%s%s\t",resname, (strlen(resname)<4)?"\t":"");

               // shared resource
               if(wadentry[count].length==0)
               {
                    printf("No\n");
                    continue;
               }
               for(count2=0;count2<count;count2++)
               {                                  // same offset + size
                    if((wadentry[count2].offset==wadentry[count].offset)
                     &&(wadentry[count2].length==wadentry[count].length))
                    {
                           printf("%s\n", convert_string8(wadentry[count2]));
                           break;
                    }
               }
               if(count2==count) // no identical lumps if it
                    printf("No\n"); // reached the last one
        }
}


/*********************** Wildcard Functions *******************************/

/* Break a filename into filename and extension ***************************/

char *find_filename(char *s)
{
        char *tempstr, *backstr;

        backstr=s;

        while(1)
        {
                tempstr=strchr(backstr,'\\');
                if(!tempstr)    // no more slashes
                {
                        tempstr=strchr(backstr,'/');
                        if(!tempstr)
                        {
                                *(backstr-1)=0;
                                return backstr;
                        }
                }
                backstr=tempstr+1;
        }
}

/* Compare two filenames ***************************************************/

int filecmp(char *filename, char *templaten)
{
        char filename1[50], template1[50];      // filename
        char *filename2 ,   *template2;      // extension
        int count;

        strcpy(filename1,filename);
        strcpy(template1,templaten);

        filename2=strchr(filename1,'.');
        if(!filename2) filename2=""; // no extension
        else
        {                       // extension
                *filename2=0; // end of main filename
                filename2++;  // set to start of extension
        }

        template2=strchr(template1,'.');
        if(!template2) template2="";
        else
        {
                *template2=0;
                template2++;
        }

        for(count=0;count<8;count++)    // compare the filenames
        {
                if(filename1[count]=='\0'
                 && template1[count]!='\0') return 0;
                if(template1[count]=='?') continue;
                if(template1[count]=='*') break;
                if(template1[count]!=filename1[count]) return 0;
                if(template1[count]=='\0') break; // end of string
        }

        for(count=0;count<3;count++)    // compare the extensions
        {
                if(filename2[count]=='\0'
                 && template2[count]!='\0') return 0;
                if(template2[count]=='?') continue;
                if(template2[count]=='*') break;
                if(template2[count]!=filename2[count]) return 0;
                if(template2[count]=='\0') break; // end of string
        }

        return 1;
}

/* Removes command line globbing ******************************************/

void *__crt0_glob_function()
{
        return 0;
}


/************************** Misc. functions ********************************/

/* Find how much smaller something is: returns a percentage ****************/

int findperc(int before, int after)
{
         int perc;

         perc=(after*100)/before;

         perc=100-perc;

         return perc;
}

/* Warning not to change IWAD **********************************************/

int iwad_warning()
{
         char tempchar;
         printf("Are you sure you want to change the main IWAD?");
         fflush(stdout);
         while(1)
         {
               tempchar=getch();
               if(tempchar=='Y' | tempchar=='y')
               {
                     printf("\n");
                     return 1;
               }
               if(tempchar=='N' | tempchar=='n')
               {
                     printf("\n");
                     return 0;
               }
         }
}

