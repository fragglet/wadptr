/**************************************************************************
 *                  The WADPTR project : LUMPS.C                          *
 *                                                                        *
 *            Functions for compressing individual lumps                  *
 *                                                                        *
 * P_* : Sidedef packing extension routines. Combines sidedefs which are  *
 *       identical in a level, and shares them between multiple linedefs  *
 *                                                                        *
 * S_* : Graphic squashing routines. Combines identical columns in        *
 *       graphic lumps to make them smaller                               *
 *                                                                        *
 **************************************************************************/

/******************************* INCLUDES **********************************/

#include "lumps.h"

enum { false, true };

/****************************** PROTOTYPES ********************************/

void p_findinfo();
void p_dopack();
void p_update();
void p_buildlinedefs(linedef_t *linedefs);
void p_rebuild();

int s_findgraf(char *s);
int s_find_colsize(unsigned char *col1);
int s_comp_columns(unsigned char *col1, unsigned char *col2);

/******************************* GLOBALS **********************************/

int p_levelnum;   // entry number (not 1 as in map01, 1 as in entry 1 in wad)
int p_sidedefnum; // sidedef wad entry number
int p_linedefnum; // linedef wad entry number
int p_num_linedefs=0, p_num_sidedefs=0; // number of sd/lds do not get confused
char p_working[50]; // the name of the level resource eg. "MAP01"

sidedef_t *p_newsidedef;   // the new sidedefs
linedef_t *p_newlinedef;   // the new linedefs
int *p_movedto;          // keep track of where the sidedefs are now
int p_newnum=0;               // the new number of sidedefs

char *p_linedefres=0;         // the new linedef resource
char *p_sidedefres=0;         // the new sidedef resource

//    Graphic squashing globals
int s_equalcolumn[400];  // 1 for each column: another column which is
                         // identical or -1 if there isn't one
short s_height, s_width, s_loffset, s_toffset;  // picture width, height etc.
long *s_columns;         // the location of each column in the lump
long s_colsize[400];     // the length(in bytes) of each column


/* Pack a level ***********************************************************/

// Call p_pack() with the level name eg. p_pack("MAP01");. p_pack will then
// pack that level. The new sidedef and linedef lumps are pointed to by
// p_sidedefres and p_linedefres. These must be free()d by other functions
// when they are no longer needed, as p_pack does not do this.

void p_pack(char *levelname)
{
         sidedef_t *sidedefs;
         linedef_t *linedefs;

         strcpy(p_working,levelname);

         p_findinfo();

         p_rebuild();                 // remove unused sidedefs

         sidedefs=p_newsidedef;
         linedefs=p_newlinedef;

         p_findinfo();                // find what's needed

         p_dopack(sidedefs);          // pack the sidedefs in memory

         p_buildlinedefs(linedefs);   // update linedefs + save to *??res

         // saving of the wad directory is left to external sources

         p_linedefres=(char *)p_newlinedef;   // point p_linedefres and p_sidedefres at
         p_sidedefres=(char *)p_newsidedef; // the new lumps

         wadentry[p_sidedefnum].length=p_newnum*sizeof(sidedef_t);

}

/* Unpack a level *********************************************************/

// Same thing, in reverse. Saves the new sidedef and linedef lumps to
// p_sidedefres and p_linedefres.

void p_unpack(char *resname)
{
         strcpy(p_working,resname);

         p_findinfo();
         p_rebuild();

         p_linedefres=(char *)p_newlinedef;
         p_sidedefres=(char *)p_newsidedef;
}

/* Find if a level is packed **********************************************/

int p_ispacked(char *s)
{
         linedef_t *linedefs;
         int count, count2;

         strcpy(p_working,s);

         p_findinfo();

         linedefs = cachelump(p_linedefnum);

            // alloc p_movedto. 10 extra sidedefs for safety
         p_movedto = malloc(sizeof(int) * (p_num_sidedefs+10));

         if(!p_movedto) errorexit("p_ispacked: couldn't malloc p_movedto\n");

         // uses p_movedto to find if
         // same sidedef has already been used
         // on an earlier linedef checked

         for(count=0;count<p_num_sidedefs;count++) // reset p_movedto to 0
                 p_movedto[count]=0;

         for(count=0;count<p_num_linedefs;count++)   // now check
         {
                 if(linedefs[count].sidedef1 !=-1) // sidedef on that linedef
                 {                                 // side
                        if(p_movedto[linedefs[count].sidedef1]) // already used
                        {                            // on a previous linedef
                                free(linedefs);
                                free(p_movedto);
                                return true; // must be packed
                        }
                        else        // mark it as used for later reference
                                p_movedto[linedefs[count].sidedef1]=1;
                 }
                 if(linedefs[count].sidedef2 !=-1) // now the other side
                 {
                        if(p_movedto[linedefs[count].sidedef2])
                        {
                                free(linedefs);
                                free(p_movedto);
                                return true;
                        }
                        else
                                p_movedto[linedefs[count].sidedef2]=1;
                 }
         }
         free(linedefs);
         free(p_movedto);
         return false; // cant be packed: none of the sidedefs are shared
}

