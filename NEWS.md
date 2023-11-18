# wadptr revision history

## 3.6 (2023-11-18)

 * A summary table is now printed after compression, listing how many
   bytes were saved and through which methods.
 * The program doesn't abort any more if a level is encountered that
   has invalid sidedef references. Instead, a warning is printed, and
   the level is skipped.
 * Larger graphics can now be squashed (the limit was previously
   320x200 and is now 1024x240).
 * Squashed graphics are now detected more accurately. Some lumps in
   the list output were previously being described as unsquashed when
   they were actually squashed.
 * Warning messages now include context about the file and the lump
   that were being processed.

## 3.5 (2023-11-04)

 * There is now better error reporting; error messages now show the
   filename and the lump being processed when the error occurred.
 * Slightly more accurate percentage values are displayed when lumps
   or WADs are reduced in size by less than 10%.
 * PSX/Doom 64 format levels are now handled correctly rather than
   aborting with an error (they are ignored). Thanks @Kroc.
 * Under-length BLOCKMAP lumps are now ignored without aborting with
   an error. Thanks @Kroc.
 * Corrupt graphic lumps are now ignored without aborting with an
   error. Thanks @Kroc.
 * WADs containing a mixture of Doom-format and Hexen-format levels
   are now correctly handled. Thanks @Kroc.
 * There is now a message printed when SIDEDEFS lumps grow in size,
   and there is a new section in the manual that explains why this can
   happen sometimes (thanks @Kroc).

## 3.4 (2023-10-29)

 * A bug was fixed on Windows that prevented new files from being
   written and therefore made the program not work at all. Big thanks
   go to @Kroc on GitHub for reporting this issue and for assistance
   in debugging.

## 3.3 (2023-10-21)

 * Compression of Hexen format levels is now supported.
 * A bug was fixed that prevented blockmaps from being stacked.
 * Two new arguments, `-extblocks` and `-extsides`, have been added
   to allow use of the extended blockmap and sidedef limits that are
   supported by some source ports.
 * Corrupt BLOCKMAP lumps are now detected and no attempt is made to
   process them.
 * Compression of BLOCKMAP lumps is now very slightly better.
 * The graphics squashing code can now do a slightly better job of
   compressing some larger graphics.
 * The code has now been tested successfully on FreeBSD, NetBSD and
   OpenBSD.
 * The test suite now includes a set of "unit test" WADs as regression
   tests for all the individual features.

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

