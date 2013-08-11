/*
SAVList (C) 2005 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#include	<windows.h>
#include	<commctrl.h>
#include	<shlobj.h>
#include	<objbase.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	"readSav.h"
#include	"resource.h"

#define MYWEB_URL "http://homepage3.nifty.com/Tatsu_syo/"

BOOL CALLBACK VerDialogProc(HWND hDlg,UINT msg,WPARAM wp,LPARAM lp)
{
	switch (msg){
		case WM_COMMAND:
			switch(LOWORD(wp)){
				case IDOK:
					EndDialog(hDlg,IDOK);
					break;
				case IDSUPPORT:
					ShellExecute(NULL,NULL,MYWEB_URL,NULL,NULL,SW_SHOW);
					break;

			}
			return FALSE;
	}
	return FALSE;

}