/* Find neccesary stuff before processing *********************************/

void p_findinfo()
{
         int count, n;

         // first find the level entry
         for(count=0;count<numentries;count++)
         {
                  if(!strncmp(wadentry[count].name,p_working,8))
                  {                   // matches the name given
                          p_levelnum=count;
                          break;
                  }
         }
         if(count==numentries)
          errorexit("p_findinfo: Couldn't find level: %s\n",p_working);


         n=0; // bit of a hack

         // now find the sidedefs
         for(count=p_levelnum+1;count<numentries;count++)
         {
                  if(!islevelentry(convert_string8(wadentry[count])))
                       errorexit("p_findinfo: Can't find sidedef/linedef entries!\n");

                  if(!strncmp(wadentry[count].name,"SIDEDEFS",8))
                  {
                           n++;
                           p_sidedefnum=count;
                  }
                  if(!strncmp(wadentry[count].name,"LINEDEFS",8))
                  {
                           n++;
                           p_linedefnum=count;
                  }
                  if(n==2) break;   // found both :)
         }
         // find number of linedefs and sidedefs for later..
         p_num_linedefs = wadentry[p_linedefnum].length/sizeof(linedef_t);
         p_num_sidedefs = wadentry[p_sidedefnum].length/sizeof(sidedef_t);
}


/* Actually pack the sidedefs *******************************************/

void p_dopack(sidedef_t *sidedefs)
{
         int count, count2;
         sidedef_t *newsidedef;

         p_newsidedef = malloc(wadentry[p_sidedefnum].length * 10);
         if(!p_newsidedef)
         {
            errorexit("p_dopack: could not alloc memory for new sidedefs\n");
         }

                // allocate p_movedto
         p_movedto = malloc(sizeof(int) * (p_num_sidedefs+10));

         p_newnum=0;
         for(count=0;count<p_num_sidedefs;count++) // each sidedef in turn
         {
                if((count % 100) == 0)
                {
                        // time for a percent-done update
                        int x, y;
                        x = wherex(); y = wherey();
                        printf("%%%i  ", 
                                (count*100) / p_num_sidedefs);
                        fflush(stdout);
                        gotoxy(x, y);
                }
                for(count2=0;count2<p_newnum;count2++) // check previous
                {
                        if(!memcmp(&p_newsidedef[count2],
                              &sidedefs[count], sizeof(sidedef_t)))
                        {      // they are identical: this one can be removed
                              p_movedto[count]=count2;
                              goto next; // i hate gotos
                        }
                }
                // a sidedef like this does not yet exist: add one
                memcpy(&p_newsidedef[p_newnum],&sidedefs[count],sizeof(sidedef_t));
                p_movedto[count]=p_newnum;
                p_newnum++;
                next:
         }
         // all done!
         free(sidedefs);   // fly free, little sidedefs!!
}

/* Update the linedefs and save sidedefs *********************************/

void p_buildlinedefs(linedef_t *linedefs)
{
       int count;

       // update the linedefs with where the sidedefs have been moved to,
       // using p_movedto[] to find where they now are..

       for(count=0;count<p_num_linedefs;count++)
       {
               if(linedefs[count].sidedef1!=-1) // this side has a sidedef
                 linedefs[count].sidedef1=p_movedto[linedefs[count].sidedef1];
               if(linedefs[count].sidedef2!=-1) // check the other side
                 linedefs[count].sidedef2=p_movedto[linedefs[count].sidedef2];
       }

       p_newlinedef=linedefs;

       // free p_movedto now.
       free(p_movedto);
}

/* Rebuild the sidedefs ***************************************************/

