#ifndef __SAVFILE_H
#define __SAVFILE_H

#define SECTORS 1440
#define SECTORSIZE 512

/* セクタ番号テーブルに仮想フロッピーファイルの内容を読み込む。 */
int initialize(FILE *fpi);
/* 仮想フロッピーディスクファイルのセクタを読み込む */
int readSector(FILE *fpi,unsigned long sectorNo,void *buf);
/* 仮想フロッピーディスクファイルのセクタに書き込む */
int writeSector(FILE *fpo,unsigned long sectorNo,void *buf);

#endif
