dvb-vulture
===========

This Archive contains the DVB Vulture, which
is a DVB streaming software with features. 

What it can do:
* RTP streaming. Multiple services possible.
* Recording. both timed and unconstrained. Multiple services possible.
* Few library/software dependencies.
* The libraries it does depend on are rather mature(=old) and stable.
* Automated service listing and selection. Never again dvbscan and crappy channel list text files.
* Background service/transponder list updates.
* System clock update.
* Stream announcement(using SAP/SDP)
* EPG schedule collection(+Viewer)
* Videotext collection(+Viewer)
* DVB-S and DVB-T support(DVB-S2 is untested and may not work)
* Complex switch/antenna Setups
* Follows pid changes by tracking PMT.
* Reference counting software demultiplexer, no copying of packets.
* Basic multi-user arbitration.

What it can _not_ do:
- Pay TV(Not even with CI and card)
- Play streams or files. But vlc or xine work well.
- Hbb stuff(there's a downloader that I nicked from Simon Kilvingtons redbutton software, but no viewer)

It is licensed under the GPLv3.

Build Configuration
-------------------

There is no configure script. 
If you want to change something, you will have to edit
the makefiles yourself. 

You can override some make variables: 

`prefix` defaults to `/usr/local`
`exec_prefix` defaults to  `$(prefix)`
`datarootdir` defaults to `$(prefix)/share`
`bindir` defaults to `$(exec_prefix)/bin`
`infodir` defaults to `$(datarootdir)/info`
`libdir` defaults to `$(exec_prefix)/lib`

Installation
------------

Just `make install` in the top level source directory, 
to make and install all necessary files (executable and .info manuals). 
It is also possible to create a .deb binary package, 
using `dpkg-buildpackage -uc -us -b` here. Parallel builds should work, 
too: `dpkg-buildpackage -uc -us -b -j20`.

Docs
----

The man page is just a stub. Documentation is maintained in texinfo format.
Texinfo pages can be viewed using `info dvbv-stream`
or `info dvbv-nr`. 

Dmalloc
-------

When compiling with `-DDMALLOC_DISABLE` make sure to 
use it for _all_ of the programs as well as the _library_. 
Otherwise there _will_ be segfaults. Background: pointers to dynamically 
allocated blocks of memory are passed between program and library. 
Some are freed/allocated on either side. 

Library dependencies(no need for the exact same versions, though)
----------------------------------------------------------------

dvbv-stream:

	linux-gate.so.1 =>  (0xb783f000)
	libdmallocth.so.5 => /usr/lib/libdmallocth.so.5 (0xb77a6000)
	libpthread.so.0 => /lib/i386-linux-gnu/libpthread.so.0 (0xb778d000)
	libsacommon.so => /usr/lib/libsacommon.so (0xb7770000)
	libc.so.6 => /lib/i386-linux-gnu/libc.so.6 (0xb762b000)
	/lib/ld-linux.so.2 (0xb7840000)

dvbv-nr:

	linux-gate.so.1 =>  (0xb784f000)
	libsacommon.so => /usr/lib/libsacommon.so (0xb7819000)
	libdmallocth.so.5 => /usr/lib/libdmallocth.so.5 (0xb7799000)
	libncurses.so.5 => /lib/libncurses.so.5 (0xb7760000)
	libmenu.so.5 => /usr/lib/libmenu.so.5 (0xb7759000)
	libc.so.6 => /lib/i386-linux-gnu/libc.so.6 (0xb7614000)
	libdl.so.2 => /lib/i386-linux-gnu/libdl.so.2 (0xb760f000)

dvbv-xr:
	linux-gate.so.1 =>  (0xb7784000)
	libXext.so.6 => /usr/lib/i386-linux-gnu/libXext.so.6 (0xb7751000)
	libX11.so.6 => /usr/lib/i386-linux-gnu/libX11.so.6 (0xb7619000)
	libXt.so.6 => /usr/lib/i386-linux-gnu/libXt.so.6 (0xb75bc000)
	libXaw.so.7 => /usr/lib/libXaw.so.7 (0xb755f000)
	libsacommon.so => /usr/lib/libsacommon.so (0xb753f000)
	libc.so.6 => /lib/i386-linux-gnu/libc.so.6 (0xb73f5000)
	libxcb.so.1 => /usr/lib/i386-linux-gnu/libxcb.so.1 (0xb73d2000)
	libdl.so.2 => /lib/i386-linux-gnu/libdl.so.2 (0xb73ce000)
	libSM.so.6 => /usr/lib/i386-linux-gnu/libSM.so.6 (0xb73c6000)
	libICE.so.6 => /usr/lib/i386-linux-gnu/libICE.so.6 (0xb73ad000)
	libXmu.so.6 => /usr/lib/i386-linux-gnu/libXmu.so.6 (0xb7393000)
	libXpm.so.4 => /usr/lib/i386-linux-gnu/libXpm.so.4 (0xb7382000)
	/lib/ld-linux.so.2 (0xb7785000)
	libXau.so.6 => /usr/lib/libXau.so.6 (0xb737f000)
	libXdmcp.so.6 => /usr/lib/libXdmcp.so.6 (0xb737a000)
	libuuid.so.1 => /lib/i386-linux-gnu/libuuid.so.1 (0xb7374000)


libsacommon.so:

	/lib/ld-linux.so.2 (0xb7850000)
	linux-gate.so.1 =>  (0xb77ca000)
	libdmallocth.so.5 => /usr/lib/libdmallocth.so.5 (0xb7714000)
	libc.so.6 => /lib/i386-linux-gnu/libc.so.6 (0xb75cf000)
	/lib/ld-linux.so.2 (0xb77cb000)

..and recently zlib(for the unfinished carousel stuff)

The source-code may be found at http://members.aon.at/~aklamme4/files.html

Note: the tab width should be set to 2 spaces when reading the source code. 
I tried to avoid spaces in indentation, but some may have crept in...