void p_rebuild()
{
         sidedef_t *sidedefs;
         linedef_t *linedefs;
         int count;

         sidedefs=cachelump(p_sidedefnum);
         linedefs=cachelump(p_linedefnum);

         p_newsidedef=malloc(wadentry[p_sidedefnum].length * 10);

         if(!p_newsidedef)
         errorexit("p_rebuild: could not alloc memory for new sidedefs\n");

         p_newnum=0;

         for(count=0;count<p_num_linedefs;count++)
         {
                if(linedefs[count].sidedef1!=-1) // sidedef on that side
                {
                       memcpy(&(p_newsidedef[p_newnum]),
                          &(sidedefs[linedefs[count].sidedef1]),
                                  sizeof(sidedef_t));
                       linedefs[count].sidedef1=p_newnum;
                       p_newnum++;
                }
                if(linedefs[count].sidedef2!=-1) // sidedef on that side
                {
                       memcpy(&(p_newsidedef[p_newnum]),
                          &(sidedefs[linedefs[count].sidedef2]),
                                  sizeof(sidedef_t));
                       linedefs[count].sidedef2=p_newnum;
                       p_newnum++;
                }
         }
         // update the wad directory
         wadentry[p_sidedefnum].length=p_newnum*sizeof(sidedef_t);

         free(sidedefs);   // no longer need the old sidedefs
         p_newlinedef=linedefs;  // still need the old linedefs:
                                         // they have been updated
}

/* Graphic squashing routines *********************************************/

/* Squash a graphic lump **************************************************/

// Squashes a graphic. Call with the lump name - eg. s_squash("TITLEPIC");
// returns a pointer to the new(compressed) lump. This must be free()d when
// it is no longer needed, as s_squash() does not do this itself.

char *s_squash(char *s)
{
       unsigned char *working , *newres;
       int entrynum, count, count2, n, n2;
       int in_post;
       long *newptr, lastpt;

       if(!s_isgraphic(s)) return;
       entrynum=entry_exist(s);
       working=cachelump(entrynum);
       if((long)working==-1) errorexit("squash: Couldn't find %s\n",s);

       if(!s_findgraf(working)) return working;    // find posts to be killed
                                           // if none, return original lump
       newres=malloc(100000);  // alloc memory for the new pic resource

       *(short *)(newres)=s_width;   // find various info: size,offset etc.
       *(short *)(newres+2)=s_height;
       *(short *)(newres+4)=s_loffset;
       *(short *)(newres+6)=s_toffset;

       newptr=(long *)(newres+8); // the new column pointers for the new lump

       lastpt=8+(s_width*4);     // last point in the lump

       for(count=0;count<s_width;count++) // go through each column in turn
       {
               if(s_equalcolumn[count]==-1) // add a new column
               {
                       newptr[count]=lastpt; // point this column to lastpt
                       memcpy(newres+lastpt,working+s_columns[count],
                                   s_colsize[count]); // add the new column
                       lastpt+=s_colsize[count]; // update lastpt
               }
               else
               {   // identical column already in: use that one
                       newptr[count]=newptr[s_equalcolumn[count]];
               }
       }

       if(lastpt>wadentry[entrynum].length )    // use the smallest
       {
             free(newres); // the new resource was bigger than the old one!
             return working; // use the old one
       }
       else
       {               // new one was smaller: use it
             wadentry[entrynum].length=lastpt; // update the lump size
             free(working);  // free back the original lump
             return newres; // use the new one
       }
}

/* Unsquash a picture ******************************************************/

// Exactly the same format as s_squash(). See there for more details.

char *s_unsquash(char *s)
{
       unsigned char *working , *newres;
       int entrynum, count, count2, n, n2;
       int in_post;
       long *newptr, lastpt;

       if(!s_isgraphic(s)) return;
       entrynum=entry_exist(s);     // cache the lump
       working=cachelump(entrynum);
       if((long)working==-1) errorexit("unsquash: Couldn't find %s\n",s);

       s_width=*(short *)(working);       // find various info
       s_height=*(short *)(working+2);
       s_loffset=*(short *)(working+4);
       s_toffset=*(short *)(working+6);
       s_columns=(long *)(working+8);

                                          // find column lengths
       for(count=0;count<s_width;count++)
              s_colsize[count]=s_find_colsize(working+s_columns[count]);

       newres=malloc(100000);  // alloc memory for the new pic resource

       *(short *)(newres)=s_width;   // find various info: size,offset etc.
       *(short *)(newres+2)=s_height;
       *(short *)(newres+4)=s_loffset;
       *(short *)(newres+6)=s_toffset;

       newptr=(long *)(newres+8); // the new column pointers for the new lump

       lastpt=8+(s_width*4);     // last point in the lump- point to start
                                 // of column data

       for(count=0;count<s_width;count++) // go through each column in turn
       {
               newptr[count]=lastpt; // point this column to lastpt
               memcpy(newres+lastpt,working+s_columns[count],
                          s_colsize[count]); // add the new column
               lastpt+=s_colsize[count]; // update lastpt
       }

       wadentry[entrynum].length=lastpt; // update the lump size
       free(working);                    // free back the original lump
       return newres;                    // use the new one
}


