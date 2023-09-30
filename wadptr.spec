
Name:		wadptr
Version:	3.0
Release:	0
Group:		Development/Tools/Building
Summary:	Redundancy compressor for Doom WAD files
License:	GPLv2+
URL:		https://soulsphere.org/projects/wadptr/

Source:		wadptr-%version.tar.gz
BuildRoot:      %_tmppath/%name-%version-build

%description
WADptr is a utility for reducing the size of Doom WAD files. The
"compressed" WADs will still work the same as the originals. The
program works by exploiting the WAD file format to combine repeated /
redundant material.

Authors:
--------
	Simon "Fraggle" Howard
	Andreas Dehmel

%prep
%setup

%build
make

%install
b="%buildroot";
make install DESTDIR="$b" PREFIX=/usr;

%files
%defattr(-,root,root)
%_bindir/*

%changelog
