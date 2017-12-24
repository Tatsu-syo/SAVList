/*
SAVList (C) 2005,2017 Tatsuhiko Shoji
The sources for SAVList are distributed under the MIT open source license
*/
#include	<windows.h>
#include	<commctrl.h>
#include	<shlobj.h>
#include	<objbase.h>
#include	<Commdlg.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	"readSav.h"
#include	"savlist.h"
#include	"resource.h"
#include	"verDialog.h"

#define EXE_NAME "savlist.exe"
#define MAN_NAME "SLhelp.html"

// 透明ウインドウ用定義
#define	WS_EX_LAYERED 0x80000
#define LWA_ALPHA 2
typedef DWORD (WINAPI *FWINLAYER)(HWND hwnd,DWORD crKey,BYTE bAlpha,DWORD dwFlags);
int winVer,minorVer;
HINSTANCE hDLL;
FWINLAYER pfWin = NULL;

char szClassName[] = "savlist_class";
HWND hWndMain;	// メインウインドウのハンドル
HWND hList;	// リスト部分のハンドル
HWND hStatus;	// ステータスバーのウインドウハンドル
HINSTANCE hInst;	// インスタンスハンドル
bool readFlag = false;	// 仮想フロッピーディスクファイルをロードしたか？
char *inoutDir;	// ファイル転送・取り出しディレクトリの起点
char *savDir;	// 仮想フロッピーディスクファイル選択の起点

/**
 * メインルーチン
 *
 * @param hCurInst インスタンスハンドル
 * @param hPrevInst 親のインスタンスハンドル
 * @param lpsCmdLine コマンドライン
 * @param nCmdShow 表示状態
 * @return 実行結果
 */
