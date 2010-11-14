#ifndef __FATFS_H
#define __FATFS_H

/* FAT12�̃G���g���̒l�𓾂�B */
unsigned long getFat12(unsigned long no);
/* FAT12�̃G���g���ɒl���������ށB */
void setFat12(unsigned long no,unsigned long value);
/* �󂫃N���X�^�ԍ��𓾂�B */
unsigned long getFreeCluster(void);
/* �󂫃N���X�^���𓾂�B */
unsigned long getFreeClusterCount(void);
/* �N���X�^��ǂݍ��� */
int readCluster(FILE *fpi,unsigned long clusterNo,void *buf);
/* �N���X�^���������� */
int writeCluster(FILE *fpi,unsigned long clusterNo,void *buf);
/* �t�@�C���ɑΉ�����FAT�G���g�����N���A����B */
void clearFatChain(unsigned long clusterNo);
/* �t�@�C��/�T�u�f�B���N�g���Ɏg���N���X�^�����W�v���� */
unsigned long countClusters(unsigned long clusterNo);

/* �f�B���N�g���G���g�� */
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

/* �f�B���N�g���G���g���̓��e�𓾂�B */
void getDirectory(struct dirEntry *dir,unsigned long no);
/* �f�B���N�g���G���g���̓��e���������ɃZ�b�g����B */
void setDirectory(struct dirEntry *dir,unsigned long no);

/* �f�B�X�N��� */
extern unsigned int FatSectors;	/* FAT�p�̃Z�N�^�� */
extern unsigned long MaxEntry;	/* �ő�G���g���� */
extern unsigned int SectorPerCluster;	/* �Z�N�^������̃N���X�^�� */
extern unsigned int DataArea;	/* �f�[�^�G���A�̊J�n�Z�N�^�ԍ� */

/* �e���Ǘ��p���[�N�G���A */
extern unsigned char *fat;	/* FAT�̓��e��u������ */
extern unsigned char *DirEntryBuf;	/* �f�B���N�g���G���g���̓��e��u������ */

#endif
