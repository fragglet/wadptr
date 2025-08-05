![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/fragglet/wadptr/c-cpp.yml)
[![CodeFactor](https://www.codefactor.io/repository/github/fragglet/wadptr/badge)](https://www.codefactor.io/repository/github/fragglet/wadptr)
![GitHub License](https://img.shields.io/github/license/fragglet/wadptr)
![GitHub Release](https://img.shields.io/github/v/release/fragglet/wadptr)
![GitHub Downloads (all assets, latest release)](https://img.shields.io/github/downloads/fragglet/wadptr/latest/total)
![GitHub repo size](https://img.shields.io/github/repo-size/fragglet/wadptr)
![GitHub Repo stars](https://img.shields.io/github/stars/fragglet/wadptr)

wadptr is a tool for compressing Doom .wad files. It takes advantage of the
structure of the WAD format and some of the lumps stored inside it to merge
repeated data. [The manpage](https://soulsphere.org/projects/wadptr/manual.html)
has some more information.

Building wadptr requires a toolchain with at least C99 and POSIX.1-2008
support, plus GNU make. Contemporary systems based on Linux, BSD,
Solaris/Illumos, Cygwin or mingw-w64 will do.

["You should have used WADPTR"](https://www.youtube.com/watch?v=EO849hbGP-c) - unhappily dshrunk fraggle
