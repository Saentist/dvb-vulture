generated_headers=help.h
sources = $(wildcard *.c)
headers = $(wildcard *.h)
archive = archive.tar.bz2
include_dirs = -I ../include
lib_dirs = -L ../common
defines = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DDMALLOC_DISABLE -D_FORTIFY_SOURCE=2
#gcov_opt = -pg

exe_name = dvbv-nr

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
ALL_CFLAGS= -std=gnu99 $(defines) $(include_dirs) $(CFLAGS)  `pkg-config --cflags ncursesw` `pkg-config --cflags menuw`
LIBRARIES = -lsacommon `pkg-config --libs ncursesw` `pkg-config --libs menuw`
# -ldmallocth 

LDFLAGS ?= 
ALL_LDFLAGS = $(lib_dirs) $(LIBRARIES) $(LDFLAGS)
CPPFLAGS ?= 

.PHONY : all docs clean distclean install uninstall srcdoc texdoc indent

all : main texdoc

.c.o:
	$(CC) -c $(CPPFLAGS) $(ALL_CFLAGS) $<

docs : srcdoc texdoc

indent :
	indent -nut -i2 -lp -bli0 *.c *.h

texdoc : info/$(exe_name).info.gz info/$(exe_name).pdf

srcdoc : $(sources) 
	(cat Doxyfile ; echo PROJECT_NUMBER=`cat ../VERSION` ) | doxygen -

#invocation message dependency. just update timestamp
info/invoking.txi : info/dbg_main.txi info/dbg_common.txi
	touch info/invoking.txi

#rule for generating debug token list
info/dbg_main.txi : main.c
	( cd info;./dbg_main.sh;cd .. )

#rule for generating common debug token list
info/dbg_common.txi : ../common/debug.c
	( cd info;./dbg_common.sh;cd .. )

info/$(exe_name).html : $(wildcard info/*.txi) ../VERSION
	( cd info;texi2html ./$(exe_name).txi;cd .. )

info/$(exe_name).info.gz : $(wildcard info/*.txi) ../VERSION
	( cd info;makeinfo ./$(exe_name).txi;gzip -f ./$(exe_name).info;cd .. )

info/$(exe_name).pdf : $(wildcard info/*.txi) ../VERSION
	( cd info;texi2pdf ./$(exe_name).txi;cd .. )

$(archive) : clean $(sources) $(headers) makefile Doxyfile $(wildcard info/*.txi)
	tar -vcjf $(archive) $(wildcard *)

install : main info/$(exe_name).info.gz
	$(INSTALL_PROGRAM) -T ./main $(DESTDIR)$(bindir)/$(exe_name)
	$(INSTALL_DATA) -T ./info/$(exe_name).info.gz  $(DESTDIR)$(infodir)/$(exe_name).info.gz
	install-info $(DESTDIR)$(infodir)/$(exe_name).info.gz $(DESTDIR)$(infodir)/dir

uninstall :
	rm $(DESTDIR)$(bindir)/$(exe_name)
	install-info --delete $(DESTDIR)$(infodir)/$(exe_name).info.gz $(DESTDIR)$(infodir)/dir
	rm $(DESTDIR)$(infodir)/$(exe_name).info.gz


main : $(sources:.c=.o)
	$(CC) $(sources:.c=.o) $(ALL_CFLAGS) $(ALL_LDFLAGS) -o main 

#specific rule for generating help.h
help.h : info/invoking.txi
	( cd info;./gen_help_h.sh;cd .. )

%.d : %.c
	cc -MM -MG $(include_dirs) $< > $@.$$$$; \
sed 's,\($*\)\.o[ :]*,\1.o $@\: ,g; s,[^\\]$$,& makefile,' < $@.$$$$ > $@; \
rm -f $@.$$$$

ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), distclean)

include $(sources:.c=.d)

endif
endif

#arrgh ... texinfo diarrhea
#wipe their ass before packaging
distclean : clean
	rm -f main
	rm -f info/dbg_common.txi
	rm -f info/dbg_main.txi
	rm -f info/*.pdf
	rm -f info/*.info.gz
	rm -f info/*.html
	rm -f info/*.aux
	rm -f info/*.cp
	rm -f info/*.fn
	rm -f info/*.ky
	rm -f info/*.log
	rm -f info/*.pg
	rm -f info/*.toc
	rm -f info/*.tp
	rm -f info/*.bak
	rm -f info/*.vr

clean : 
	rm -f $(sources:.c=.d)
	rm -f $(sources:.c=.o)
	rm -f $(generated_headers)
	rm -f *~
	rm -f *.bak
	rm -fr doc/*