int WINAPI WinMain(HINSTANCE hCurInst,HINSTANCE hPrevInst,LPSTR lpsCmdLine,int nCmdShow)
{
	MSG msg;
	INITCOMMONCONTROLSEX ic;
	HACCEL hAccel;

    DWORD dwver;

    dwver = GetVersion();
	winVer = LOBYTE(dwver);
	if (winVer < 5){	// Windows バージョンチェック
		MessageBox(NULL,"Windows 2000以降のWindowsでのみ\n動作します。","お詫び",MB_OK);
		return(0);
	}
	minorVer = HIBYTE(dwver);

	CoInitialize(NULL);

	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&ic);

	hInst = hCurInst;
	
	savDir = (char *)malloc(MAX_PATH);
	if (savDir == NULL) {
		return FALSE;
	}
	savDir[0] = '\0';

	if (!InitApp(hCurInst)){
		free(savDir);
		return FALSE;
	}
	
	if (!InitInstance(hCurInst,nCmdShow)){
		free(savDir);
		return FALSE;
	}

	inoutDir = (char *)malloc(MAX_PATH);
	if (inoutDir == NULL) {
		free(savDir);
		return FALSE;
	}
	inoutDir[0] = '\0';


	hAccel = LoadAccelerators(hCurInst,MAKEINTRESOURCE(IDR_ACCELERATOR1));
	
	// メッセージ処理
	while (GetMessage(&msg,NULL,0,0)){
		if (!TranslateAccelerator(hWndMain,hAccel,&msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	free(savDir);
	free(inoutDir);

	return (int)msg.wParam;
	
}

/**
 * メインウインドウのウインドウクラスを登録する。
 *
 * @param hInst インスタンスハンドル
 */
ATOM InitApp(HINSTANCE hInst)
{
	WNDCLASSEX wc;
	
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
	wc.lpszClassName = (LPCSTR)szClassName;
	wc.hIconSm = (HICON)LoadImage(hInst,MAKEINTRESOURCE(IDI_ICON1),IMAGE_ICON,
							16,16,LR_DEFAULTSIZE);
	
	return(RegisterClassEx(&wc));
	
}

// メインウインドウを作成する。
BOOL InitInstance(HINSTANCE hInst,int nCmdShow)
{
	HWND hWnd;
	
	hWnd = CreateWindow(szClassName,
			"SAVList",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			510,
			400,
			HWND_DESKTOP,
			NULL,
			hInst,
			NULL);
	if (hWnd == NULL){
		return FALSE;
	}
	ShowWindow(hWnd,nCmdShow);
	UpdateWindow(hWnd);
	hWndMain = hWnd;	// メインウインドウのハンドルを保存する。

	// ファイルのドロップを受けられるようにする
	DragAcceptFiles(hWnd, TRUE);
	
	return TRUE;
}

/**
 * ウインドウプローシジャ
 *
 * @param hWnd 対象ウインドウハンドル
 * @param msg メッセージ
 * @param wp WPARAM
 * @param lp LPARAM
 * @return 実行結果
 */
LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp)
{
	RECT rc;
	
	switch (msg){
		case WM_CREATE:
			if (winVer > 4){
				LONG info;

				info = GetWindowLong(hWnd,GWL_EXSTYLE);	// 拡張スタイルゲット
				SetWindowLong(hWnd,GWL_EXSTYLE,info | WS_EX_LAYERED);

				hDLL = LoadLibrary("user32.dll");
				pfWin = (FWINLAYER)GetProcAddress(hDLL,"SetLayeredWindowAttributes");
				if (pfWin != NULL)
					pfWin(hWnd,0,255,LWA_ALPHA);
			}

			hList = MakeMyList(hWnd);
			InsertMyColumn(hList);
			//InsertMyItem(hList);
			if (__argc > 1){
				openSavFile(__argv[1]);
				if ((strstr(__argv[1], ":\\") != NULL) || (__argv[1][1] == '\\')) {
					getSavDir(__argv[1]);
				} else {
					GetCurrentDirectory(MAX_PATH, savDir);
				}
			}
		break;
		case WM_SIZE:
			// ウインドウサイズ調整に伴うリストビューの調整
			RECT statusRect;
			SendMessage(hStatus,msg,wp,lp);
			GetWindowRect(hStatus,&statusRect);
			GetClientRect(hWnd,&rc);
			MoveWindow(hList,0,0,rc.right - rc.left,
				rc.bottom - rc.top - (statusRect.bottom - statusRect.top)
				,TRUE);
		break;
		case WM_NOTIFY:
			return notifyProc(hWnd, msg, wp, lp);
		case WM_CLOSE:
			cleanup();
			DestroyWindow(hList);
			DestroyWindow(hWnd);
		break;
		case WM_COMMAND:
			return CommandProc(hWnd,msg,wp,lp);
		break;
		case WM_DROPFILES:
			// ファイルがドロップされた
			onDropFiles(wp);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		default:
			return(DefWindowProc(hWnd,msg,wp,lp));
	}
	return 0;
}

/**
 * WM_COMMAND処理ルーチン
 *
 * @param hWnd 対象ウインドウハンドル
 * @param msg メッセージ
 * @param wp WPARAM
 * @param lp LPARAM
 * @return 実行結果
 */
LRESULT CommandProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp)
{
	HMENU menuHandle;

	switch(LOWORD(wp)){
		case IDM_OPEN:
			openSavFileSelect();
			break;
		case IDM_EXIT:
			cleanup();
			DestroyWindow(hList);
			DestroyWindow(hWndMain);
			break;
		case IDM_SELECTALL:
			if (readFlag){
				selectAllFiles(hList);
				UpdateWindow(hList);
			}
			break;
		case IDM_ADDFILE:
			transferFiles();
			break;
		case IDM_EXT:
			extractFiles();
			break;
		case IDM_DELFILES:
			queryDeleteFiles();
			break;
		case IDM_LFNKILL:
			queryLFNKill();
			break;
		case IDM_DISP_EXT:
			menuHandle = GetMenu(hWndMain);
			addExt = 1 - addExt;
			checkMenuItem(menuHandle, IDM_DISP_EXT, addExt);
			refreshFilesDisp(hList);
			break;
		case IDM_FULL_SORT:
			break;
		case IDM_HELP:
			displayHelpFile();
			break;
		case IDM_ABOUT:
			versionDialog();
			break;
		case IDM_UPDIR:
			directoryUpKey();
			break;
		default:
			return DefWindowProc(hWnd,msg,wp,lp);
	}

	return 0;
}

