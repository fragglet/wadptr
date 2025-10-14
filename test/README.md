Test WADs for wadptr compression. Use the `run_tests.sh` script to
run tests. Included here are:

* The `6fiffy*` series, levels by George Fiffy (King REoL) who is
  notorious for his "extreme detail" levels that contain a large number
  of linedefs and sidedefs. `6fiffy6.wad` is a Hexen format level,
  which checks that we handle such WADs appropriately.
* Eternal Doom,  which is similarly notorious for having very large
  levels.
* `crymap02.wad` is MAP02 from [Cryogenics](https://doomwiki.org/wiki/Cryogenics),
  a large level that contains an empty BLOCKMAP lump.
* `evimap32.wad` is MAP32 from [Eviternity](https://doomwiki.org/wiki/Eviternity),
  an exceedingly large level.
* `btsx2m20.wad` is MAP20 from [BTSX E2](https://doomwiki.org/wiki/Back_to_Saturn_X),
  which is a very large level close to the vanilla BLOCKMAP limit and
  that has been built with ZokumBSP to make it fit within the limit. So
  we check that we can at least process that BLOCKMAP without making it
  any larger. The WAD has a single redundant byte at the end so that
  wadptr will successfully reduce its size.
* `btsxcred.wad` is the CREDIT screen graphic from btsx\_e1a.wad, which
  has its columns unnecessarily spread into two posts, and checks that
  the code to combine posts works as intended.
* `64c30n9.wad` is a Doom 64-format WAD. A single zero byte has been
  appended to the end of the WAD so that the tests successfully reduce
  the file size.
* `gotcha3.wad` contains a graphic lump (PLATFORM) that is corrupted;
  we print an error message when such lumps are encountered but
  otherwise proceed without changing their contents.

Mini / unit test WADs:

* `samelump.wad` contains two lumps with identical contents; this
  confirms that the WAD merging code works as intended.
* `samecol.wad` has a single graphic containing two identical columns,
  to confirm those will be combined.
* `suffix.wad` has a single graphic containing two columns that are
  different, but one column is a suffix of the other.
* `packable.wad` contains a minimal level with four identical sidedefs
  that can be packed, but the blockmap is too small to be stackable.
* `hxpackable.wad`, same thing but in Hexen format.
* `stackable.wad` contains a minimal level with no identical sidedefs
  (cannot be packed), but is large enough that the blockmap can be
  stacked.
* `mixed.wad` contains both a Doom format level and a Hexen format
  level, to check that we correctly handle mixed-format WADs.
* `bad_sdi.wad` is a copy of `packable.wad` containing a bad sidedef
  reference on one of its linedefs, to confirm that such WADs can
  still be processed successfully (albeit with an error message).
* `special_tag.wad` contains special lines (scrolling walls), the sidedefs
  of which would not normally be merged, except that they have a sector
  tag of 9701, in the magic range that wadptr recognizes for special
  effects.
