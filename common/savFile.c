/*
SAVList (C) 2005 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	"savFile.h"

/* 添え字が実際のセクタ番号、値は0から始まる(4+SECTORSIZE)バイトブロックの番号 */
static unsigned long sectorTable[SECTORS];
/* 最大ブロック番号 */
static unsigned long maxBlock;

/* セクタ番号テーブルに仮想フロッピーファイルの内容を読み込む。 */
int initialize(FILE *fpi)
{
	unsigned char head[4];
	long sector,blockNo;
	size_t ioCount;
	int result = 0,i,j,seekResult;

	for (i = 0;i < SECTORS;i++){
		sectorTable[i] = 0xffffffff;
	}

	blockNo = 0;
	while(feof(fpi) == 0){
		/* 最初の4バイトがセクタ番号 */
		ioCount = fread(head,1,4,fpi);
		if (ioCount < 4){
			if (ferror(fpi)){
				result = 1;
			}
			break;
		}

		/* セクタの中身は飛ばす。 */
		/* printf("seek\n"); */
		seekResult = fseek(fpi,SECTORSIZE,SEEK_CUR);
		if (seekResult){
			result = 1;
			break;
		}
		if (ferror(fpi)){
			result = 1;
			break;
		}

		/* 最初の4バイトがセクタ番号 */
		sector = 0;
		for (j = 0;j < 4;j++){
			sector = sector << 8;
			sector = sector | head[3 - j];
		}
		/* 最大セクタ数より大きいセクタ番号が出てきたら正常なファイルではないので抜ける。 */
		if (sector >= SECTORS){
			fprintf(stderr,"Bad virtual floppy disk file.\n");
			result = 1;
			break;
		}
		/* 同じセクタ番号が2度出てきたら正常なファイルではないので抜ける。 */
		if (sectorTable[sector] != 0xffffffff){
			fprintf(stderr,"Bad virtual floppy disk file.\n");
			result = 1;
			break;
		}
		
		sectorTable[sector] = blockNo;
		blockNo++;
	}
	maxBlock = blockNo;

	return result;
}

/* 仮想フロッピーディスクファイルのセクタを読み込む */
int readSector(FILE *fpi,unsigned long sectorNo,void *buf)
{
	long sectorPos;
	size_t ioCount;

	/* 存在しないセクタ番号を指定した場合、エラーとする。 */
	if (sectorNo >= SECTORS){
		return 1;
	}

	/* 仮想フロッピーに存在しないファイルだったらエラーにする。 */
	if (sectorTable[sectorNo] == 0xffffffff){
		return 1;
	}

	/* ファイル上の位置に直す。 */
	sectorPos = (long)(sectorTable[sectorNo] * (4 + SECTORSIZE));

	fseek(fpi,sectorPos + 4,SEEK_SET);
	ioCount = fread(buf,SECTORSIZE,1,fpi);
	if (ioCount < 1){
		return 1;
	}
	return 0;
}

/* 仮想フロッピーディスクファイルのセクタに書き込む */
int writeSector(FILE *fpo,unsigned long sectorNo,void *buf)
{
	long sectorPos;
	size_t ioCount;
	unsigned char secNoBuf[4];
	
	/* 存在しないセクタ番号を指定した場合、エラーとする。 */
	if (sectorNo >= SECTORS){
		return 1;
	}

	/* 仮想フロッピーに存在しないファイルだったら一番最後に書く。 */
	if (sectorTable[sectorNo] == 0xffffffff){
		fseek(fpo,maxBlock * (4 + SECTORSIZE),SEEK_SET);

		sectorTable[sectorNo] = maxBlock;
		secNoBuf[0] = (unsigned char)(sectorNo & 0xff);
		secNoBuf[1] = (unsigned char)((sectorNo >> 8) & 0xff);
		secNoBuf[2] = (unsigned char)((sectorNo >> 16) & 0xff);
		secNoBuf[3] = (unsigned char)((sectorNo >> 24) & 0xff);
		
		maxBlock++;
		
		/* セクタ番号部分を書き込む。 */
		ioCount = fwrite(secNoBuf,4,1,fpo);
		if (ioCount < 1){
			return 1;
		}
		
	}else{
		/* ファイル上の位置に直す。 */
		sectorPos = (long)(sectorTable[sectorNo] * (4 + SECTORSIZE));

		fseek(fpo,sectorPos + 4,SEEK_SET);
	}

	ioCount = fwrite(buf,SECTORSIZE,1,fpo);
	if (ioCount < 1){
		return 1;
	}
	return 0;
}