/**
 * WM_NOTIFY処理プロセス
 *
 * @param hWnd 対象ウインドウハンドル
 * @param msg ウインドウメッセージ
 * @param wp WPARAM
 * @param lp lparam
 * @return 実行結果 
 */
LRESULT notifyProc(HWND &hWnd, UINT &msg, WPARAM &wp, LPARAM &lp)
{
	LPNMHDR nmhdr;
	LV_DISPINFO *lvinfo;
	NM_LISTVIEW *pNMLV;

	nmhdr = (LPNMHDR)lp;

	if (nmhdr->hwndFrom == hList){
		lvinfo = (LV_DISPINFO *)lp;
		switch (lvinfo->hdr.code) {
			case LVN_COLUMNCLICK:
				/* カラムがクリックされた時のソート処理 */
				pNMLV = (NM_LISTVIEW *)lp;
				sortStartFlag = 1;
				ListView_SortItems(hList, compareItem, pNMLV->iSubItem);
				break;
			case NM_DBLCLK:
				selectSubDirectory();
				break;
			case NM_RETURN:
				selectSubDirectory();
				break;
			default:
				return DefWindowProc(hWnd,msg,wp,lp);
		}
		return 0;
	}
	return DefWindowProc(hWnd,msg,wp,lp);
}


/**
 * リストビューを作成する。
 *
 * @param 親ウインドウのハンドル
 * @return リストビューのハンドル
 */
HWND MakeMyList(HWND hWnd)
{
	HWND hList;
	DWORD dwStyle;
	LONG style;
	
	/* リストビューを作る */
	hList = CreateWindowEx(0,WC_LISTVIEW,"",
		WS_CHILD | WS_VISIBLE | LVS_REPORT,
		0, 0, 0, 0,
		hWnd,(HMENU)100,hInst,NULL);
	style = GetWindowLong(hList,GWL_STYLE);
	if (style & LVS_EDITLABELS){
		style = style ^ LVS_EDITLABELS;
		SetWindowLong(hList, GWL_STYLE, style);
	}
	dwStyle = ListView_GetExtendedListViewStyle(hList);
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP;	// | LVS_EX_CHECKBOXES;
	ListView_SetExtendedListViewStyle(hList,dwStyle);

	/* ついでにステータスバーも作る。 */
	hStatus = CreateWindowEx(0,STATUSCLASSNAME,NULL,WS_CHILD | WS_VISIBLE | CCS_BOTTOM,
		0,0,0,0,hWnd,(HMENU)39999,hInst,NULL);

	return hList;
}

// カラムを挿入する。
void InsertMyColumn(HWND hList)
{
	LV_COLUMN lvcol;
	
	lvcol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvcol.fmt = LVCFMT_LEFT;
	lvcol.cx = 80;
	lvcol.pszText = "エントリ順";
	lvcol.iSubItem = 0;
	ListView_InsertColumn(hList,0,&lvcol);

	lvcol.cx = 100;
	lvcol.pszText = "ファイル名";
	lvcol.iSubItem = 1;
	ListView_InsertColumn(hList,1,&lvcol);
	
	lvcol.cx = 60;
	lvcol.pszText = "拡張子";
	lvcol.iSubItem = 2;
	ListView_InsertColumn(hList,2,&lvcol);
	
	lvcol.cx = 80;
	lvcol.pszText = "サイズ";
	lvcol.iSubItem = 3;
	ListView_InsertColumn(hList,3,&lvcol);

	lvcol.cx = 150;
	lvcol.pszText = "更新時刻";
	lvcol.iSubItem = 4;
	ListView_InsertColumn(hList,4,&lvcol);
	
	return;
}

