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

// �����E�C���h�E�p��`
#define	WS_EX_LAYERED 0x80000
#define LWA_ALPHA 2
typedef DWORD (WINAPI *FWINLAYER)(HWND hwnd,DWORD crKey,BYTE bAlpha,DWORD dwFlags);
int winVer,minorVer;
HINSTANCE hDLL;
FWINLAYER pfWin = NULL;

char szClassName[] = "savlist_class";
HWND hWndMain;	// ���C���E�C���h�E�̃n���h��
HWND hList;	// ���X�g�����̃n���h��
HWND hStatus;	// �X�e�[�^�X�o�[�̃E�C���h�E�n���h��
HINSTANCE hInst;	// �C���X�^���X�n���h��
bool readFlag = false;	// ���z�t���b�s�[�f�B�X�N�t�@�C�������[�h�������H
char *inoutDir;	// �t�@�C���]���E���o���f�B���N�g���̋N�_
char *savDir;	// ���z�t���b�s�[�f�B�X�N�t�@�C���I���̋N�_

/**
 * ���C�����[�`��
 *
 * @param hCurInst �C���X�^���X�n���h��
 * @param hPrevInst �e�̃C���X�^���X�n���h��
 * @param lpsCmdLine �R�}���h���C��
 * @param nCmdShow �\�����
 * @return ���s����
 */