/* Find the redundant columns **********************************************/

int s_findgraf(char *x)
{
       int count, count2;
       int entrynum;    // entry number in wad
       int num_killed=0;

       s_width=*(short *)(x);
       s_height=*(short *)(x+2);
       s_loffset=*(short *)(x+4);
       s_toffset=*(short *)(x+6);

       s_columns=(long *)(x+8);

       for(count=0;count<s_width;count++) // each column in turn
       {
              s_equalcolumn[count]=-1; // first assume no identical column
                                       // exists

                                      // find the column size
              s_colsize[count]=s_find_colsize(x+s_columns[count]);

              for(count2=0;count2<count;count2++) //check all previous columns
              {
                     if(s_colsize[count]!=s_colsize[count2])
                            continue;   // columns are different sizes: must
                                        // be different
                     if(!memcmp(x+s_columns[count],x+s_columns[count2],
                                                        s_colsize[count]))
                     {                  // columns are identical
                            s_equalcolumn[count]=count2;
                            num_killed++;        // increase deathcount
                            break;      // found one, exit the loop
                     };
              }
       }
       return num_killed;  // tell squash how many can be 'got rid of'
}

/* Find the size of a column ***********************************************/

int s_find_colsize(unsigned char *col1)
{
       int count=0;

       while(1)
       {
            if(col1[count]==255)
            {                   // no more posts
                  return count+1;  // must be +1 or the pic gets cacked up
            }
            count=count+col1[count+1]+4; // jump to the beginning of the next
                                         // post
       }
}

/* Find if a graphic is squashed *******************************************/

int s_is_squashed(char *s)
{
      int entrynum;
      char *pic;
      int count,count2;

      entrynum=entry_exist(s);
      if(entrynum==-1) errorexit("is_squashed: %s does not exist!\n",s);
      pic=cachelump(entrynum); // cache the lump

      s_width=*(short *)(pic);      // find lump info
      s_height=*(short *)(pic+2);
      s_loffset=*(short *)(pic+4);
      s_toffset=*(short *)(pic+6);

      s_columns=(long *)(pic+8); // find the column locations

      for(count=0;count<s_width;count++) // each column
      {
            for(count2=0;count2<count;count2++) // every previous column
            {
                  if(s_columns[count]==s_columns[count2])
                  {  // these columns have the same lump location
                        free(pic);
                        return true;  // it is squashed
                  }
            }
      }
      free(pic);
      return false; // it cant be : no 2 columns have the same lump location

}

/* Is this a graphic ? *****************************************************/

int s_isgraphic(char *s)
{
      char *graphic;
      int entrynum , count;
      short width, height,loffset, toffset;
      long *columns;

      if(!strcmp(s,"ENDOOM")) return false;   // endoom
//      if(islevel(s)) return false;
      if(islevelentry(s)) return false;
      if(s[0]=='D' && (s[1]=='_' | s[1]=='S')) // sfx or music
       {
                   return false;
       }

      entrynum=entry_exist(s);
      if(entrynum==-1) errorexit("isgraphic: %s does not exist!\n",s);

      graphic=cachelump(entrynum);

      width=*(short *)(graphic);
      height=*(short *)(graphic+2);
      loffset=*(short *)(graphic+4);
      toffset=*(short *)(graphic+6);
      columns=(long *)(graphic+8);

      if((width>320) || (height>200) || (width==0) || (height==0)
        || (width<0) || (height<0) )
      {
            free(graphic);
            return false;
      }

         // it could be a graphic, but better safe than sorry
      if(wadentry[entrynum].length==4096 | // flat;
         wadentry[entrynum].length==4000)  // endoom
      {
            free(graphic);
            return false;
      }

      for(count=0;count<width;count++)
      {
             if(columns[count]>wadentry[entrynum].length)
             {    // cant be a graphic resource then -offset outside lump
                   free(graphic);
                   return false;
             }
      }
      free(graphic);
      return true;   // if its passed all these checks it must be(well probably)
}
