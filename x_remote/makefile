sources = $(wildcard *.c)
headers = $(wildcard *.h)
archive = archive.tar.bz2
include_dirs = -I ../include
lib_dirs = -L ../common
defines = -D_REENTRANT -DDMALLOC_DISABLE -D_FORTIFY_SOURCE=2

exe_name = dvbv-xr

#directories
prefix ?= /usr/local
exec_prefix ?= $(prefix)
datarootdir ?= $(prefix)/share
bindir ?= $(exec_prefix)/bin
infodir ?= $(datarootdir)/info
INSTALL ?= install
CC ?= cc
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 664

#compiling and linking
opt?=-Os
CFLAGS ?= -g $(opt) -fno-common -fstrict-aliasing -fstack-protector -Wall -Wextra 
ALL_CFLAGS= -std=gnu99 $(defines) $(include_dirs) $(CFLAGS)
LIBRARIES = -lXext -lX11 -lXt -lXaw -lsacommon
# -ldmallocth
LDFLAGS ?= 
ALL_LDFLAGS = $(lib_dirs) $(LIBRARIES) $(LDFLAGS)
CPPFLAGS ?= 


#SUBDIRS = $(wildcard */)
#all sub directories

#.PHONY : subdirs $(SUBDIRS)

.PHONY : all clean distclean install uninstall indent

all : main

.c.o:
	$(CC) -c $(ALL_CFLAGS) $<

docs : srcdoc

indent :
	indent -nut -i2 -lp -bli0 *.c *.h

srcdoc : $(sources) ../VERSION
	(cat Doxyfile ; echo PROJECT_NUMBER=`cat ../VERSION` ) | doxygen -

$(archive) : clean $(sources) $(headers) makefile
	tar -vcjf $(archive) $(wildcard *)

install : main
	$(INSTALL_PROGRAM) -T ./main $(DESTDIR)$(bindir)/$(exe_name)

uninstall :
	rm $(DESTDIR)$(bindir)/$(exe_name)

# subdirs

#subdirs : $(SUBDIRS)

#$(SUBDIRS) :
#	$(MAKE) -C $@ $(MAKECMDGOALS)


main : $(sources:.c=.o)
	$(CC) $(sources:.c=.o) $(ALL_CFLAGS) $(ALL_LDFLAGS) -o main 


%.d : %.c
	$(CC) -MM -MG $(include_dirs) $< > $@.$$$$; \
sed 's,\($*\)\.o[ :]*,\1.o $@\: ,g; s,[^\\]$$,& makefile,' < $@.$$$$ > $@; \
rm -f $@.$$$$

ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), distclean)

include $(sources:.c=.d)

endif
endif

distclean : clean
	rm -f main

clean : 
	rm -f $(sources:.c=.d)
	rm -f $(sources:.c=.o)
	rm -f *~
	rm -f *.bak
	rm -fr doc/*

