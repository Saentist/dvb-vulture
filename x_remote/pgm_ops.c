#include"pgm_ops.h"
/*
void pgmOpsTune(Widget w, XtPointer client_data, XtPointer call_data)
{
	PgmState * pod=client_data;
	if(pod->valid)
	{
		clientDoTune(pod->sock,pod->pos,pod->frequency,pod->pol,pod->pnr);
	}
}

void pgmOpsEpg(Widget w, XtPointer client_data, XtPointer call_data)
{
	PgmState * pod=client_data;
	EpgArray * epga=NULL;
	
	if(pod->valid)
	{
		epga=clientGetEpg(pod->sock,pod->pos,pod->frequency,pod->pol,&pod->pnr,1,24*60*60);
	}
	XtUnmap(pod->parent);
	XtMap(pod->epg);
	//hide current dialog and show epg here
//	epgShow(pgm,epga);
}

void pgmOpsVt(Widget w, XtPointer client_data, XtPointer call_data)
{
	PgmState * pod=client_data;
	//hide current dialog and show vt here
	XtUnmap(pod->parent);
	XtMap(pod->vt);
}

void pgmOpsRecStart(Widget w, XtPointer client_data, XtPointer call_data)
{
	PgmState * pod=client_data;
	clientRecordStart(pod->sock);
}

void pgmOpsRecStop(Widget w, XtPointer client_data, XtPointer call_data)
{
	PgmState * pod=client_data;
	clientRecordStop(pod->sock);
}
*/
