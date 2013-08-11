/*
SAVList (C) 2005 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	"savFile.h"
#include	"fatFs.h"

/* �f�B�X�N��� */
unsigned int FatSectors;	/* FAT�p�̃Z�N�^�� */
unsigned long MaxEntry;	/* �ő�G���g���� */
unsigned int SectorPerCluster;	/* �Z�N�^������̃N���X�^�� */
unsigned int DataArea;	/* �f�[�^�G���A�̊J�n�Z�N�^�ԍ� */

/* �e���Ǘ��p���[�N�G���A */
unsigned char *fat = NULL;	/* FAT�̓��e��u������ */
unsigned char *DirEntryBuf = NULL;	/* �f�B���N�g���G���g���̓��e��u������ */

/* FAT12�̃G���g���̒l�𓾂�B */
unsigned long getFat12(unsigned long no)
{
	unsigned long entry;
	unsigned long link;
	
	no = no & 0x0fffl;
	if (no & 1){
		entry = no / 2 * 3 + 1;
		link = fat[entry + 1];
		link = link << 4;
		link = link | ((fat[entry] >> 4) & 0x0f);
	}else{
		entry = no / 2 * 3;
		link = fat[entry + 1] & 0x0f;
		link = link << 8;
		link = link | fat[entry];
	}
	
	return link;
}

/* FAT12�̃G���g���ɒl���������ށB */
void setFat12(unsigned long no,unsigned long value)
{
	unsigned long entry;
	
	no = no & 0x0fffl;
	if (no & 1){
		entry = no / 2 * 3 + 1;
		
		fat[entry] = (unsigned char)( ((value & 0x0f) << 4) | (fat[entry] & 0x0f) );
		fat[entry + 1] = (unsigned char)(value >> 4);

	}else{
		entry = no / 2 * 3;

		fat[entry] = (unsigned char)(value & 0xff);
		fat[entry + 1] = (unsigned char)( (fat[entry + 1] & 0xf0) | ((value >> 8) & 0x0f) );

	}
}

/* �󂫃N���X�^�ԍ��𓾂�B */
unsigned long getFreeCluster(void)
{
	unsigned long i,link;
	
	for (i = 2;i < MaxEntry;i++){
		link = getFat12(i);
		if (link == 0){
			return i;
		}
	}
	return 0;
}

/* �󂫃N���X�^���𓾂�B */
unsigned long getFreeClusterCount(void)
{
	unsigned long i,link,count;
	
	count = 0;
	for (i = 2;i < MaxEntry;i++){
		link = getFat12(i);
		if (link == 0){
			count++;
		}
	}

	return count;
}

/* �f�B���N�g���G���g���̓��e�𓾂�B */
void getDirectory(struct dirEntry *dir,unsigned long no)
{
	unsigned char *p;
	int i;
	unsigned int temp;
	unsigned long size,stemp;

	p = DirEntryBuf + no * 32;
	
	for (i = 0;i < 8;i++){
		dir->filename[i] = *p;
		p++;
	}
	
	for (i = 0;i < 3;i++){
		dir->ext[i] = *p;
		p++;
	}
	dir->attr = (unsigned char)*p;
	p++;
	p += 10;
	
	temp = *(p+1);
	temp = temp << 8;
	temp = temp | *p;
	dir->time = temp;
	
	p += 2;

	temp = *(p+1);
	temp = temp << 8;
	temp = temp | *p;
	dir->date = temp;
	
	p += 2;

	temp = *(p+1);
	temp = temp << 8;
	temp = temp | *p;
	dir->cluster = temp;

	p += 2;
	
	size = 0;
	for (i = 0;i < 4;i++){
		size = size << 8;
		stemp = *(p + 3 - i);
		size = size | stemp;
	}
	dir->size = size;
}

/* �f�B���N�g���G���g���̓��e���������ɃZ�b�g����B */
void setDirectory(struct dirEntry *dir,unsigned long no)
{
	char *p;
	int i;
	unsigned long stemp;

	p = DirEntryBuf + no * 32;
	
	for (i = 0;i < 8;i++){
		*p = dir->filename[i];
		p++;
	}
	
	for (i = 0;i < 3;i++){
		*p = dir->ext[i];
		p++;
	}
	*p = (char)dir->attr;
	p++;
	p += 10;

	*p = (char)(dir->time & 0xff);
	p++;
	*p = (char)((dir->time >> 8) & 0xff);
	p++;

	*p = (char)(dir->date & 0xff);
	p++;
	*p = (char)((dir->date >> 8) & 0xff);
	p++;

	*p = (char)(dir->cluster & 0xff);
	p++;
	*p = (char)((dir->cluster >> 8) & 0xff);
	p++;

	stemp = dir->size;
	for (i = 0;i < 4;i++){
		*p = (char)(stemp & 0xff);
		stemp = stemp >> 8;
		p++;
	}
}

/* �N���X�^��ǂݍ��� */
int readCluster(FILE *fpi,unsigned long clusterNo,void *buf)
{
	int result;
	unsigned long i;
	
	for (i = 0;i < SectorPerCluster;i++){
		result = readSector(fpi,DataArea + (clusterNo - 2) * SectorPerCluster + i,(char *)buf + i * SECTORSIZE);
		if (result){
			return result;
		}
	}
	return 0;
}

/* �N���X�^���������� */
int writeCluster(FILE *fpi,unsigned long clusterNo,void *buf)
{
	int result;
	unsigned long i;

	for (i = 0;i < SectorPerCluster;i++){
		result = writeSector(fpi,DataArea + (clusterNo - 2) * SectorPerCluster + i,(char *)buf + i * SECTORSIZE);
		if (result){
			return result;
		}
	}
	return 0;
}

/* �t�@�C���ɑΉ�����FAT�G���g�����N���A����B */
void clearFatChain(unsigned long clusterNo)
{
	unsigned long nextCluster,currentCluster;
	
	currentCluster = clusterNo;
	nextCluster = clusterNo;
	
	do{
		/* ���̃N���X�^�ԍ����擾����B */
		nextCluster = getFat12(currentCluster);
		/* �N���X�^���󂫂ɂ���B */
		setFat12(currentCluster,0);
		/* ���݂̃N���X�^���X�V����B */
		currentCluster = nextCluster;
	}while(nextCluster < 0xff7l);
	
}

/* �w�肵���N���X�^�ԍ�����̃N���X�^�����W�v����B */
unsigned long countClusters(unsigned long clusterNo)
{
	unsigned long nextCluster,currentCluster;
	unsigned long count = 0;
	
	currentCluster = clusterNo;
	nextCluster = clusterNo;

	do{
		count++;
		/* ���̃N���X�^�ԍ����擾����B */
		nextCluster = getFat12(nextCluster);
	}while(nextCluster < 0xff7l);
	
	return count;
}

/* �e�X�g�p���C�� */
#if 0
int main(int argc,char *argv[])
{
	unsigned long val,i;
	
	setFat12(2,0x123);
	setFat12(3,0x456);
	
	if (getFat12(2) != 0x123){
		printf("Fail 2\n");
	}else{
		printf("Ok 2\n");
	}

	if (getFat12(3) != 0x456){
		printf("Fail 3\n");
	}else{
		printf("Ok 3\n");
	}
	
	printf("%lx\n",getFreeCluster());
	printf("%lx\n",getFreeClusterCount());
	
	for (i = 0;i < 9;i++){
		printf("%02x\n",(int)fat[i]);
	}
}
#endif
