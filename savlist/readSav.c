/*
SAVList (C) 2005 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>

#include	<windows.h>
#include	<commctrl.h>

#include	"savFile.h"
#include	"fatFs.h"
#include	"readSav.h"

static FILE *fpSav = NULL;
int *entryOnListView = NULL;
int listViewEntrys;
int directoryEntrys;	/* �f�B���N�g���G���g���� */
int isSubDirectory = 0;
unsigned long fs_startCluster;

/* ���z�t���b�s�[�t�@�C���ɏ��������߂��B */
int flushSavfile(void)
{
	int i,j;
	unsigned long now_cluster;
	unsigned long clusterSize;
	
	/* FAT�������� */
	for (i = 0;i < 2;i++){
		for (j = 0;j < 3;j++){
			writeSector(fpSav,1 + (i * 3) + j,fat + SECTORSIZE * j );
		}
	}

	if (!isSubDirectory){
		/* ���[�g�f�B���N�g���������� */
		for (i = 0;i < 7;i++){
			writeSector(fpSav,7 + i,DirEntryBuf + (SECTORSIZE * i));
		}
	}else{
		/* FAT�����ǂ��ăT�u�f�B���N�g�����������ށB */
		clusterSize = SECTORSIZE * SectorPerCluster;
		now_cluster = fs_startCluster;
		i = 0;
		do{
			writeCluster(fpSav,now_cluster,DirEntryBuf + clusterSize * i);
			i++;
			now_cluster = getFat12(now_cluster);
		}while(now_cluster < 0x0ff7l);
	}

	return 0;
}

/* ���z�t���b�s�[�t�@�C�����J���ď���ǂݍ��ށB */
int openSavfile(char *savFile)
{
	int i,result;
	
	fpSav = fopen(savFile,"r+b");
	if (fpSav == NULL){
		fprintf(stderr,"Can't open virtual floppy disk file.\n");
		return 1;
	}
	fprintf(stderr,"Initializing.\n");
	result = initialize(fpSav);
	if (result){
		fclose(fpSav);
		return 1;
	}

	fprintf(stderr,"Memory allocating.\n");
	fat = calloc(3,SECTORSIZE);
	if (fat == NULL){
		fprintf(stderr,"No enough memory.\n");
		fclose(fpSav);
		return 1;
	}
	DirEntryBuf = calloc(7,SECTORSIZE);
	if (DirEntryBuf == NULL){
		fprintf(stderr,"No enough memory.\n");
		free(fat);
		fclose(fpSav);
		return 1;
	}

	fprintf(stderr,"Reading disk information.\n");
	/* FAT�ǂݍ��� */
	for (i = 0;i < 3;i++){
		readSector(fpSav,1 + i,fat + (SECTORSIZE * i));
	}
	setFat12(0,0xff9);
	setFat12(1,0xfff);
	

	/* ���[�g�f�B���N�g���ǂݍ��� */
	for (i = 0;i < 7;i++){
		readSector(fpSav,7 + i,DirEntryBuf + (SECTORSIZE * i));
	}
	isSubDirectory = 0;

	return 0;
}

/* ���\�[�X���J������ */
void cleanup(void)
{
	if (DirEntryBuf != NULL){
		free(DirEntryBuf);
		DirEntryBuf = NULL;
	}
	if (fat != NULL){
		free(fat);
		fat = NULL;
	}
	if (fpSav != NULL){
		fclose(fpSav);
		fpSav = NULL;
	}
	if (entryOnListView != NULL){
		free(entryOnListView);
		entryOnListView = NULL;
	}
}

/**
 * �w�肳�ꂽ�t�@�C���̏����f�B���N�g���ɃZ�b�g����B
 * ������Windows�Ɉˑ�����B
 *
 * @param d �f�B���N�g�����
 * @filename �f�B���N�g�����\���̂ɐݒ肷��t�@�C����
 */
