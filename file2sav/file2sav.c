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
int directoryEntrys;	/* ディレクトリエントリ数 */
int isSubDirectory = 0;
unsigned long fs_startCluster;

/* 仮想フロッピーファイルに情報を書き戻す。 */
int flushSavfile(void)
{
	int i,j;
	unsigned long now_cluster;
	unsigned long clusterSize;
	
	/* FAT書き込み */
	for (i = 0;i < 2;i++){
		for (j = 0;j < 3;j++){
			writeSector(fpSav,1 + (i * 3) + j,fat + SECTORSIZE * j );
		}
	}

	if (!isSubDirectory){
		/* ルートディレクトリ書き込み */
		for (i = 0;i < 7;i++){
			writeSector(fpSav,7 + i,DirEntryBuf + (SECTORSIZE * i));
		}
	}else{
		/* FATをたどってサブディレクトリを書き込む。 */
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

/* 仮想フロッピーファイルを開いて情報を読み込む。 */
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
	/* FAT読み込み */
	for (i = 0;i < 3;i++){
		readSector(fpSav,1 + i,fat + (SECTORSIZE * i));
	}
	setFat12(0,0xff9);
	setFat12(1,0xfff);
	

	/* ルートディレクトリ読み込み */
	for (i = 0;i < 7;i++){
		readSector(fpSav,7 + i,DirEntryBuf + (SECTORSIZE * i));
	}
	isSubDirectory = 0;

	return 0;
}

/* リソースを開放する */
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

/* 指定されたファイルの情報をディレクトリにセットする。 */
/* ここだけはWindowsに依存する。 */
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

	/* ファイルの情報を得る。 */
	hFind = FindFirstFile(filename,&info);
	if (hFind == INVALID_HANDLE_VALUE){
		fprintf(stderr,"File not found.\n");
		return 2;
	}
	FindClose(hFind);

	/* ディスクの空きよりファイルサイズの方が大きかったらエラーとして抜ける。 */
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

	/* ファイルの情報をディレクトリに設定していく。 */
	/* タイムスタンプ */
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

	/* ファイルサイズ */
	d->size = info.nFileSizeLow;
	for (i = 0;i < 8;i++){
		d->filename[i] = ' ';
	}
	for (i = 0;i < 3;i++){
		d->ext[i] = ' ';
	}

	/* ファイル名 */
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

/* サブディレクトリにディレクトリエントリを追加する。 */
int addDirectoryEntry(unsigned long startCluster)
{
	int i;
	unsigned long now_cluster,next_cluster;
	unsigned long newCluster;	/* 付け加えるクラスタ */
	unsigned long clusterSize;
	unsigned long clusterCount;
	unsigned char *newDirectory;

	newCluster = getFreeCluster();
	if (newCluster == 0){
		/* 付け加えるエントリがない */
		return 1;
	}

	/* FATをたどってサブディレクトリを書き込む。 */
	clusterSize = SECTORSIZE * SectorPerCluster;
	/* ディレクトリエントリの最後のクラスタを出す。 */
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
	/* FATをつなげる。 */
	setFat12(now_cluster,newCluster);
	setFat12(newCluster,0xfffl);

	/* 拡大したサイズのエントリ分のメモリを確保する。 */
	newDirectory = calloc(clusterSize * (clusterCount + 1),1);
	if (newDirectory == NULL){
		return 1;
	}
	/* ディレクトリエントリを新しいものに付け替える。 */
	memcpy(newDirectory,DirEntryBuf,clusterSize * clusterCount);
	free(DirEntryBuf);
	DirEntryBuf = newDirectory;

	/* ディレクトリに入るエントリ数を更新する。 */
	directoryEntrys = (clusterSize * (clusterCount + 1)) / 32;

	return 0;
}

/* ディレクトリエントリに空きがあるかどうかチェックする。 */
int checkEmptyEntry(struct dirEntry *d)
{
	int i,empty;
	struct dirEntry d2;
	unsigned long countFree;
	unsigned long imgSize;

	/* 仮想フロッピーディスクファイルにディレクトリエントリの空きがあるか？ */
	empty = -1;
	for (i = 0;i < directoryEntrys;i++){
		getDirectory(&d2,i);
		if ((d2.filename[0] == 0x00) || (d2.filename[0] == (char)0xe5)){
			if (empty == -1){
				/* まだ、他の空きを見つけていなかったらこの場所にする。 */
				empty = i;
				/* 最終ディレクトリエントリ以外でエントリのエンドマークだったら */
				/* 次のエントリにエンドマークをつける。 */
				if ((d2.filename[0] == 0x00) && (i < (directoryEntrys - 1))){
					setDirectory(&d2,i+1);
				}
			}
			/* エンドマークだったらこの先のディレクトリエントリは無効なので抜ける。 */
			if (d2.filename[0] == 0x00){
				break;
			}
		}
		/* 指定されたファイル名がすでに存在したら安全を考えて書き込みを行わないようにする。 */
		if (memcmp(d->filename,d2.filename,8) == 0){
			if (memcmp(d->ext,d2.ext,3) == 0){
#ifdef NOOVERWRITE				
				fprintf(stderr,"File already exists.\n");
				return -1;
#else
				/* 空くクラスタ数を計算して空きがあるかどうかを判定する。 */
				countFree = countClusters(d2.cluster);
				countFree += getFreeClusterCount();
				imgSize = SECTORSIZE * SectorPerCluster * countFree;
				if (imgSize < d->size){
					/* ファイルサイズの方が大きい場合は抜ける。 */
					return -2;
				}

				/* このエントリを使用する。 */
				clearFatChain((unsigned long)(d2.cluster & 0x0fff));
				empty = i;
#endif
			}
		}
	}
	return empty;
}

/* ファイルの書き込みを行う。 */
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

	/* ディレクトリがセットされていない場合、何もしない。 */
	if (DirEntryBuf == NULL)
		return 1;

	result = setupDirectory(&d,filename);
	if (result > 0){
		return 1;
	}

	/* 空きエントリがあるかどうか調べる */
	empty = checkEmptyEntry(&d);

	if (empty == -2){
		/* すでに空き容量がない場合は抜ける。 */
		return NO_DISKSPACE;
	}
	if ((empty == -1) && (isSubDirectory)){
		/* サブディレクトリの場合、エントリを拡大できないかどうか確認する。 */
		result = addDirectoryEntry(fs_startCluster);
		if (result){
			/* エントリを拡大することはできない */
			return NO_DISKSPACE;
		}
		/* 拡大したエントリから空きを探す。 */
		empty = checkEmptyEntry(&d);
		if (empty < 0){
			return NO_ENTRY;
		}
	}

	/* 空きエントリがない場合は抜ける */
	if (empty < 0){
		return NO_ENTRY;
	}

	/* 空きクラスタ数を計算して空きがあるかどうかを判定する。 */
	countFree = getFreeClusterCount();
	imgSize = SECTORSIZE * SectorPerCluster * countFree;
	if (imgSize < d.size){
		/* ファイルサイズの方が大きい場合は抜ける。 */
		return NO_DISKSPACE;
	}

	/* ディスクに書き込む */
	cl = getFreeCluster();	/* 最初の空きクラスタ番号 */
	d.cluster = (unsigned short)cl;	/* ディレクトリエントリの開始クラスタ */

	fp = fopen(filename,"rb");
	if (fp == NULL){
		return 1;
	}

	/* 最初の1つめのクラスタ */
	read = fread(buf,1,1024,fp);
	if (read != 0){
		/* 最初の1クラスタ目の書き込み */
		if (cl == 0){
			return NO_DISKSPACE;
		}

		writeCluster(fpSav,cl,buf);
		setFat12(cl,0xfff);	/* 一応、読み込んだ時点でFATを同期しておく */
		next = getFreeCluster();
		if (next == 0){
			return NO_DISKSPACE;
		}

		while(1){
			read = fread(buf,1,1024,fp);
			if (read == 0){
				/* 丁度クラスタ数単位に収まったときはFATを繋がない。 */
				break;
			}else{
				/* FATエントリを繋ぐ。 */
				setFat12(cl,next);
			}
			/* printf("."); */
			/* 読み込んだ内容を入れるクラスタを設定する。 */
			cl = next;
			writeCluster(fpSav,cl,buf);
			setFat12(cl,0xfff);	/* 一応、読み込んだ時点でFATを同期しておく */
			if (read == 1024){
				/* 次のクラスタへつなぐ。 */
				next = getFreeCluster();
				/* printf("Now:%lx Next:%lx\n",cl,next); */
			}else{
				/* ファイルを読みきった場合はここで抜ける。 */
				setFat12(cl,0xfff);
				break;
			}
		}
	}else{
		/* 0バイトのファイルの場合、クラスタエントリは0になる。 */
		d.cluster = (unsigned short)0;	/* ディレクトリエントリの開始クラスタ */
	}

	fclose(fp);
	/* 空きエントリにディレクトリの情報を設定する。 */
	setDirectory(&d,empty);

	/* ファイル書き込みに成功したらFATとルートディレクトリを書き込む。 */
	flushSavfile();

	return 0;
}