int WINAPI WinMain(HINSTANCE hCurInst,HINSTANCE hPrevInst,LPSTR lpsCmdLine,int nCmdShow)
{
	MSG msg;
	INITCOMMONCONTROLSEX ic;
	HACCEL hAccel;

    DWORD dwver;

    dwver = GetVersion();
	winVer = LOBYTE(dwver);
	if (winVer < 5){	// Windows �o�[�W�����`�F�b�N
		MessageBox(NULL,"Windows 2000�ȍ~��Windows�ł̂�\n���삵�܂��B","���l��",MB_OK);
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
	
	// ���b�Z�[�W����
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
 * ���C���E�C���h�E�̃E�C���h�E�N���X��o�^����B
 *
 * @param hInst �C���X�^���X�n���h��
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

// ���C���E�C���h�E���쐬����B
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
	hWndMain = hWnd;	// ���C���E�C���h�E�̃n���h����ۑ�����B

	// �t�@�C���̃h���b�v���󂯂���悤�ɂ���
	DragAcceptFiles(hWnd, TRUE);
	
	return TRUE;
}

/**
 * �E�C���h�E�v���[�V�W��
 *
 * @param hWnd �ΏۃE�C���h�E�n���h��
 * @param msg ���b�Z�[�W
 * @param wp WPARAM
 * @param lp LPARAM
 * @return ���s����
 */
LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp)
{
	RECT rc;
	
	switch (msg){
		case WM_CREATE:
			if (winVer > 4){
				LONG info;

				info = GetWindowLong(hWnd,GWL_EXSTYLE);	// �g���X�^�C���Q�b�g
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
			// �E�C���h�E�T�C�Y�����ɔ������X�g�r���[�̒���
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
			// �t�@�C�����h���b�v���ꂽ
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
 * WM_COMMAND�������[�`��
 *
 * @param hWnd �ΏۃE�C���h�E�n���h��
 * @param msg ���b�Z�[�W
 * @param wp WPARAM
 * @param lp LPARAM
 * @return ���s����
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
 * WM_NOTIFY�����v���Z�X
 *
 * @param hWnd �ΏۃE�C���h�E�n���h��
 * @param msg �E�C���h�E���b�Z�[�W
 * @param wp WPARAM
 * @param lp lparam
 * @return ���s���� 
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
				/* �J�������N���b�N���ꂽ���̃\�[�g���� */
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
 * ���X�g�r���[���쐬����B
 *
 * @param �e�E�C���h�E�̃n���h��
 * @return ���X�g�r���[�̃n���h��
 */
HWND MakeMyList(HWND hWnd)
{
	HWND hList;
	DWORD dwStyle;
	LONG style;
	
	/* ���X�g�r���[����� */
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

	/* ���łɃX�e�[�^�X�o�[�����B */
	hStatus = CreateWindowEx(0,STATUSCLASSNAME,NULL,WS_CHILD | WS_VISIBLE | CCS_BOTTOM,
		0,0,0,0,hWnd,(HMENU)39999,hInst,NULL);

	return hList;
}

// �J������}������B
void InsertMyColumn(HWND hList)
{
	LV_COLUMN lvcol;
	
	lvcol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvcol.fmt = LVCFMT_LEFT;
	lvcol.cx = 80;
	lvcol.pszText = "�G���g����";
	lvcol.iSubItem = 0;
	ListView_InsertColumn(hList,0,&lvcol);

	lvcol.cx = 100;
	lvcol.pszText = "�t�@�C����";
	lvcol.iSubItem = 1;
	ListView_InsertColumn(hList,1,&lvcol);
	
	lvcol.cx = 60;
	lvcol.pszText = "�g���q";
	lvcol.iSubItem = 2;
	ListView_InsertColumn(hList,2,&lvcol);
	
	lvcol.cx = 80;
	lvcol.pszText = "�T�C�Y";
	lvcol.iSubItem = 3;
	ListView_InsertColumn(hList,3,&lvcol);

	lvcol.cx = 150;
	lvcol.pszText = "�X�V����";
	lvcol.iSubItem = 4;
	ListView_InsertColumn(hList,4,&lvcol);
	
	return;
}

/**
 * ���j���[�̃`�F�b�N��Ԃ�ݒ肷��B
 *
 * @param menuHandle ���j���[�n���h��
 * @param id ���j���[ID
 * @param true:�`�F�b�N���� false:�`�F�b�N���Ȃ�
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
 * ���z�t���b�s�[�f�B�X�N�t�@�C���I���_�C�A���O���o���B
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
	ofn.lpstrFilter = "���z�t���b�s�[�f�B�X�N�t�@�C��(*.SAV)\0*.SAV\0�f�B�X�N�C���[�W�t�@�C��(*.DSK)\0*.DSK\0�S�Ẵt�@�C�� (*.*)\0*.*\0";
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = GotFileName;
	ofn.nMaxFile = 256;
	ofn.lpstrTitle = "���z�t���b�s�[�f�B�X�N�t�@�C�����J��";
	if (savDir[0] != '\0') {
		ofn.lpstrInitialDir = savDir;
	}

	if (GetOpenFileName(&ofn)){
		openSavFile(GotFileName);
		getSavDir(GotFileName);
	}
}

/* ���z�t���b�s�[�f�B�X�N�t�@�C�����J�� */
void openSavFile(char *filename)
{
	char title[_MAX_PATH + 32];
	int result;

	cleanup();

	// �f�B�X�N�C���[�W��ǂݍ��ށB
	result = readSavFile(filename);
	if (result){
		return;
	}
	/* �^�C�g���ƃX�e�[�^�X�o�[�̏�Ԃ��X�V����B */
	sprintf(title,"%s - SAVList",filename);
	SetWindowText(hWndMain,title);
	setStatusBarInfo();
	// ���X�g�r���[�ɔ��f����B
	refreshDir(hList);
	readFlag = true;
	ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
	SetFocus(hList);
	// �f�B���N�g�����ĕ\������B
	UpdateWindow(hList);
	UpdateWindow(hStatus);
}


/* �t�@�C�������z�t���b�s�[�f�B�X�N�ɓ]������ */
void transferFiles(void)
{
	OPENFILENAME ofn;
	char *gotFileName;
	char path[260];
	char *p;
	int result;

	if (!readFlag){
		MessageBox(hWndMain,"���z�t���b�s�[�f�B�X�N�t�@�C����ǂݍ���ł��������B","�G���[",MB_OK | MB_ICONEXCLAMATION);
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
	ofn.lpstrFilter = "�S�Ẵt�@�C�� (*.*)\0*.*\0";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = gotFileName;
	ofn.nMaxFile = 10240;
	ofn.lpstrTitle = "�]������t�@�C����I��";
	if (inoutDir[0]) {
		ofn.lpstrInitialDir = inoutDir;
	}

	if (GetOpenFileName(&ofn)){
		p = gotFileName + ofn.nFileOffset;

		if (ofn.nFileOffset > 0){
			if (*(p - 1) != '\0'){
				// �t�@�C������1�̂Ƃ��̓t���p�X������B
				strcpy(path,gotFileName);

				result = writeFile(path);
				if (result){
					putTransferErrorMessage(result);
				}
				getDirname(path);
			}else{
				// �t�@�C������2�ȏ�̂Ƃ���
				// �f�B���N�g��\0�t�@�C����1\0(��)\0\0
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
		// �X�e�[�^�X�o�[���X�V����B
		setStatusBarInfo();
		// �f�B���N�g�����ĕ\������B
		refreshDir(hList);
		UpdateWindow(hList);
		UpdateWindow(hStatus);
	}
	free(gotFileName);
}

/**
 * �f�B���N�g�������擾����B
 *
 * @param path �I�������f�B���N�g��
 */
void getDirname(char *path)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR];
	int len;

	_splitpath(path, drive,dir, NULL, NULL);
	sprintf(inoutDir, "%s%s", drive, dir);

}

/**
 * SAV�t�@�C���̃f�B���N�g�������擾����B
 *
 * @param path �I�������f�B���N�g��
 */
void getSavDir(char *path)
{
	char drive[_MAX_DRIVE],dir[_MAX_DIR];
	int len;

	_splitpath(path, drive,dir, NULL, NULL);
	sprintf(savDir, "%s%s", drive, dir);

}

/**
 * �t�@�C���]�����̃G���[���b�Z�[�W�\��
 *
 * @param result �����̌��ʋN�������G���[���
 */
void putTransferErrorMessage(int result)
{
	if (result == NO_DISKSPACE){
		MessageBox(hWndMain,"���z�t���b�s�[�f�B�X�N�ɋ󂫂�����܂���B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
	}else if (result == NO_ENTRY){
		MessageBox(hWndMain,"�f�B���N�g���G���g���̋󂫂�����܂���B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
	}else{
		MessageBox(hWndMain,"�t�@�C���]���Ɏ��s���܂����B","�G���[",MB_OK | MB_ICONEXCLAMATION);
	}
}

/* �I�����ꂽ�t�@�C����W�J����B */
void extractFiles(void)
{
	int count,i;
	UINT state;
	char buf[_MAX_PATH];

	if (!readFlag){
		MessageBox(hWndMain,"���z�t���b�s�[�f�B�X�N�t�@�C����ǂݍ���ł��������B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	count = ListView_GetSelectedCount(hList);
	if (!count){
		MessageBox(hWndMain,"�W�J����t�@�C�����w�肵�Ă��������B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	// �W�J��p�X���擾����B
	if (dirselect(hWndMain,buf)){
		return;
	}
	strcpy(inoutDir, buf);

	// �I�����ꂽ�t�@�C���������āA�t�@�C����W�J���Ă����B
	count = ListView_GetItemCount(hList);
	for (i = 0;i < count;i++){
		// ���X�g�r���[�̑I����Ԃ��擾����B
		state = ListView_GetItemState(hList,i,LVIS_SELECTED);
		if (state){
			// TODO:���X�g�r���[�̈ʒu����G���g���̈ʒu�ɒ�������������B
			extractFile(buf, i);
		}
		ListView_SetItemState(hList,i,0,LVIS_SELECTED);
	}
	UpdateWindow(hList);
	UpdateWindow(hStatus);
}

/* �W�J��f�B���N�g����I������B */
int dirselect(HWND hWnd,char *buf)
{
	BROWSEINFO info;
	ITEMIDLIST *lpid;
	char dir[MAX_PATH];

    HRESULT hr;
    LPMALLOC pMalloc = NULL;//IMalloc�ւ̃|�C���^

	memset(&info, 0, sizeof(BROWSEINFO));
	info.hwndOwner = hWnd;	//�e�E�C���h�E�n���h��
	info.pszDisplayName = dir;	// ���ʂ�����o�b�t�@
	info.lpszTitle = "�^�[�Q�b�g�f�B���N�g���̑I��";
	info.ulFlags = BIF_RETURNONLYFSDIRS ;
	info.pidlRoot = NULL;	// �����f�B���N�g��
	info.lpfn = NULL;	// �C�x���g���N���������̊֐�
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

		// buf�ɑ΂��ēW�J���s���B
	}
	return 0;
}

/* �t�@�C�����폜���邩�ǂ����₢���킹�Ă���폜����B */
void queryDeleteFiles(void)
{
	int result;

	if (!readFlag){
		MessageBox(hWndMain,"���z�t���b�s�[�f�B�X�N�t�@�C����ǂݍ���ł��������B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	if (ListView_GetSelectedCount(hList) < 1){
		MessageBox(hWndMain,"�t�@�C����I�����Ă�����s���Ă��������B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	result = MessageBox(hWndMain,"�I�������t�@�C�����폜���܂����H","�m�F"
		,MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES) {
		// �ŏ��ɑI������Ă����Ƃ�����擾����B
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
			// TODO:���X�g�r���[�̈ʒu����G���g���̈ʒu�ɒ�������������B
			deleteSelectedFiles(hList, i);
		}

		// �X�e�[�^�X�o�[���X�V����B
		setStatusBarInfo();
		// ���X�g�r���[�ɔ��f����B
		// �f�B���N�g�����ĕ\������B
		//refreshDir(hList);

		// �J�[�\���\���ʒu�𒲐�����B
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

/* �t�@�C�����폜���邩�ǂ����₢���킹�Ă���폜����B */
void queryLFNKill(void)
{
	int result;

	if (!readFlag){
		MessageBox(hWndMain,"���z�t���b�s�[�f�B�X�N�t�@�C����ǂݍ���ł��������B","�G���["
					,MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	result = MessageBox(hWndMain,"�����O�t�@�C�������폜���܂����H","�m�F"
		,MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

	if (result == IDYES){
		deleteLongFilename(hList);

		// �X�e�[�^�X�o�[���X�V����B
		setStatusBarInfo();
		// ���X�g�r���[�ɔ��f����B
		// �f�B���N�g�����ĕ\������B
		refreshDir(hList);
		UpdateWindow(hList);
		UpdateWindow(hStatus);
	}
	ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
	SetFocus(hList);
}

/* �T�u�f�B���N�g�����_�u���N���b�N�����Ƃ��̓��� */
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

	// �I�����ꂽ�t�@�C���������āA�t�@�C����W�J���Ă����B
	count = ListView_GetItemCount(hList);
	for (i = 0;i < count;i++){
		// ���X�g�r���[�̑I����Ԃ��擾����B
		// TODO:���X�g�r���[�̈ʒu����G���g���̈ʒu�ɒ�������������B
		state = ListView_GetItemState(hList,i,LVIS_SELECTED);
		if (state){
			ListView_SetItemState(hList,i,0,LVIS_SELECTED);
			enterDirectory(hList,i);
			break;
		}
	}
	// �X�e�[�^�X�o�[���X�V����B
	setStatusBarInfo();
	// �f�B���N�g�����X�g���X�V����B
	UpdateWindow(hList);
	UpdateWindow(hStatus);
	ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
	SetFocus(hList);
	
}

/* �T�u�f�B���N�g�����_�u���N���b�N�����Ƃ��̓��� */
void directoryUpKey(void)
{
	if (!readFlag){
		return;
	}

	if (upDirectory(hList)){
		// �X�e�[�^�X�o�[���X�V����B
		setStatusBarInfo();
		// �f�B���N�g�����X�g���X�V����B
		UpdateWindow(hList);
		UpdateWindow(hStatus);
		ListView_SetItemState(hList,0,1,LVIS_FOCUSED);
		SetFocus(hList);
	}
	
}

/**
 * �t�@�C���h���b�v���̓���
 *
 * @param wp �t�@�C���h���b�v�ɂ��n���ꂽWPARAM
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
			// �ǂݍ��ݍς݂̏ꍇ��SAV�t�@�C���ւ̓]���Ƃ���B
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

	// �X�e�[�^�X�o�[���X�V����B
	setStatusBarInfo();
	// �f�B���N�g�����ĕ\������B
	refreshDir(hList);
	UpdateWindow(hList);
	UpdateWindow(hStatus);

}


/* �X�e�[�^�X�o�[�ɏ���ݒ肷��B */
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
 * �w���v�t�@�C����\������
 */
void displayHelpFile(void)
{
	DWORD result;
	char pathname[MAX_PATH],curDir[_MAX_DIR+_MAX_DRIVE+1],drive[_MAX_DRIVE+1],dir[_MAX_DIR+1];
	HMODULE ModHdl;
	char helpFile[MAX_PATH];
	HINSTANCE inst;

	// ���s�t�@�C���̃f�B���N�g���𓾂�B
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
 * �o�[�W�����\���_�C�A���O��\������
 */
void versionDialog(void)
{
	// Windows 2000/XP�ŃE�C���h�E�𓧖�������B
	if ((winVer > 4) && (pfWin != NULL))
		pfWin(hWndMain,0,192,LWA_ALPHA);

	// �o�[�W�������_�C�A���O��\������B
	DialogBox(hInst,MAKEINTRESOURCE(IDD_VERSIONDIALOG),hWndMain,VerDialogProc);

	// Windows 2000/XP�ŃE�C���h�E�����ɖ߂��B
	if ((winVer > 4) && (pfWin != NULL))
		pfWin(hWndMain,0,255,LWA_ALPHA);
}