int setupDirectory(struct dirEntry *d,char *filename)
{
	HANDLE hFind;
	WIN32_FIND_DATA info;
	unsigned short time,date;
	int i;
	char name[_MAX_FNAME],ext[_MAX_EXT];
	FILETIME localFileTime;
	TIME_ZONE_INFORMATION timeZoneInfo;
	DWORD timezoneStatus;

	/* �t�@�C���̏��𓾂�B */
	hFind = FindFirstFile(filename,&info);
	if (hFind == INVALID_HANDLE_VALUE){
		fprintf(stderr,"File not found.\n");
		return 2;
	}
	FindClose(hFind);

	/* �f�B�X�N�̋󂫂��t�@�C���T�C�Y�̕����傫��������G���[�Ƃ��Ĕ�����B */
	if (info.nFileSizeHigh > 0){
		/* fprintf(stderr,"No disk space.\n"); */
		return 1;
	}

#if 0
	cl = getFreeClusterCount();
	imgSize = SECTORSIZE * SectorPerCluster * cl;
	if (imgSize < info.nFileSizeLow){
		/* fprintf(stderr,"No disk space.\n"); */
		return 1;
	}
#endif

	memset(d,0x00,sizeof(struct dirEntry));

	/* �t�@�C���̏����f�B���N�g���ɐݒ肵�Ă����B */
	/* �^�C���X�^���v */
	/* UTC���猻�݂̎����ɒ��� */

	/* ����������Ƃ��̏��� */
	FileTimeToLocalFileTime(&(info.ftLastWriteTime), &localFileTime);

	timezoneStatus = GetTimeZoneInformation(&timeZoneInfo);
	if (timezoneStatus == TIME_ZONE_ID_DAYLIGHT) {
		// Adjust localFileTime with daylight bias.
		// Because FileTimeToLocalFileTime treats summertime.

		// I don't understand summertime.
		// If adjustment is incorrect, please fix it.

		int bias;
		unsigned long long withBias;

		withBias = localFileTime.dwLowDateTime + localFileTime.dwHighDateTime << 32;

		bias = -(timeZoneInfo.DaylightBias);
		withBias -= (bias * 100 * 1000000000 * 60);
		localFileTime.dwHighDateTime = withBias >> 32;
		localFileTime.dwLowDateTime = withBias & 0xffffffff;
	}

	FileTimeToDosDateTime(&localFileTime,&date,&time);

	d->time = time;
	d->date = date;

	/* �t�@�C���T�C�Y */
	d->size = info.nFileSizeLow;
	for (i = 0;i < 8;i++){
		d->filename[i] = ' ';
	}
	for (i = 0;i < 3;i++){
		d->ext[i] = ' ';
	}

	/* �t�@�C���� */
	_splitpath(filename,NULL,NULL,name,ext);
	/* printf("%s %s\n",name,ext); */
	for (i = 0;i < 8;i++){
		if (name[i] != 0x00){
			d->filename[i] = toupper(name[i]);
		}else{
			break;
		}
	}
	for (i = 0;i < 3;i++){
		if (ext[i+1] != 0x00){
			d->ext[i] = toupper(ext[i+1]);
		}else{
			break;
		}
	}
	return 0;
}

