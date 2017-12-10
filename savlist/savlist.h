/*
SAVList (C) 2005,2017 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#ifndef SAVLIST_H
#define SAVLIST_H

LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
ATOM InitApp(HINSTANCE);
BOOL InitInstance(HINSTANCE,int);
LRESULT CommandProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp);
LRESULT notifyProc(HWND &hWnd, UINT &msg, WPARAM &wp, LPARAM &lp);

HWND MakeMyList(HWND);
void InsertMyColumn(HWND);

void openSavFileSelect(void);
void openSavFile(char *filename);
void transferFiles(void);
void putTransferErrorMessage(int result);
void extractFiles(void);
int dirselect(HWND hWnd,char *buf);
void queryDeleteFiles(void);
void queryLFNKill(void);
void directoryUpKey(void);
void selectSubDirectory(void);
void setStatusBarInfo(void);
void displayHelpFile(void);
void versionDialog(void);

void getDirname(char *path);
void getSavDir(char *path);

#endif	/* SAVLIST_H */