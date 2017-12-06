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
	if (entryOnListView != NULL){
		free(entryOnListView);
		entryOnListView = NULL;
	}
}

/**
 * 指定されたファイルの情報をディレクトリにセットする。
 * ここはWindowsに依存する。
 *
 * @param d ディレクトリ情報
 * @filename ディレクトリ情報構造体に設定するファイル名
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

	/* ファイルの情報を得る。 */
	hFind = FindFirstFile(filename,&info);
	if (hFind == INVALID_HANDLE_VALUE){
		fprintf(stderr,"File not found.\n");
		return 2;
	}
	FindClose(hFind);

	/* ディスクの空きよりファイルサイズの方が大きかったらエラーとして抜ける。 */
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

	/* ファイルの情報をディレクトリに設定していく。 */
	/* タイムスタンプ */
	/* UTCから現在の時刻に直す */

	/* 時差があるときの処理 */
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
	int *newEntryOnListView;

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

	/* ディレクトリエントリ数分再度確保する */
	newEntryOnListView = (int *)malloc(sizeof(int) * directoryEntrys);
	if (newEntryOnListView == NULL){
		return 1;
	}
	entryOnListView = newEntryOnListView;

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

/**
 * ファイルの書き込みを行う。
 *
 * @param filename 書き込むファイル名
 * @return 書き込み結果 0:成功 0以外:失敗
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

	/* ディレクトリがセットされていない場合、何もしない。 */
	if (DirEntryBuf == NULL)
		return 1;
	if (entryOnListView == NULL)
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

/* ファイルを展開する */
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

	/* ディレクトリがセットされていない場合、何もしない。 */
	if (DirEntryBuf == NULL)
		return 1;
	if (entryOnListView == NULL)
		return 1;

	/* ディレクトリエントリ取得 */
	entry = entryOnListView[listviewEntry];
	getDirectory(&d,entry);

	/* パス名生成 */
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

	/* タイムスタンプを設定する。 */
	setTimeStamp(&d, buf);

	return 0;
}

/**
 * タイムスタンプをディスクイメージのファイルから設定する。
 *
 * @param d ディレクトリエントリ
 * @param filename 設定対象ファイル
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

/* ディレクトリを読み込む */
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
		directoryEntrys = SECTORSIZE * 7 / 32;	/* ディレクトリエントリ数 */

		DirEntryBuf = malloc((size_t)(SECTORSIZE * 7));	/* ディレクトリの分だけ再度確保する。 */
		if (DirEntryBuf == NULL){
			return;
		}

		/* ディレクトリエントリ数分再度確保する */
		entryOnListView = (int *)malloc(sizeof(int) * directoryEntrys);
		if (entryOnListView == NULL){
			return;
		}

		/* ルートディレクトリを読み込む */
		for (i = 0;i < 7;i++){
			readSector(fpSav,7 + i,DirEntryBuf + (SECTORSIZE * i));
		}
		isSubDirectory = 0;
	}else{
		clusterCount = countClusters(startCluster);	/* ディレクトリに割り当てられたクラスタ数 */
		directoryEntrys = clusterCount * clusterSize / 32;	/* ディレクトリエントリ数 */

		DirEntryBuf = malloc((size_t)(clusterSize * clusterCount));	/* ディレクトリの分だけ再度確保する。 */
		if (DirEntryBuf == NULL){
			return;
		}

		/* ディレクトリエントリ数分再度確保する */
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
 * ディレクトリの状態(ファイル数・ディレクトリ数)を算出する。
 */
void getSavStatus(struct SavStatus *status)
{
	int i;
	unsigned long clusterSize;
	unsigned long files = 0,directorys = 0;
	struct dirEntry d2;

	/* 空き容量計算 */
	clusterSize = SECTORSIZE * SectorPerCluster;
	status->freeSize = getFreeClusterCount() * clusterSize;
	status->maxSize = MaxEntry * clusterSize;


	/* 仮想フロッピーディスクファイルにディレクトリエントリの空きがあるか？ */
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
				/* ボリュームラベルかロングファイル名 */
			}else{
				files++;
			}
		}
	}

	status->directorys = directorys;
	status->files = files;
}