/* �T�u�f�B���N�g���Ƀf�B���N�g���G���g����ǉ�����B */
int addDirectoryEntry(unsigned long startCluster)
{
	int i;
	unsigned long now_cluster,next_cluster;
	unsigned long newCluster;	/* �t��������N���X�^ */
	unsigned long clusterSize;
	unsigned long clusterCount;
	unsigned char *newDirectory;
	int *newEntryOnListView;

	newCluster = getFreeCluster();
	if (newCluster == 0){
		/* �t��������G���g�����Ȃ� */
		return 1;
	}

	/* FAT�����ǂ��ăT�u�f�B���N�g�����������ށB */
	clusterSize = SECTORSIZE * SectorPerCluster;
	/* �f�B���N�g���G���g���̍Ō�̃N���X�^���o���B */
	now_cluster = startCluster;
	next_cluster = startCluster;
	clusterCount = 0;
	i = 0;
	do{
		now_cluster = next_cluster;
		/* writeCluster(fpSav,now_cluster,DirEntryBuf + clusterSize * i); */
		i++;
		clusterCount++;
		next_cluster = getFat12(now_cluster);
	}while(next_cluster < 0x0ff7l);
	/* FAT���Ȃ���B */
	setFat12(now_cluster,newCluster);
	setFat12(newCluster,0xfffl);

	/* �g�債���T�C�Y�̃G���g�����̃��������m�ۂ���B */
	newDirectory = calloc(clusterSize * (clusterCount + 1),1);
	if (newDirectory == NULL){
		return 1;
	}
	/* �f�B���N�g���G���g����V�������̂ɕt���ւ���B */
	memcpy(newDirectory,DirEntryBuf,clusterSize * clusterCount);
	free(DirEntryBuf);
	DirEntryBuf = newDirectory;

	/* �f�B���N�g���ɓ���G���g�������X�V����B */
	directoryEntrys = (clusterSize * (clusterCount + 1)) / 32;

	/* �f�B���N�g���G���g�������ēx�m�ۂ��� */
	newEntryOnListView = (int *)malloc(sizeof(int) * directoryEntrys);
	if (newEntryOnListView == NULL){
		return 1;
	}
	entryOnListView = newEntryOnListView;

	return 0;
}

/* �f�B���N�g���G���g���ɋ󂫂����邩�ǂ����`�F�b�N����B */
int checkEmptyEntry(struct dirEntry *d)
{
	int i,empty;
	struct dirEntry d2;
	unsigned long countFree;
	unsigned long imgSize;

	/* ���z�t���b�s�[�f�B�X�N�t�@�C���Ƀf�B���N�g���G���g���̋󂫂����邩�H */
	empty = -1;
	for (i = 0;i < directoryEntrys;i++){
		getDirectory(&d2,i);
		if ((d2.filename[0] == 0x00) || (d2.filename[0] == (char)0xe5)){
			if (empty == -1){
				/* �܂��A���̋󂫂������Ă��Ȃ������炱�̏ꏊ�ɂ���B */
				empty = i;
				/* �ŏI�f�B���N�g���G���g���ȊO�ŃG���g���̃G���h�}�[�N�������� */
				/* ���̃G���g���ɃG���h�}�[�N������B */
				if ((d2.filename[0] == 0x00) && (i < (directoryEntrys - 1))){
					setDirectory(&d2,i+1);
				}
			}
			/* �G���h�}�[�N�������炱�̐�̃f�B���N�g���G���g���͖����Ȃ̂Ŕ�����B */
			if (d2.filename[0] == 0x00){
				break;
			}
		}
		/* �w�肳�ꂽ�t�@�C���������łɑ��݂�������S���l���ď������݂��s��Ȃ��悤�ɂ���B */
		if (memcmp(d->filename,d2.filename,8) == 0){
			if (memcmp(d->ext,d2.ext,3) == 0){
#ifdef NOOVERWRITE				
				fprintf(stderr,"File already exists.\n");
				return -1;
#else
				/* �󂭃N���X�^�����v�Z���ċ󂫂����邩�ǂ����𔻒肷��B */
				countFree = countClusters(d2.cluster);
				countFree += getFreeClusterCount();
				imgSize = SECTORSIZE * SectorPerCluster * countFree;
				if (imgSize < d->size){
					/* �t�@�C���T�C�Y�̕����傫���ꍇ�͔�����B */
					return -2;
				}

				/* ���̃G���g�����g�p����B */
				clearFatChain((unsigned long)(d2.cluster & 0x0fff));
				empty = i;
#endif
			}
		}
	}
	return empty;
}

/**
 * �t�@�C���̏������݂��s���B
 *
 * @param filename �������ރt�@�C����
 * @return �������݌��� 0:���� 0�ȊO:���s
 */
