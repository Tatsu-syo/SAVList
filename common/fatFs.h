#ifndef __FATFS_H
#define __FATFS_H

/* FAT12のエントリの値を得る。 */
unsigned long getFat12(unsigned long no);
/* FAT12のエントリに値を書き込む。 */
void setFat12(unsigned long no,unsigned long value);
/* 空きクラスタ番号を得る。 */
unsigned long getFreeCluster(void);
/* 空きクラスタ数を得る。 */
unsigned long getFreeClusterCount(void);
/* クラスタを読み込む */
int readCluster(FILE *fpi,unsigned long clusterNo,void *buf);
/* クラスタを書き込む */
int writeCluster(FILE *fpi,unsigned long clusterNo,void *buf);
/* ファイルに対応するFATエントリをクリアする。 */
void clearFatChain(unsigned long clusterNo);
/* ファイル/サブディレクトリに使うクラスタ数を集計する */
unsigned long countClusters(unsigned long clusterNo);

/* ディレクトリエントリ */
struct dirEntry {
	char filename[8];
	char ext[3];
	unsigned char attr;
	char reserved[11];
	unsigned short time;
	unsigned short date;
	unsigned short cluster;
	unsigned long size;
};

/* ディレクトリエントリの内容を得る。 */
void getDirectory(struct dirEntry *dir,unsigned long no);
/* ディレクトリエントリの内容をメモリにセットする。 */
void setDirectory(struct dirEntry *dir,unsigned long no);

/* ディスク情報 */
extern unsigned int FatSectors;	/* FAT用のセクタ数 */
extern unsigned long MaxEntry;	/* 最大エントリ数 */
extern unsigned int SectorPerCluster;	/* セクタあたりのクラスタ数 */
extern unsigned int DataArea;	/* データエリアの開始セクタ番号 */

/* 各情報管理用ワークエリア */
extern unsigned char *fat;	/* FATの内容を置く部分 */
extern unsigned char *DirEntryBuf;	/* ディレクトリエントリの内容を置く部分 */

#endif
