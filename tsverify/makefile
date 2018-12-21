generated_headers=help.h
sources = $(wildcard *.c)
headers = $(wildcard *.h)
archive = archive.tar.bz2
include_dirs = -I ../include
lib_dirs = -L ../common
defines = -D_REENTRANT -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DDMALLOC_DISABLE
#gcov_opt = --coverage
#gcov_opt = -pg

exe_name = dvbv-tsverify
prefix ?= /usr/local
exec_prefix ?= $(prefix)
datarootdir ?= $(prefix)/share
bindir ?= $(exec_prefix)/bin
infodir ?= $(datarootdir)/info

INSTALL ?= install
CC ?= cc
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 664

opt?=-Os

CFLAGS ?= -g $(opt) -fno-common -fstrict-aliasing -fstack-protector -Wall -Wextra 
ALL_CFLAGS= -std=gnu99 $(defines) $(include_dirs) $(CFLAGS)
LIBRARIES =  -lsacommon
# -ldmallocth

LDFLAGS ?= 
ALL_LDFLAGS = $(lib_dirs) $(LIBRARIES) $(LDFLAGS)
CPPFLAGS ?= 

.PHONY : all docs clean distclean install uninstall srcdoc indent

all : main

.c.o:
	$(CC) -c $(CPPFLAGS) $(ALL_CFLAGS) $<

docs : srcdoc

indent :
	indent -nut -i2 -lp -bli0 *.c *.h

srcdoc : $(sources) ../VERSION
	(cat Doxyfile ; echo PROJECT_NUMBER=`cat ../VERSION` ) | doxygen -

$(archive) : clean $(sources) $(headers) makefile Doxyfile
	tar -vcjf $(archive) $(wildcard *)

install : main
	$(INSTALL_PROGRAM) -T ./main $(DESTDIR)$(bindir)/$(exe_name)

uninstall :
	rm $(DESTDIR)$(bindir)/$(exe_name)

main : $(sources:.c=.o)
	$(CC) $(sources:.c=.o) $(ALL_CFLAGS) $(ALL_LDFLAGS) -o main 

%.d : %.c makefile
	cc -MM -MG $(include_dirs) $< > $@.$$$$; \
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