int writeFile(char *filename)
{
	struct dirEntry d;
	int result,empty;
	size_t read;
	unsigned long cl,next;
	char buf[1024];
	FILE *fp;
	unsigned long countFree;
	unsigned long imgSize;

	/* �f�B���N�g�����Z�b�g����Ă��Ȃ��ꍇ�A�������Ȃ��B */
	if (DirEntryBuf == NULL)
		return 1;
	if (entryOnListView == NULL)
		return 1;

	result = setupDirectory(&d,filename);
	if (result > 0){
		return 1;
	}

	/* �󂫃G���g�������邩�ǂ������ׂ� */
	empty = checkEmptyEntry(&d);

	if (empty == -2){
		/* ���łɋ󂫗e�ʂ��Ȃ��ꍇ�͔�����B */
		return NO_DISKSPACE;
	}
	if ((empty == -1) && (isSubDirectory)){
		/* �T�u�f�B���N�g���̏ꍇ�A�G���g�����g��ł��Ȃ����ǂ����m�F����B */
		result = addDirectoryEntry(fs_startCluster);
		if (result){
			/* �G���g�����g�傷�邱�Ƃ͂ł��Ȃ� */
			return NO_DISKSPACE;
		}
		/* �g�債���G���g������󂫂�T���B */
		empty = checkEmptyEntry(&d);
		if (empty < 0){
			return NO_ENTRY;
		}
	}

	/* �󂫃G���g�����Ȃ��ꍇ�͔����� */
	if (empty < 0){
		return NO_ENTRY;
	}

	/* �󂫃N���X�^�����v�Z���ċ󂫂����邩�ǂ����𔻒肷��B */
	countFree = getFreeClusterCount();
	imgSize = SECTORSIZE * SectorPerCluster * countFree;
	if (imgSize < d.size){
		/* �t�@�C���T�C�Y�̕����傫���ꍇ�͔�����B */
		return NO_DISKSPACE;
	}

	/* �f�B�X�N�ɏ������� */
	cl = getFreeCluster();	/* �ŏ��̋󂫃N���X�^�ԍ� */
	d.cluster = (unsigned short)cl;	/* �f�B���N�g���G���g���̊J�n�N���X�^ */

	fp = fopen(filename,"rb");
	if (fp == NULL){
		return 1;
	}

	/* �ŏ���1�߂̃N���X�^ */
	read = fread(buf,1,1024,fp);
	if (read != 0){
		/* �ŏ���1�N���X�^�ڂ̏������� */
		if (cl == 0){
			return NO_DISKSPACE;
		}

		writeCluster(fpSav,cl,buf);
		setFat12(cl,0xfff);	/* �ꉞ�A�ǂݍ��񂾎��_��FAT�𓯊����Ă��� */
		next = getFreeCluster();
		if (next == 0){
			return NO_DISKSPACE;
		}

		while(1){
			read = fread(buf,1,1024,fp);
			if (read == 0){
				/* ���x�N���X�^���P�ʂɎ��܂����Ƃ���FAT���q���Ȃ��B */
				break;
			}else{
				/* FAT�G���g�����q���B */
				setFat12(cl,next);
			}
			/* printf("."); */
			/* �ǂݍ��񂾓��e������N���X�^��ݒ肷��B */
			cl = next;
			writeCluster(fpSav,cl,buf);
			setFat12(cl,0xfff);	/* �ꉞ�A�ǂݍ��񂾎��_��FAT�𓯊����Ă��� */
			if (read == 1024){
				/* ���̃N���X�^�ւȂ��B */
				next = getFreeCluster();
				/* printf("Now:%lx Next:%lx\n",cl,next); */
			}else{
				/* �t�@�C����ǂ݂������ꍇ�͂����Ŕ�����B */
				setFat12(cl,0xfff);
				break;
			}
		}
	}else{
		/* 0�o�C�g�̃t�@�C���̏ꍇ�A�N���X�^�G���g����0�ɂȂ�B */
		d.cluster = (unsigned short)0;	/* �f�B���N�g���G���g���̊J�n�N���X�^ */
	}

	fclose(fp);
	/* �󂫃G���g���Ƀf�B���N�g���̏���ݒ肷��B */
	setDirectory(&d,empty);

	/* �t�@�C���������݂ɐ���������FAT�ƃ��[�g�f�B���N�g�����������ށB */
	flushSavfile();

	return 0;
}

