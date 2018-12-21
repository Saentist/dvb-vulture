date = $(shell date -I)
archive =dvbv_$(date).tar.bz2
PGM_DIRS=ncurses_remote/ stream/ x_remote/ tsverify/ bandscan/
LIB_DIRS=common/ 
#SUBDIRS = $PGM_DIRS $LIB_DIRS 
#all sub directories

.PHONY : subdirs libs pgms $(PGM_DIRS) $(LIB_DIRS) install uninstall clean distclean dist main docs indent

#.NOTPARALLEL : subdirs

# subdirs
install uninstall clean distclean main indent docs: pgms

#should prolly have this do the debian source thingy...and distclean
dist : $(archive)

$(archive) :
	(cd ..; tar --exclude=dvbv/.git --group nogroup --owner nobody -vcjf $(archive) ./dvbv; cd dvbv)

pgms : $(PGM_DIRS)

libs : $(LIB_DIRS)

$(LIB_DIRS) :
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(PGM_DIRS) : libs
	$(MAKE) -C $@ $(MAKECMDGOALS)
