/*
SAVList (C) 2005 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	"savFile.h"

/* �Y���������ۂ̃Z�N�^�ԍ��A�l��0����n�܂�(4+SECTORSIZE)�o�C�g�u���b�N�̔ԍ� */
static unsigned long sectorTable[SECTORS];
/* �ő�u���b�N�ԍ� */
static unsigned long maxBlock;

/* �Z�N�^�ԍ��e�[�u���ɉ��z�t���b�s�[�t�@�C���̓��e��ǂݍ��ށB */
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
		/* �ŏ���4�o�C�g���Z�N�^�ԍ� */
		ioCount = fread(head,1,4,fpi);
		if (ioCount < 4){
			if (ferror(fpi)){
				result = 1;
			}
			break;
		}

		/* �Z�N�^�̒��g�͔�΂��B */
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

		/* �ŏ���4�o�C�g���Z�N�^�ԍ� */
		sector = 0;
		for (j = 0;j < 4;j++){
			sector = sector << 8;
			sector = sector | head[3 - j];
		}
		/* �ő�Z�N�^�����傫���Z�N�^�ԍ����o�Ă����琳��ȃt�@�C���ł͂Ȃ��̂Ŕ�����B */
		if (sector >= SECTORS){
			fprintf(stderr,"Bad virtual floppy disk file.\n");
			result = 1;
			break;
		}
		/* �����Z�N�^�ԍ���2�x�o�Ă����琳��ȃt�@�C���ł͂Ȃ��̂Ŕ�����B */
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

/* ���z�t���b�s�[�f�B�X�N�t�@�C���̃Z�N�^��ǂݍ��� */
int readSector(FILE *fpi,unsigned long sectorNo,void *buf)
{
	long sectorPos;
	size_t ioCount;

	/* ���݂��Ȃ��Z�N�^�ԍ����w�肵���ꍇ�A�G���[�Ƃ���B */
	if (sectorNo >= SECTORS){
		return 1;
	}

	/* ���z�t���b�s�[�ɑ��݂��Ȃ��t�@�C����������G���[�ɂ���B */
	if (sectorTable[sectorNo] == 0xffffffff){
		return 1;
	}

	/* �t�@�C����̈ʒu�ɒ����B */
	sectorPos = (long)(sectorTable[sectorNo] * (4 + SECTORSIZE));

	fseek(fpi,sectorPos + 4,SEEK_SET);
	ioCount = fread(buf,SECTORSIZE,1,fpi);
	if (ioCount < 1){
		return 1;
	}
	return 0;
}

/* ���z�t���b�s�[�f�B�X�N�t�@�C���̃Z�N�^�ɏ������� */
int writeSector(FILE *fpo,unsigned long sectorNo,void *buf)
{
	long sectorPos;
	size_t ioCount;
	unsigned char secNoBuf[4];
	
	/* ���݂��Ȃ��Z�N�^�ԍ����w�肵���ꍇ�A�G���[�Ƃ���B */
	if (sectorNo >= SECTORS){
		return 1;
	}

	/* ���z�t���b�s�[�ɑ��݂��Ȃ��t�@�C�����������ԍŌ�ɏ����B */
	if (sectorTable[sectorNo] == 0xffffffff){
		fseek(fpo,maxBlock * (4 + SECTORSIZE),SEEK_SET);

		sectorTable[sectorNo] = maxBlock;
		secNoBuf[0] = (unsigned char)(sectorNo & 0xff);
		secNoBuf[1] = (unsigned char)((sectorNo >> 8) & 0xff);
		secNoBuf[2] = (unsigned char)((sectorNo >> 16) & 0xff);
		secNoBuf[3] = (unsigned char)((sectorNo >> 24) & 0xff);
		
		maxBlock++;
		
		/* �Z�N�^�ԍ��������������ށB */
		ioCount = fwrite(secNoBuf,4,1,fpo);
		if (ioCount < 1){
			return 1;
		}
		
	}else{
		/* �t�@�C����̈ʒu�ɒ����B */
		sectorPos = (long)(sectorTable[sectorNo] * (4 + SECTORSIZE));

		fseek(fpo,sectorPos + 4,SEEK_SET);
	}

	ioCount = fwrite(buf,SECTORSIZE,1,fpo);
	if (ioCount < 1){
		return 1;
	}
	return 0;
}