/**
 * メニューのチェック状態を設定する。
 *
 * @param menuHandle メニューハンドル
 * @param id メニューID
 * @param true:チェックする false:チェックしない
 */
void checkMenuItem(HMENU menuHandle, int id, int checked)
{
	 MENUITEMINFO info;

	 memset(&info, 0, sizeof(MENUITEMINFO));
	 info.cbSize = sizeof(MENUITEMINFO);
	 info.fMask = MIIM_STATE;

	 GetMenuItemInfo(menuHandle, id, FALSE, &info);
	 if (checked) {
		 info.fState |= MFS_CHECKED;
	 } else {
		 if (info.fState & MFS_CHECKED) {
			info.fState ^= MFS_CHECKED;
		 }
	 }
	 SetMenuItemInfo(menuHandle, id, FALSE, &info);
}

/**
 * 仮想フロッピーディスクファイル選択ダイアログを出す。
 */
void openSavFileSelect(void)
{
	OPENFILENAME ofn;
	char GotFileName[256];

	memset(GotFileName,0x00,sizeof(GotFileName));
	memset(&ofn,0x00,sizeof(OPENFILENAME));
//	if ((winVer > 4) || ((winVer == 4) && (minorVer > 89))){
		ofn.lStructSize = sizeof(OPENFILENAME);
//	}else{
//		ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
//	}
	ofn.hwndOwner = hWndMain;
	ofn.lpstrFilter = "仮想フロッピーディスクファイル(*.SAV)\0*.SAV\0ディスクイメージファイル(*.DSK)\0*.DSK\0全てのファイル (*.*)\0*.*\0";
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = GotFileName;
	ofn.nMaxFile = 256;
	ofn.lpstrTitle = "仮想フロッピーディスクファイルを開く";
	if (savDir[0] != '\0') {
		ofn.lpstrInitialDir = savDir;
	}

	if (GetOpenFileName(&ofn)){
		openSavFile(GotFileName);
		getSavDir(GotFileName);
	}
}

/* 仮想フロッピーディスクファイルを開く */
void openSavFile(char *filename)
{
	char title[_MAX_PATH + 32];
	int result;

	cleanup();

	// ディスクイメージを読み込む。
	result = readSavFile(filename);
	if (result){
		return;
	}
	/* タイトルとステータスバーの状態を更新する。 */
	sprintf(title,"%s - SAVList",filename);
	SetWindowText(hWndMain,title);
	setStatusBarInfo();
	// リストビューに反映する。
	refreshDir(hList);
	readFlag = true;
	ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
	SetFocus(hList);
	// ディレクトリを再表示する。
	UpdateWindow(hList);
	UpdateWindow(hStatus);
}


