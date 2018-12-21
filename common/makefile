sources = $(wildcard *.c)
defines = -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DDMALLOC_DISABLE -D_FORTIFY_SOURCE=2
include_dirs = -I ../include

file_name = libsacommon.so
prefix ?= /usr/local
exec_prefix ?= $(prefix)
libdir ?= $(exec_prefix)/lib
INSTALL ?= install
CC ?= cc
INSTALL_DATA ?= $(INSTALL) -m 664
#gcov_opt = --coverage
#gcov_opt = -pg
opt?=-Os

CFLAGS ?= -g $(opt) -fno-common -fstrict-aliasing -fstack-protector -Wall -Wextra 
#the gnu99 seems necessary for the bool hijinks
ALL_CFLAGS= -std=gnu99 -fPIC $(defines) $(include_dirs) $(CFLAGS)
#LIBRARIES = -ldmallocth

LDFLAGS ?= 
ALL_LDFLAGS = -shared -Wl,-soname,$(file_name) $(gcov_opt) -Wl,-export-dynamic $(lib_dirs) $(LIBRARIES) $(LDFLAGS)
CPPFLAGS ?= 

#SUBDIRS = $(wildcard */)
#all sub directories

#.PHONY : subdirs $(SUBDIRS)

.PHONY : all docs clean distclean install uninstall main indent

all : $(file_name)

.c.o:
	$(CC) -c $(CPPFLAGS) $(ALL_CFLAGS) $<

docs : $(sources) 
	(cat Doxyfile ; echo PROJECT_NUMBER=`cat ../VERSION` ) | doxygen -

indent :
	indent -nut -i2 -lp -bli0 *.c win/*.c lin/*c ../include/*.h

main : $(file_name)

install : $(file_name)
	$(INSTALL_DATA) $(file_name) $(DESTDIR)$(libdir)/$(file_name)

uninstall :
	rm $(DESTDIR)$(libdir)/$(file_name)

# subdirs

#subdirs : $(SUBDIRS)

#$(SUBDIRS) :
#	$(MAKE) -C $@ $(MAKECMDGOALS)


$(file_name) : $(sources:.c=.o)
	$(CC) $(sources:.c=.o) $(ALL_CFLAGS) $(ALL_LDFLAGS) -o $(file_name)


%.d : %.c makefile
	cc -MM -MG $(include_dirs) $< > $@.$$$$; \
sed 's,\($*\)\.o[ :]*,\1.o $@\: ,g; s,[^\\]$$,& makefile,' < $@.$$$$ > $@; \
rm -f $@.$$$$

ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), distclean)

include $(sources:.c=.d)

endif
endif

include dvb_string.tmk
include rcfile.tmk

distclean : clean
	rm -f $(file_name)

clean : 
	rm -f $(sources:.c=.d)
	rm -f $(sources:.c=.o)
	rm -fr doc/*
	rm -f *~
	rm -f ../include/*~
	rm -f *.bak
	rm -f dvbstr_tst

