

#building pdcurses for dos in linux:
#download pdcurses
#extract in wat/pdcurses
#copy wcclin.mak to its dos directory
#set PDCURSES_SRCDIR=pdcurses
#cd wat
#run wmake -f pdcurses/dos/wcclin.mak
#cd ..

#watt32 has exe files and cannot be built natively...
#could try copy in the generated files and attempt native copmile or just
#tell users to use dosbox or put binary WATTCPWF.LIB in wat 
#
#

#make file for wmake..
#should work in DOS

!ifdef __LINUX__
RM=rm -f
incpath=../include

!else
RM=del
incpath=..\include

!endif


CC=wcc386
CFLAGS= -wx -d2 -i=$(incpath) -mf -bt=dos


#how to get a list of .c files?
#do not want to update makefile when adding/removing...

object_files=bifi.obj    debug.obj       fav_list.obj   iterator.obj  rcptr.obj       timestamp.obj &
bits.obj    dhash.obj       fp_mm.obj      list.obj      reminder.obj    tp_info.obj &
btre.obj    dvb_string.obj  in_out.obj     opt.obj       rs_entry.obj    utl.obj &
client.obj  epg_evt.obj     ipc.obj        pgm_info.obj  size_stack.obj  vt_pk.obj &
custck.obj  fav.obj         item_list.obj  rcfile.obj    switch.obj



all : main .SYMBOLIC

main : sacommon.lib .SYMBOLIC

.c.obj: .AUTODEPEND
	$(CC) $(CFLAGS) $< -fo=$@

sacommon.lib : $(object_files)
 %create ppcommon.lbl
 @for %i in ($(object_files)) do @%append sacommon.lbl +'%i'
 *wlib -b -c -n -q $@ @ppcommon.lbl



clean : .SYMBOLIC
	$(RM) *.obj *.OBJ *.lbl *.LBL *.lnk *.LNK *.err *.ERR

distclean : clean .SYMBOLIC
	$(RM) *.EXE *.exe *.lib *.LIB *.MAP *.map *.COM *.com
