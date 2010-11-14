#ifndef __SAVFILE_H
#define __SAVFILE_H

#define SECTORS 1440
#define SECTORSIZE 512

/* �Z�N�^�ԍ��e�[�u���ɉ��z�t���b�s�[�t�@�C���̓��e��ǂݍ��ށB */
int initialize(FILE *fpi);
/* ���z�t���b�s�[�f�B�X�N�t�@�C���̃Z�N�^��ǂݍ��� */
int readSector(FILE *fpi,unsigned long sectorNo,void *buf);
/* ���z�t���b�s�[�f�B�X�N�t�@�C���̃Z�N�^�ɏ������� */
int writeSector(FILE *fpo,unsigned long sectorNo,void *buf);

#endif