/* �t�@�C����W�J���� */
int extractFile(char *dir,int listviewEntry)
{
	int entry,i;
	size_t len;
	char buf[_MAX_PATH];
	char *clusterBuf;
	struct dirEntry d;
	unsigned long size;
	unsigned long clusterSize;
	unsigned long clusterNo;
	FILE *fpo;

	/* �f�B���N�g�����Z�b�g����Ă��Ȃ��ꍇ�A�������Ȃ��B */
	if (DirEntryBuf == NULL)
		return 1;
	if (entryOnListView == NULL)
		return 1;

	/* �f�B���N�g���G���g���擾 */
	entry = entryOnListView[listviewEntry];
	getDirectory(&d,entry);

	/* �p�X������ */
	strcpy(buf,dir);
	len = strlen(dir);
	if (dir[len - 1] != '\\'){
		strcat(buf,"\\");
		len++;
	}
	for (i = 0;i < 8;i++){
		if (d.filename[i] == ' '){
			break;
		}
		buf[len] = d.filename[i]; 
		len++;
	}
	if (d.ext[0] != ' '){
		buf[len] = '.'; 
		len++;
		for (i = 0;i < 3;i++){
			if (d.ext[i] == ' '){
				break;
			}
			buf[len] = d.ext[i]; 
			len++;
		}
	}
	buf[len] = '\0';
	len++;

	size = d.size;
	clusterSize = SECTORSIZE * SectorPerCluster;
	clusterNo = d.cluster;
	clusterBuf = malloc((size_t)clusterSize);
	if (clusterBuf == NULL){
		return 1;
	}

	fpo = fopen(buf,"wb");
	if (fpo == NULL){
		free(clusterBuf);
		return 1;
	}
	while(size >= clusterSize){
		readCluster(fpSav,clusterNo,clusterBuf);
		fwrite(clusterBuf,1,clusterSize,fpo);

		clusterNo = getFat12(clusterNo);
		size = size - clusterSize;
		if (clusterNo >= 0x00000ff7){
			break;
		}
	}
	if (size > 0){
		readCluster(fpSav,clusterNo,clusterBuf);
		fwrite(clusterBuf,1,size,fpo);
	}
	fclose(fpo);
	free(clusterBuf);

	/* �^�C���X�^���v��ݒ肷��B */
	setTimeStamp(&d, buf);

	return 0;
}

/**
 * �^�C���X�^���v���f�B�X�N�C���[�W�̃t�@�C������ݒ肷��B
 *
 * @param d �f�B���N�g���G���g��
 * @param filename �ݒ�Ώۃt�@�C��
 */
void setTimeStamp(struct dirEntry *d, char *filename)
{
	HANDLE hFile;
	FILETIME fileTime;
	SYSTEMTIME sysTime;
	FILETIME localFileTime;

	memset(&sysTime, 0,sizeof(SYSTEMTIME));

	hFile = CreateFile(filename,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}
	DosDateTimeToFileTime(d->date, d->time, &fileTime);
	
	LocalFileTimeToFileTime(&fileTime, &localFileTime);

	SetFileTime(hFile, NULL, NULL, &localFileTime);
	CloseHandle(hFile);

}

