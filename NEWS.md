# wadptr revision history

## 3.2 (2023-10-14)

 * The sidedef packing code is now considerably more efficient,
   allowing some levels to be processed in a fraction of the time it
   took for previous versions.
 * Very large levels are now safely handled, and the vanilla sidedefs
   limit will never be overflowed.
 * The graphics squashing code was rewritten entirely, and is now
   slightly more effective than before.
 * The `-o` command line argument was fixed.
 * A bug with handling of special linedefs was fixed.
 * Empty lumps are now described as "empty", instead of "stored".
 * Style for the HTML documentation files was made nicer.
 * There is now a `-v` command line argument to show the version.
 * When showing the summary of a newly-compressed file, the number of
   bytes smaller is now shown as well as the percentage reduction.
 * The Windows .exe now has an icon.

## 3.1 (2023-10-07)

 * This release adds support for BLOCKMAP lump compression. For
   compatibility reasons, the algorithm is deliberately conservative
   and does not use some of the more aggressive optimizations supported
   by ZokumBSP, but a reference to ZokumBSP was added in the manual.
 * The WAD lump merging code was rewritten. It is now much faster, and
   lump data is now arranged within generated WAD files in a way that
   helps make WADs compress better when compressed as eg. .zip or .gz.
 * The list command (-l) no longer hides level sub-lumps like BLOCKMAP
   or SIDEDEFS. The full set of lumps is always listed.
 * A bug was fixed where it wasn't possible to list the contents of
   Hexen format WADs.
 * The Chocolate Doom quickcheck regression test suite was integrated
   to prevent demo desyncs from being introduced by wadptr.
 * A static limit on graphic sizes was eliminated.

## 3.0 (2023-09-30)

 * A bug was fixed with sidedef packing where if multiple scrolling walls
   would share a sidedef, those walls would scroll at the wrong speed
   (thanks to viti95 for reporting this on Doomworld). This has been
   fixed in a generic way that should also cover other potential bugs
   related to animated walls.
 * Hexen format WADs, while not yet fully supported, can be safely
   handled (sidedef packing is automatically disabled for these WADs).
 * The progress indicator has been fixed.
 * Some of the more spammy program output has been removed. There is now
   a '-q' command line option to suppress normal output.
 * The old 'wadptr.txt' documentation file was replaced with a
   better-written Unix manpage.
 * Static limits on the number of lumps in a WAD and the number of
   sidedefs in a level have been removed.
 * The original file is now replaced with the compressed or decompressed
   version in a safer way; it always happens via an atomic rename as the
   final step of processing. Temporary files are also created more safely.
 * The Makefile was simplified and special support for old operating
   systems like Solaris and HP-UX has been dropped. RISC OS support has
   also been ditched.
 * Source code was significantly cleaned up. Code has been reformatted
   and functions renamed to better names. Legacy code left over from the
   original DOS version was removed. The C99 boolean and fixed-width
   integer types are now used where appropriate.
 * Many static buffers have been eliminated in the code. The deprecated
   `strcpy()` function is no longer used anywhere in the codebase.
 * There are now automated tests using several large WAD files.

## 2.4 (2011-08-05)

Fixes from Jan Engelhardt:

 * fix 64-bit compilation.
 * fix progress bar.
 * add .spec file for RPM build.

## 2.3 GPL re-release (2001-07-18)

 - Re-released under the GNU GPL.

## 2.3

Enhancements by Andreas Dehmel <dehmel@forwiss.tu-muenchen.de>.

 * Removal of another limit
 * Slightly better graphic squashing.
 * More portable code
 * UNIX/RISCOS/Linux version.
 * -o (output file) option.

## 2.2

 * Removed limits on level sizes(more-or-less). Building of very large
   levels should now be possible.
 * Support for levels without the standard 'MAPxy'/'ExMy' format for use
   in SMMU wads.
 * Added percent-done counter to level building

## 2.1

 * Added wildcards
 * better file-offset in -l(list) option.
 * Old wad files are not kept after compress/uncompressing.
 * -nomerge, -nosquash, -nopack options.
 * Warning before changing IWAD.

## 2.0

 * Removed all old options, added new all-in-one -compress option.
 * Added uncompressing and graphic squashing.
 * New PKZIP style interface rather than DEUTEX interface.
 * More detailed -list option.
 * Code can now be easily incorporated into other programs if neccesary.

## 1.4

 * Added '-pack' sidedef packing option and '-works' "do the works" option.

## 1.3

 * Added '-rebuild' option.
 * WAD compression is now completely automatic.
 * '-list' option is now default if no option is chosen, but WAD is still
   specified.
 * '-help' appears if no WAD is specified.
 * Old 'DMWAD' code removed.

## 1.2

 * Added '-suggest' option to make use of the '-tweak' option easier.

## 1.1

 * Original release, only options are '-list', '-help' and '-tweak'.

