# wadptr revision history

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