/* ファイルを仮想フロッピーディスクに転送する */
void transferFiles(void)
{
	OPENFILENAME ofn;
	char *gotFileName;
	char path[260];
	char *p;
	int result;

	if (!readFlag){
		MessageBox(hWndMain,"仮想フロッピーディスクファイルを読み込んでください。","エラー",MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	gotFileName = (char *)malloc(10240);
	if (gotFileName == NULL){
		return;
	}

	memset(gotFileName,0x00,10240);
	memset(&ofn,0x00,sizeof(OPENFILENAME));
//	if ((winVer > 4) || ((winVer == 4) && (minorVer > 89))){
		ofn.lStructSize = sizeof(OPENFILENAME);
//	}else{
//		ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
//	}
	ofn.hwndOwner = hWndMain;
	ofn.lpstrFilter = "全てのファイル (*.*)\0*.*\0";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = gotFileName;
	ofn.nMaxFile = 10240;
	ofn.lpstrTitle = "転送するファイルを選択";
	if (inoutDir[0]) {
		ofn.lpstrInitialDir = inoutDir;
	}

	if (GetOpenFileName(&ofn)){
		p = gotFileName + ofn.nFileOffset;

		if (ofn.nFileOffset > 0){
			if (*(p - 1) != '\0'){
				// ファイル名が1つのときはフルパスが入る。
				strcpy(path,gotFileName);

				result = writeFile(path);
				if (result){
					putTransferErrorMessage(result);
				}
				getDirname(path);
			}else{
				// ファイル名が2つ以上のときは
				// ディレクトリ\0ファイル名1\0(略)\0\0
				do{
					strcpy(path,gotFileName);
					strcat(path,"\\");
					strcat(path,p);

					result = writeFile(path);
					if (result){
						putTransferErrorMessage(result);
						break;
					}
					getDirname(path);

					while(*p){
						p++;
					}
					p++;

				}while(*p);
			}
		}
		// ステータスバーを更新する。
		setStatusBarInfo();
		// ディレクトリを再表示する。
		refreshDir(hList);
		UpdateWindow(hList);
		UpdateWindow(hStatus);
	}
	free(gotFileName);
}

/**
 * ディレクトリ名を取得する。
 *
 * @param path 選択したディレクトリ
 */
void getDirname(char *path)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR];
	int len;

	_splitpath(path, drive,dir, NULL, NULL);
	sprintf(inoutDir, "%s%s", drive, dir);

}

/**
 * SAVファイルのディレクトリ名を取得する。
 *
 * @param path 選択したディレクトリ
 */
void getSavDir(char *path)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR];
	int len;

	_splitpath(path, drive,dir, NULL, NULL);
	sprintf(savDir, "%s%s", drive, dir);

}

/**
 * ファイル転送時のエラーメッセージ表示
 *
 * @param result 処理の結果起こったエラー情報
 */
