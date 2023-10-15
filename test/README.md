Test WADs for wadptr compression. Use the `run_tests.sh` script to
run tests. Included here are:

* The `6fiffy*` series, levels by George Fiffy (King REoL) who is
  notorious for his "extreme detail" levels that contain a large number
  of linedefs and sidedefs. `6fiffy6.wad` is a Hexen format level,
  which checks that we handle such WADs appropriately.
* Eternal Doom,  which is similarly notorious for having very large
  levels.
* `evimap32.wad` is MAP32 from [Eviternity](https://doomwiki.org/wiki/Eviternity),
  an exceedingly large level.
* `btsx2m20.wad` is MAP20 from [BTSX E2](https://doomwiki.org/wiki/Back_to_Saturn_X),
  which is a very large level close to the vanilla BLOCKMAP limit and
  that has been built with ZokumBSP to make it fit within the limit. So
  we check that we can at least process that BLOCKMAP without making it
  any larger. The WAD has a single redundant byte at the end so that
  wadptr will successfully reduce its size.