/* �f�B���N�g����ǂݍ��� */
void readDirectory(unsigned long startCluster)
{
	unsigned long i;
	unsigned long clusterCount;
	unsigned long clusterPos;
	unsigned long nextCluster;
	unsigned long clusterSize;

	clusterSize = SECTORSIZE * SectorPerCluster;

	if (DirEntryBuf != NULL){
		free(DirEntryBuf);
		DirEntryBuf = NULL;
	}
	if (entryOnListView != NULL){
		free(entryOnListView);
		entryOnListView = NULL;
	}

	if (startCluster == 0){
		directoryEntrys = SECTORSIZE * 7 / 32;	/* �f�B���N�g���G���g���� */

		DirEntryBuf = malloc((size_t)(SECTORSIZE * 7));	/* �f�B���N�g���̕������ēx�m�ۂ���B */
		if (DirEntryBuf == NULL){
			return;
		}

		/* �f�B���N�g���G���g�������ēx�m�ۂ��� */
		entryOnListView = (int *)malloc(sizeof(int) * directoryEntrys);
		if (entryOnListView == NULL){
			return;
		}

		/* ���[�g�f�B���N�g����ǂݍ��� */
		for (i = 0;i < 7;i++){
			readSector(fpSav,7 + i,DirEntryBuf + (SECTORSIZE * i));
		}
		isSubDirectory = 0;
	}else{
		clusterCount = countClusters(startCluster);	/* �f�B���N�g���Ɋ��蓖�Ă�ꂽ�N���X�^�� */
		directoryEntrys = clusterCount * clusterSize / 32;	/* �f�B���N�g���G���g���� */

		DirEntryBuf = malloc((size_t)(clusterSize * clusterCount));	/* �f�B���N�g���̕������ēx�m�ۂ���B */
		if (DirEntryBuf == NULL){
			return;
		}

		/* �f�B���N�g���G���g�������ēx�m�ۂ��� */
		entryOnListView = (int *)malloc(sizeof(int) * directoryEntrys);
		if (entryOnListView == NULL){
			return;
		}

		nextCluster = startCluster;
		clusterPos = 0;
		do {
			readCluster(fpSav,nextCluster,DirEntryBuf + clusterSize * clusterPos);
			clusterPos++;
			nextCluster = getFat12(nextCluster);
		}while(nextCluster < 0x0ff7l);
		isSubDirectory = 1;
	}
}

/**
 * �f�B���N�g���̏��(�t�@�C�����E�f�B���N�g����)���Z�o����B
 */
void getSavStatus(struct SavStatus *status)
{
	int i;
	unsigned long clusterSize;
	unsigned long files = 0,directorys = 0;
	struct dirEntry d2;

	/* �󂫗e�ʌv�Z */
	clusterSize = SECTORSIZE * SectorPerCluster;
	status->freeSize = getFreeClusterCount() * clusterSize;
	status->maxSize = MaxEntry * clusterSize;


	/* ���z�t���b�s�[�f�B�X�N�t�@�C���Ƀf�B���N�g���G���g���̋󂫂����邩�H */
	for (i = 0;i < directoryEntrys;i++){
		getDirectory(&d2,i);
		if (d2.filename[0] == 0x00){
			break;
		}
		if ((unsigned char)(d2.filename[0]) == 0xe5){
			continue;
		}
		if (d2.attr & 0x10){
			directorys++;
		}else{
			if (d2.attr & 0x08){
				/* �{�����[�����x���������O�t�@�C���� */
			}else{
				files++;
			}
		}
	}

	status->directorys = directorys;
	status->files = files;
}

/* SAV�t�@�C����ǂݍ��܂��� */
int readSavFile(char *filename)
{
	int result;
	
	/* �f�B�X�N�̏���ݒ肷��B */
	FatSectors = 3;	/* FAT�p�̃Z�N�^�� */
	MaxEntry = (SECTORS - 14) / 2 + 2;	/* �ő�FAT�G���g���� */
	SectorPerCluster = 2;	/* 1�N���X�^���\������Z�N�^�� */
	DataArea = 14;	/* �f�[�^�G���A�J�n�Z�N�^ */
	directoryEntrys = 112;	/* ���݂̃f�B���N�g���̃G���g���� */
	isSubDirectory = 0;	/* ���[�g�f�B���N�g�� */

	result = openSavfile(filename);
	if (result > 0){
		return 2;
	}

	entryOnListView = (int *)malloc(sizeof(int) * directoryEntrys);
	if (entryOnListView == NULL){
		return 2;
	}

	return 0;
}

/*-----------------------*/
/* ���̐��GUI�A�g�p�֐� */
/*-----------------------*/