void putTransferErrorMessage(int result)
{
	if (result == NO_DISKSPACE){
		MessageBox(hWndMain,"仮想フロッピーディスクに空きがありません。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
	}else if (result == NO_ENTRY){
		MessageBox(hWndMain,"ディレクトリエントリの空きがありません。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
	}else{
		MessageBox(hWndMain,"ファイル転送に失敗しました。","エラー",MB_OK | MB_ICONEXCLAMATION);
	}
}

/* 選択されたファイルを展開する。 */
void extractFiles(void)
{
	int count,i;
	UINT state;
	char buf[_MAX_PATH];

	if (!readFlag){
		MessageBox(hWndMain,"仮想フロッピーディスクファイルを読み込んでください。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	count = ListView_GetSelectedCount(hList);
	if (!count){
		MessageBox(hWndMain,"展開するファイルを指定してください。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	// 展開先パスを取得する。
	if (dirselect(hWndMain,buf)){
		return;
	}
	strcpy(inoutDir, buf);

	// 選択されたファイル数を見て、ファイルを展開していく。
	count = ListView_GetItemCount(hList);
	for (i = 0;i < count;i++){
		// リストビューの選択状態を取得する。
		state = ListView_GetItemState(hList,i,LVIS_SELECTED);
		if (state){
			// TODO:リストビューの位置からエントリの位置に直す処理が入る。
			extractFile(buf, i);
		}
		ListView_SetItemState(hList,i,0,LVIS_SELECTED);
	}
	UpdateWindow(hList);
	UpdateWindow(hStatus);
}

/* 展開先ディレクトリを選択する。 */
int dirselect(HWND hWnd,char *buf)
{
	BROWSEINFO info;
	ITEMIDLIST *lpid;
	char dir[MAX_PATH];

    HRESULT hr;
    LPMALLOC pMalloc = NULL;//IMallocへのポインタ

	memset(&info, 0, sizeof(BROWSEINFO));
	info.hwndOwner = hWnd;	//親ウインドウハンドル
	info.pszDisplayName = dir;	// 結果が入るバッファ
	info.lpszTitle = "ターゲットディレクトリの選択";
	info.ulFlags = BIF_RETURNONLYFSDIRS ;
	info.pidlRoot = NULL;	// 初期ディレクトリ
	info.lpfn = NULL;	// イベントが起こった時の関数
	info.lParam = (LPARAM)0;

	lpid = SHBrowseForFolder(&info);
	if (lpid == NULL) {
		return 1;
	} else {
        hr = SHGetMalloc(&pMalloc);
        if (hr == E_FAIL) {
            MessageBox(hWnd, "SHGetMalloc Error", "Error", MB_OK);
            return 1;
        }
        SHGetPathFromIDList(lpid, buf);
		//_SHFree(lpid);
		//SendMessage(control[EDIT_DIR],WM_SETTEXT,(WPARAM)0,(LPARAM)buf);
		//MessageBox(hWnd,buf,"Selected",MB_OK);

		pMalloc->Free(lpid);
		pMalloc->Release();

		// bufに対して展開を行う。
	}
	return 0;
}

/* ファイルを削除するかどうか問い合わせてから削除する。 */
void queryDeleteFiles(void)
{
	int result;

	if (!readFlag){
		MessageBox(hWndMain,"仮想フロッピーディスクファイルを読み込んでください。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	if (ListView_GetSelectedCount(hList) < 1){
		MessageBox(hWndMain,"ファイルを選択してから実行してください。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	result = MessageBox(hWndMain,"選択したファイルを削除しますか？","確認"
		,MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES) {
		// 最初に選択されていたところを取得する。
		int firstSelected;
		for (int i = 0;i < listViewEntrys;i++){
			UINT state = ListView_GetItemState(hList, i, LVIS_FOCUSED);
			if (state & LVIS_FOCUSED){
				firstSelected = i;
				break;
			}
		}

		for (int i = listViewEntrys - 1;i > -1;i--){
			UINT state = ListView_GetItemState(hList,i, LVIS_SELECTED);
			if (!state){
				continue;
			}
			// TODO:リストビューの位置からエントリの位置に直す処理が入る。
			deleteSelectedFiles(hList, i);
		}

		// ステータスバーを更新する。
		setStatusBarInfo();
		// リストビューに反映する。
		// ディレクトリを再表示する。
		//refreshDir(hList);

		// カーソル表示位置を調整する。
		int items = ListView_GetItemCount(hList);
		int newPos;
		if (firstSelected >= items) {
			newPos = items - 1;
		} else {
			newPos = firstSelected;
		}
		ListView_SetItemState(hList, newPos, 1, LVIS_FOCUSED);

		UpdateWindow(hList);
		UpdateWindow(hStatus);
	}
	SetFocus(hList);
}

/* ファイルを削除するかどうか問い合わせてから削除する。 */
void queryLFNKill(void)
{
	int result;

	if (!readFlag){
		MessageBox(hWndMain,"仮想フロッピーディスクファイルを読み込んでください。","エラー"
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	result = MessageBox(hWndMain,"ロングファイル名を削除しますか？","確認"
		,MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES){
		deleteLongFilename(hList);

		// ステータスバーを更新する。
		setStatusBarInfo();
		// リストビューに反映する。
		// ディレクトリを再表示する。
		refreshDir(hList);
		UpdateWindow(hList);
		UpdateWindow(hStatus);
	}
	ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
	SetFocus(hList);
}

/* サブディレクトリをダブルクリックしたときの動作 */
void selectSubDirectory(void)
{
	int count,i;
	UINT state;

	if (!readFlag){
		return;
	}

	count = ListView_GetSelectedCount(hList);
	if (!count){
		return;
	}

	// 選択されたファイル数を見て、ファイルを展開していく。
	count = ListView_GetItemCount(hList);
	for (i = 0;i < count;i++){
		// リストビューの選択状態を取得する。
		// TODO:リストビューの位置からエントリの位置に直す処理が入る。
		state = ListView_GetItemState(hList,i,LVIS_SELECTED);
		if (state){
			ListView_SetItemState(hList,i,0,LVIS_SELECTED);
			enterDirectory(hList,i);
			break;
		}
	}
	// ステータスバーを更新する。
	setStatusBarInfo();
	// ディレクトリリストを更新する。
	UpdateWindow(hList);
	UpdateWindow(hStatus);
	ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
	SetFocus(hList);
	
}

/* サブディレクトリをダブルクリックしたときの動作 */
void directoryUpKey(void)
{
	if (!readFlag){
		return;
	}

	if (upDirectory(hList)){
		// ステータスバーを更新する。
		setStatusBarInfo();
		// ディレクトリリストを更新する。
		UpdateWindow(hList);
		UpdateWindow(hStatus);
		ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
		SetFocus(hList);
	}
	
}

/**
 * ファイルドロップ時の動作
 *
 * @param wp ファイルドロップにより渡されたWPARAM
 */
void onDropFiles(WPARAM wp)
{
	HDROP hDrop;
	int dropFiles;
	int i;
	char filePath[MAX_PATH];
	int result;
	char fileExt[_MAX_EXT];

	hDrop = (HDROP)wp;

	dropFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	for (i = 0; i < dropFiles; i++) {
		DragQueryFile(hDrop, i, filePath, MAX_PATH);

		if (readFlag) {
			// 読み込み済みの場合はSAVファイルへの転送とする。
			result = writeFile(filePath);
			if (result){
				putTransferErrorMessage(result);
				break;
			}
		} else {
			if (strlen(filePath) < 4) {
				return;
			}
			_splitpath(filePath, NULL, NULL, NULL, fileExt);
			if (_stricmp(".sav", fileExt)) {
				return;
			}
			openSavFile(filePath);
			break;
		}
	}
	DragFinish(hDrop);

	// ステータスバーを更新する。
	setStatusBarInfo();
	// ディレクトリを再表示する。
	refreshDir(hList);
	UpdateWindow(hList);
	UpdateWindow(hStatus);

}


/* ステータスバーに情報を設定する。 */
void setStatusBarInfo(void)
{
	struct SavStatus status;
	char buf[256];

	getSavStatus(&status);
	sprintf(buf,"%ld Files %ld Directorys Free:%ld Total:%ld",
		status.files,status.directorys,status.freeSize,status.maxSize);
	SetWindowText(hStatus,buf);
}

/**
 * ヘルプファイルを表示する
 */
void displayHelpFile(void)
{
	DWORD result;
	char pathname[MAX_PATH],curDir[_MAX_DIR+_MAX_DRIVE+1],drive[_MAX_DRIVE+1],dir[_MAX_DIR+1];
	HMODULE ModHdl;
	char helpFile[MAX_PATH];
	HINSTANCE inst;

	// 実行ファイルのディレクトリを得る。
	ModHdl = GetModuleHandle(NULL);
	if (ModHdl == NULL)
		return;
	result = GetModuleFileName(ModHdl,pathname,MAX_PATH);

	_splitpath(pathname,drive,dir,NULL,NULL);

	curDir[0] = '\0';
	strcpy(curDir,drive);
	strcat(curDir,dir);

	strcpy(helpFile,curDir);
	strcat(helpFile,MAN_NAME);

	//inst = ShellExecute(hWnd,"open",MAN_NAME,NULL,curDir,SW_SHOWNORMAL);
	inst = ShellExecute(NULL,"open",helpFile,NULL,NULL,SW_SHOWNORMAL);
}

/**
 * バージョン表示ダイアログを表示する
 */
void versionDialog(void)
{
	// Windows 2000/XPでウインドウを透明化する。
	if ((winVer > 4) && (pfWin != NULL))
		pfWin(hWndMain,0,192,LWA_ALPHA);

	// バージョン情報ダイアログを表示する。
	DialogBox(hInst,MAKEINTRESOURCE(IDD_VERSIONDIALOG),hWndMain,VerDialogProc);

	// Windows 2000/XPでウインドウを元に戻す。
	if ((winVer > 4) && (pfWin != NULL))
		pfWin(hWndMain,0,255,LWA_ALPHA);
}
