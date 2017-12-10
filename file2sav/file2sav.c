/*
file2sav (C) 2005,2017 Tatsuhiko Shoji
The sources for file2sav are distributed under the MIT open source license
*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<windows.h>
#include	<ctype.h>

#include	"savFile.h"
#include	"fatFs.h"

#define NO_DISKSPACE 2
#define NO_ENTRY 3

static FILE *fpSav;
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
}

/* �w�肳�ꂽ�t�@�C���̏����f�B���N�g���ɃZ�b�g����B */
/* ����������Windows�Ɉˑ�����B */
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
		fprintf(stderr,"No disk space.\n");
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
	FileTimeToLocalFileTime(&(info.ftLastWriteTime), &localFileTime);

	/* On daylight saving time season, FileTimeToLocalFileTime applies
	   unneeded daylight bias.

	*/
	timezoneStatus = GetTimeZoneInformation(&timeZoneInfo);
	if (timezoneStatus == TIME_ZONE_ID_DAYLIGHT) {
		// Adjust a localFileTime daylight bias.
		// Because FileTimeToLocalFileTime treats daylight saving time.

		// I don't understand summertime.
		// If adjustment is incorrect, please fix it.

		long long bias;
		unsigned long long withBias;

		withBias = localFileTime.dwHighDateTime;
		withBias = withBias << 32;
		withBias += localFileTime.dwLowDateTime;

		bias = timeZoneInfo.DaylightBias;
		
		withBias += (bias * 60 * 10000000);
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

/* �t�@�C���̏������݂��s���B */
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

/* SAV�t�@�C����ǂݍ��܂��� */
int readSavFile(char *filename)
{
	int result;
	
	/* �f�B�X�N�̏���ݒ肷��B */
	FatSectors = 3;	/* FAT�p�̃Z�N�^�� */
#if 0
	MaxEntry = SECTORSIZE * FatSectors / 3 * 2;	/* �ő�FAT�G���g���� */
#endif
	MaxEntry = (SECTORS - 14) / 2 + 2;
	SectorPerCluster = 2;	/* 1�N���X�^���\������Z�N�^�� */
	DataArea = 14;	/* �f�[�^�G���A�J�n�Z�N�^ */
	directoryEntrys = 112;	/* ���݂̃f�B���N�g���̃G���g���� */
	isSubDirectory = 0;	/* ���[�g�f�B���N�g�� */

	result = openSavfile(filename);
	if (result > 0){
		return 2;
	}

	return 0;
}

/*
	���X�|���X�t�@�C������t�@�C������ǂݍ���ŏ������݃��[�`���ɓn���B
*/
int parseResponseFile(char *filename)
{
	char buf[1024],*p;
	FILE *fp;
	int result;

	fp = fopen(filename + 1,"r");
	if (fp == NULL){
		return 1;
	}
	while(fgets(buf,1024,fp) != NULL){
		/* ���s�͕s�v�Ȃ̂Ŏ�苎��B */
		p = buf;
		while(*p){
			if (*p == '\n'){
				*p = '\0';
			}
			p++;
		}
		fprintf(stderr,"Transfering %s ...\n",buf);
		result = writeFile(buf);
	}
	fclose(fp);
	return 0;
}

/*
	���C���h�J�[�h�Ƀ}�b�`����t�@�C��������������
	�]�����s���B
*/
int parseWildcard(char *filename)
{
	HANDLE hFind;
	WIN32_FIND_DATA info;
	int result;
	char drive[_MAX_DRIVE],dir[_MAX_DIR];
	char buf[_MAX_PATH];

	memset(drive,0x00,_MAX_DRIVE);
	memset(dir,0x00,_MAX_DIR);
	_splitpath(filename,drive,dir,NULL,NULL);

	/* �t�@�C���̏��𓾂�B */
	hFind = FindFirstFile(filename,&info);
	if (hFind == INVALID_HANDLE_VALUE){
		fprintf(stderr,"File not found.\n");
		return 1;
	}
	
	/* �t�@�C�������݂��镪�����]�����s���B */
	do{
		if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
			/* �f�B���N�g���łȂ��ꍇ�����]�����s���B */
			sprintf(buf,"%s%s%s",drive,dir,info.cFileName);
			fprintf(stderr,"Transfering %s ...\n",buf);
			result = writeFile(buf);
			if (result){
				if (result == NO_DISKSPACE){
					fprintf(stderr,"No disk space.\n");
				}else if (result == NO_ENTRY){
					fprintf(stderr,"No directory entry.\n");
				}else{
					fprintf(stderr,"Error.\n");
				}
				FindClose(hFind);
				return 1;
			}
		}
	}while(FindNextFile(hFind,&info));
	
	FindClose(hFind);
	return 0;
}

/*
	�t�@�C��������
*/
int parseFilename(char *filename)
{
	int result,isWildcard;
	char *p;
	
	if (*filename == '@'){
		/* ���X�|���X�t�@�C�� */
		result = parseResponseFile(filename);
	}else{
		p = filename;
		isWildcard = 0;
		while(*p){
			if ((*p == '*') || (*p == '?')){
				isWildcard = 1;
			}
			p++;
		}
		if (isWildcard){
			/* ���C���h�J�[�h���������ꍇ�̓��C���h�J�[�h���߃��[�`���� */
			result = parseWildcard(filename);
		}else{
			fprintf(stderr,"Transfering %s ...\n",filename);
			result = writeFile(filename);
			if (result){
				if (result == NO_DISKSPACE){
					fprintf(stderr,"No disk space.\n");
				}else if (result == NO_ENTRY){
					fprintf(stderr,"No directory entry.\n");
				}else{
					fprintf(stderr,"Error.\n");
				}
			}
		}
	}
	return result;
}

int main(int argc,char *argv[])
{
	int result,i;
	
	if (argc < 3){
		fprintf(stderr,"file2sav Version 1.03\nBy Tatsuhiko Syoji(Tatsu) 2003-2005,2017\n\nUsage:file2sav sav_file transfer_file ...\n");
		return 1;
	}

	/* �f�B�X�N�̏���ݒ肷��B */
	FatSectors = 3;
	MaxEntry = SECTORSIZE * FatSectors / 3 * 2;
	SectorPerCluster = 2;
	DataArea = 14;

	result = readSavFile(argv[1]);
	if (result > 0){
		return 2;
	}
	fprintf(stderr,"Convert start.\n");

	/* �t�@�C�����̕������]������B */
	for (i = 2;i < argc;i++){
		/* �t�@�C�������߃��[�`���ɓn���B */
		result = parseFilename(argv[i]);
		if (result){
			break;
		}
	}

	cleanup();
	if (result){
		return 2;
	}

	return 0;
}