/* �f�B���N�g�����ĕ\������ */
void refreshDir(HWND hList)
{
	int i,pos;
	LV_ITEM item;
	char buf[32];
	struct dirEntry d;
	int listCount = 0;	/* ���X�g��ɓ����Ă���G���g���� */

	if (DirEntryBuf == NULL)
		return;
	if (entryOnListView == NULL)
		return;

	memset(&item,0,sizeof(LV_ITEM));
	ListView_DeleteAllItems(hList);

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		item.mask = LVIF_TEXT;

		/* �f�B���N�g���G���g�����擾����B */
		getDirectory(&d,i);

		/* �f�B���N�g���G���g���̏I���̏ꍇ�\����ł��؂�B */
		if (d.filename[0] == 0x00){
			break;
		}

		/* �폜���ꂽ�G���g���͕\�����Ȃ��B */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		/* �����O�t�@�C�����͕\�����Ȃ� */
		if (d.attr == 0x0f){
			continue;
		}

		/* �f�B���N�g���G���g����̈ʒu�ƃ��X�g�r���[��̈ʒu���֘A�t���� */
		entryOnListView[listCount] = i;
		listCount++;

		/* �f�B���N�g���G���g����̈ʒu */
		sprintf(buf,"%d",i);
		item.pszText = buf;
		item.iItem = pos;
		item.iSubItem = 0;
		ListView_InsertItem(hList,&item);

		/* �t�@�C���� */
		memset(buf,0x00,32);
		memcpy(buf,d.filename,8);

		item.pszText = buf;
		item.iItem = pos;
		item.iSubItem = 1;
		ListView_SetItem(hList,&item);

		memset(buf,0x00,32);
		if (d.ext[0] != ' '){
			buf[0] = '.';
			memcpy(buf + 1,d.ext,3);
		} else {
			strcpy(buf,"    ");
		}
		item.pszText = buf;
		item.iItem = pos;
		item.iSubItem = 2;
		ListView_SetItem(hList,&item);


		/* �t�@�C���T�C�Y */
		if (d.attr & 0x10){
			sprintf(buf,"<DIR>");
		}else if (d.attr & 0x08){
			sprintf(buf,"<VOL>");
		}else{
			memset(buf,0x00,32);
			sprintf(buf,"%lu",d.size);
		}

		item.pszText = buf;
		item.iItem = pos;
		item.iSubItem = 3;
		ListView_SetItem(hList,&item);

		/* �X�V���� */
		sprintf(buf,"%04d/%02d/%02d %02d:%02d:%02d",
			((d.date >> 9) & 0x7f) + 1980,
			(d.date >> 5) & 0x0f,
			(d.date) & 0x1f,
			(d.time >> 11) & 0x1f,
			(d.time >> 5) & 0x3f,
			(d.time & 0x1f) << 1);

		item.pszText = buf;
		item.iItem = pos;
		item.iSubItem = 4;
		ListView_SetItem(hList,&item);

		pos++;
	}

	listViewEntrys = listCount;
}

/* �t�@�C�������ׂđI������ */
void selectAllFiles(HWND hList)
{
	int i,pos;
	struct dirEntry d;
	int listCount = 0;	/* ���X�g��ɓ����Ă���G���g���� */

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		/* �f�B���N�g���G���g�����擾����B */
		getDirectory(&d,i);

		/* �f�B���N�g���G���g���̏I���̏ꍇ�\����ł��؂�B */
		if (d.filename[0] == 0x00){
			break;
		}

		/* �폜���ꂽ�G���g���͕\�����Ȃ��B */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		/* �����O�t�@�C�����͕\�����Ȃ� */
		if (d.attr == 0x0f){
			continue;
		}

		if (d.attr & 0x10){
			/* �f�B���N�g�� */
			ListView_SetItemState(hList,listCount,0,LVIS_SELECTED);
		}else if (d.attr & 0x08) {
			/* �{�����[�����x�� */
			ListView_SetItemState(hList,listCount,0,LVIS_SELECTED);
		}else{
			ListView_SetItemState(hList,listCount,LVIS_SELECTED,LVIS_SELECTED);
		}

		/* �f�B���N�g���G���g����̈ʒu�ƃ��X�g�r���[��̈ʒu���֘A�t���� */
		listCount++;
	}

}