/* SAVファイルを読み込ませる */
int readSavFile(char *filename)
{
	int result;
	
	/* ディスクの情報を設定する。 */
	FatSectors = 3;	/* FAT用のセクタ数 */
	MaxEntry = (SECTORS - 14) / 2 + 2;	/* 最大FATエントリ数 */
	SectorPerCluster = 2;	/* 1クラスタを構成するセクタ数 */
	DataArea = 14;	/* データエリア開始セクタ */
	directoryEntrys = 112;	/* 現在のディレクトリのエントリ数 */
	isSubDirectory = 0;	/* ルートディレクトリ */

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
/* この先はGUI連携用関数 */
/*-----------------------*/

/* ディレクトリを再表示する */
void refreshDir(HWND hList)
{
	int i,pos;
	LV_ITEM item;
	char buf[32];
	struct dirEntry d;
	int listCount = 0;	/* リスト上に入っているエントリ数 */

	if (DirEntryBuf == NULL)
		return;
	if (entryOnListView == NULL)
		return;

	memset(&item,0,sizeof(LV_ITEM));
	ListView_DeleteAllItems(hList);

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		item.mask = LVIF_TEXT;

		/* ディレクトリエントリを取得する。 */
		getDirectory(&d,i);

		/* ディレクトリエントリの終わりの場合表示を打ち切る。 */
		if (d.filename[0] == 0x00){
			break;
		}

		/* 削除されたエントリは表示しない。 */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		/* ロングファイル名は表示しない */
		if (d.attr == 0x0f){
			continue;
		}

		/* ディレクトリエントリ上の位置とリストビュー上の位置を関連付ける */
		entryOnListView[listCount] = i;
		listCount++;

		/* ディレクトリエントリ上の位置 */
		sprintf(buf,"%d",i);
		item.pszText = buf;
		item.iItem = pos;
		item.iSubItem = 0;
		ListView_InsertItem(hList,&item);

		/* ファイル名 */
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


		/* ファイルサイズ */
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

		/* 更新時刻 */
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

/* ファイルをすべて選択する */
void selectAllFiles(HWND hList)
{
	int i,pos;
	struct dirEntry d;
	int listCount = 0;	/* リスト上に入っているエントリ数 */

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		/* ディレクトリエントリを取得する。 */
		getDirectory(&d,i);

		/* ディレクトリエントリの終わりの場合表示を打ち切る。 */
		if (d.filename[0] == 0x00){
			break;
		}

		/* 削除されたエントリは表示しない。 */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		/* ロングファイル名は表示しない */
		if (d.attr == 0x0f){
			continue;
		}

		if (d.attr & 0x10){
			/* ディレクトリ */
			ListView_SetItemState(hList,listCount,0,LVIS_SELECTED);
		}else if (d.attr & 0x08) {
			/* ボリュームラベル */
			ListView_SetItemState(hList,listCount,0,LVIS_SELECTED);
		}else{
			ListView_SetItemState(hList,listCount,LVIS_SELECTED,LVIS_SELECTED);
		}

		/* ディレクトリエントリ上の位置とリストビュー上の位置を関連付ける */
		listCount++;
	}

}

/* ファイルをすべて選択する */
int upDirectory(HWND hList)
{
	int i,pos;
	struct dirEntry d;
	int listCount = 0;	/* リスト上に入っているエントリ数 */

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		/* ディレクトリエントリを取得する。 */
		getDirectory(&d,i);

		/* ディレクトリエントリの終わりの場合処理を打ち切る。 */
		if (d.filename[0] == 0x00){
			break;
		}

		/* 削除されたエントリは飛ばす。 */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		/* ロングファイル名は飛ばす */
		if (d.attr == 0x0f){
			continue;
		}

		if (d.attr & 0x10){
			/* ディレクトリ */
			if (d.filename[0] == '.' &&
				d.filename[1] == '.' &&
				d.filename[2] == ' '){
				enterDirectory(hList,i);
				return 1;
			}
		}

		/* ディレクトリエントリ上の位置とリストビュー上の位置を関連付ける */
		listCount++;
	}
	return 0;
}

/* サブディレクトリの中に入る */
void enterDirectory(HWND hList,int listviewEntry)
{
	int entry;
	struct dirEntry d;

	if (DirEntryBuf == NULL)
		return;
	if (entryOnListView == NULL)
		return;

	/* ディレクトリエントリ取得 */
	entry = entryOnListView[listviewEntry];
	getDirectory(&d,entry);

	if (d.attr & 0x10){
		if (d.filename[0] == '.' && d.filename[1] == ' '){
			/* .(自分のディレクトリ)の場合何もしない */
		}else{
			/* ディレクトリを読み込む */
			readDirectory(d.cluster);
			fs_startCluster = d.cluster;
			/* ディレクトリの内容をリストビューに転送する */
			refreshDir(hList);
		}
	}

}

/* 選択したファイルを削除する */
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

		/* ディレクトリエントリを取得する。 */
		getDirectory(&d,entry);

		if (d.attr & 0x10){
			/* ディレクトリ */
			continue;
		}else if (d.attr & 0x08) {
			/* ボリュームラベル */
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

	/* ファイル削除に成功したらFATとディレクトリを書き込む。 */
	flushSavfile();
}

/**
 * ロングファイル名を削除する。 
 */
void deleteLongFilename(HWND hList)
{
	int i,pos;
	struct dirEntry d;
	int listCount = 0;	/* リスト上に入っているエントリ数 */

	pos = 0;
	for (i = 0;i < directoryEntrys;i++){
		/* ディレクトリエントリを取得する。 */
		getDirectory(&d,i);

		/* ディレクトリエントリの終わりの場合表示を打ち切る。 */
		if (d.filename[0] == 0x00){
			break;
		}

		/* 削除されたエントリは表示しない。 */
		if ((unsigned char)(d.filename[0]) == 0xe5){
			continue;
		}

		if ((d.attr & 0x0f) == 0x0f){
			d.filename[0] = 0xe5;
			setDirectory(&d,i);
		}
	}
	/* ファイル削除に成功したらFATとディレクトリを書き込む。 */
	flushSavfile();

}

