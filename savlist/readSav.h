/*
SAVList (C) 2005,2017 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#ifndef READSAV_H
#define READSAV_H

#include "fatFs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NO_DISKSPACE 2
#define NO_ENTRY 3

int writeFile(char *filename);
int extractFile(char *dir,int listviewEntry);
void cleanup(void);
int readSavFile(char *filename);
void getSavStatus(struct SavStatus *status);

void refreshDir(HWND hList);
void selectAllFiles(HWND hList);
int upDirectory(HWND hList);
void enterDirectory(HWND hList,int listviewEntry);
void deleteSelectedFiles(HWND hList, int position);
void deleteLongFilename(HWND hList);
void setTimeStamp(struct dirEntry *d, char *filename);

struct SavStatus {
	unsigned long files;
	unsigned long directorys;
	unsigned long freeSize;
	unsigned long maxSize;
};

extern int listViewEntrys;

#ifdef __cplusplus
}
#endif

#endif