/* �t�@�C�������ׂđI������ */
int upDirectory(HWND hList)
{
	int i,pos;
	struct dirEntry d;
	int listCount = 0;	/* ���X�g��ɓ����Ă���G���g���� */

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		/* �f�B���N�g���G���g�����擾����B */
		getDirectory(&d,i);

		/* �f�B���N�g���G���g���̏I���̏ꍇ������ł��؂�B */
		if (d.filename[0] == 0x00){
			break;
		}

		/* �폜���ꂽ�G���g���͔�΂��B */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		/* �����O�t�@�C�����͔�΂� */
		if (d.attr == 0x0f){
			continue;
		}

		if (d.attr & 0x10){
			/* �f�B���N�g�� */
			if (d.filename[0] == '.' &&
				d.filename[1] == '.' &&
				d.filename[2] == ' '){
				enterDirectory(hList,i);
				return 1;
			}
		}

		/* �f�B���N�g���G���g����̈ʒu�ƃ��X�g�r���[��̈ʒu���֘A�t���� */
		listCount++;
	}
	return 0;
}

/* �T�u�f�B���N�g���̒��ɓ��� */
void enterDirectory(HWND hList,int listviewEntry)
{
	int entry;
	struct dirEntry d;

	if (DirEntryBuf == NULL)
		return;
	if (entryOnListView == NULL)
		return;

	/* �f�B���N�g���G���g���擾 */
	entry = entryOnListView[listviewEntry];
	getDirectory(&d,entry);

	if (d.attr & 0x10){
		if (d.filename[0] == '.' && d.filename[1] == ' '){
			/* .(�����̃f�B���N�g��)�̏ꍇ�������Ȃ� */
		}else{
			/* �f�B���N�g����ǂݍ��� */
			readDirectory(d.cluster);
			fs_startCluster = d.cluster;
			/* �f�B���N�g���̓��e�����X�g�r���[�ɓ]������ */
			refreshDir(hList);
		}
	}

}

/* �I�������t�@�C�����폜���� */
void deleteSelectedFiles(HWND hList)
{
	int i,entry;
	struct dirEntry d;
	UINT state;

	for (i = listViewEntrys - 1;i > -1;i--){
		state = ListView_GetItemState(hList,i,LVIS_SELECTED);
		if (!state){
			continue;
		}
		entry = entryOnListView[i];

		/* �f�B���N�g���G���g�����擾����B */
		getDirectory(&d,entry);

		if (d.attr & 0x10){
			/* �f�B���N�g�� */
			continue;
		}else if (d.attr & 0x08) {
			/* �{�����[�����x�� */
			continue;
		}else{
			d.filename[0] = 0xe5;
			if (d.cluster != 0){
				clearFatChain(d.cluster);
			}
			setDirectory(&d,entry);
			ListView_DeleteItem(hList, i);
		}

	}

	/* �t�@�C���폜�ɐ���������FAT�ƃf�B���N�g�����������ށB */
	flushSavfile();
}

/**
 * �����O�t�@�C�������폜����B 
 */
void deleteLongFilename(HWND hList)
{
	int i,pos;
	struct dirEntry d;
	int listCount = 0;	/* ���X�g��ɓ����Ă���G���g���� */

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		/* �f�B���N�g���G���g�����擾����B */
		getDirectory(&d,i);

		/* �f�B���N�g���G���g���̏I���̏ꍇ�\����ł��؂�B */
		if (d.filename[0] == 0x00){
			break;
		}

		/* �폜���ꂽ�G���g���͕\�����Ȃ��B */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		if ((d.attr & 0x0f) == 0x0f){
			d.filename[0] = 0xe5;
			setDirectory(&d,i);
		}
	}
	/* �t�@�C���폜�ɐ���������FAT�ƃf�B���N�g�����������ށB */
	flushSavfile();

}