/* SAVファイルを読み込ませる */
int readSavFile(char *filename)
{
	int result;
	
	/* ディスクの情報を設定する。 */
	FatSectors = 3;	/* FAT用のセクタ数 */
#if 0
	MaxEntry = SECTORSIZE * FatSectors / 3 * 2;	/* 最大FATエントリ数 */
#endif
	MaxEntry = (SECTORS - 14) / 2 + 2;
	SectorPerCluster = 2;	/* 1クラスタを構成するセクタ数 */
	DataArea = 14;	/* データエリア開始セクタ */
	directoryEntrys = 112;	/* 現在のディレクトリのエントリ数 */
	isSubDirectory = 0;	/* ルートディレクトリ */

	result = openSavfile(filename);
	if (result > 0){
		return 2;
	}

	return 0;
}

/*
	レスポンスファイルからファイル名を読み込んで書き込みルーチンに渡す。
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
		/* 改行は不要なので取り去る。 */
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
	ワイルドカードにマッチするファイル名を検索して
	転送を行う。
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

	/* ファイルの情報を得る。 */
	hFind = FindFirstFile(filename,&info);
	if (hFind == INVALID_HANDLE_VALUE){
		fprintf(stderr,"File not found.\n");
		return 1;
	}
	
	/* ファイルが存在する分だけ転送を行う。 */
	do{
		if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
			/* ディレクトリでない場合だけ転送を行う。 */
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
	ファイル名解釈
*/
int parseFilename(char *filename)
{
	int result,isWildcard;
	char *p;
	
	if (*filename == '@'){
		/* レスポンスファイル */
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
			/* ワイルドカードがあった場合はワイルドカード解釈ルーチンへ */
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

	/* ディスクの情報を設定する。 */
	FatSectors = 3;
	MaxEntry = SECTORSIZE * FatSectors / 3 * 2;
	SectorPerCluster = 2;
	DataArea = 14;

	result = readSavFile(argv[1]);
	if (result > 0){
		return 2;
	}
	fprintf(stderr,"Convert start.\n");

	/* ファイル数の分だけ転送する。 */
	for (i = 2;i < argc;i++){
		/* ファイル名解釈ルーチンに渡す。 */
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
