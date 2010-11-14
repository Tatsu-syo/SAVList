#include	<windows.h>
#include	<commctrl.h>
#include	<shlobj.h>
#include	<objbase.h>
#include	<Commdlg.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	"readSav.h"
#include	"resource.h"
#include	"verDialog.h"

LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
ATOM InitApp(HINSTANCE);
BOOL InitInstance(HINSTANCE,int);
LRESULT CommandProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp);

HWND MakeMyList(HWND);
void InsertMyColumn(HWND);

void openSavFileSelect(void);
void openSavFile(char *filename);
void transferFiles(void);
void putTransferErrorMessage(int result);
void extractFiles(void);
int dirselect(HWND hWnd,char *buf);
void queryDeleteFiles(void);
void queryLFNKill(void);
void directoryUpKey(void);
void selectSubDirectory(void);
void setStatusBarInfo(void);
void displayHelpFile(void);
void versionDialog(void);

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
bool readFlag = false;

int WINAPI WinMain(HINSTANCE hCurInst,HINSTANCE hPrevInst,LPSTR lpsCmdLine,int nCmdShow)
{
	MSG msg;
	INITCOMMONCONTROLSEX ic;
	HACCEL hAccel;

    DWORD dwver;

    dwver = GetVersion();
	winVer = LOBYTE(dwver);
	if (winVer < 4){	// Windows NT 3.xx�`�F�b�N
		MessageBox(NULL,"Windows 95/NT4�ȍ~��Windows�ł̂�\n���삵�܂��B","���l��",MB_OK);
		return(0);
	}
	minorVer = HIBYTE(dwver);

	CoInitialize(NULL);

	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&ic);

	hInst = hCurInst;
	
	if (!InitApp(hCurInst)){
		return FALSE;
	}
	
	if (!InitInstance(hCurInst,nCmdShow)){
		return FALSE;
	}

	hAccel = LoadAccelerators(hCurInst,MAKEINTRESOURCE(IDR_ACCELERATOR1));
	
	// ���b�Z�[�W����
	while (GetMessage(&msg,NULL,0,0)){
		if (!TranslateAccelerator(hWndMain,hAccel,&msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
	
}

// ���C���E�C���h�E�̃E�C���h�E�N���X��o�^����B
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
			475,
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
	
	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp)
{
	RECT rc;
	LPNMHDR nm;
	
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
			}
		break;
		case WM_SIZE:
			RECT statusRect;
			SendMessage(hStatus,msg,wp,lp);
			GetWindowRect(hStatus,&statusRect);
			GetClientRect(hWnd,&rc);
			MoveWindow(hList,0,0,rc.right - rc.left,
				rc.bottom - rc.top - (statusRect.bottom - statusRect.top)
				,TRUE);
		break;
		case WM_NOTIFY:
			nm = (LPNMHDR)lp;
			if (nm->hwndFrom == hList){
				switch (nm->code) {
					case NM_DBLCLK:
						selectSubDirectory();
						break;
					case NM_RETURN:
						selectSubDirectory();
						break;
					default:
						return(DefWindowProc(hWnd,msg,wp,lp));
						break;
				}
			}else{
				return(DefWindowProc(hWnd,msg,wp,lp));
			}
			break;
		case WM_CLOSE:
			cleanup();
			DestroyWindow(hList);
			DestroyWindow(hWnd);
		break;
		case WM_COMMAND:
			CommandProc(hWnd,msg,wp,lp);
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		default:
			return(DefWindowProc(hWnd,msg,wp,lp));
	}
	return 0;
}

//
LRESULT CommandProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp)
{
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
			return(DefWindowProc(hWnd,msg,wp,lp));
	}

	return 0;
}

// ���X�g�r���[���쐬����B
HWND MakeMyList(HWND hWnd)
{
	HWND hList;
	DWORD dwStyle;
	LONG style;
	
	/* ���X�g�r���[����� */
	hList = CreateWindowEx(0,WC_LISTVIEW,"",
		WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_EX_CLIENTEDGE | LVS_NOSORTHEADER ,0,0,0,0,
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

	lvcol.cx = 130;
	lvcol.pszText = "�t�@�C����";
	lvcol.iSubItem = 1;
	ListView_InsertColumn(hList,1,&lvcol);
	
	lvcol.cx = 90;
	lvcol.pszText = "�T�C�Y";
	lvcol.iSubItem = 2;
	ListView_InsertColumn(hList,2,&lvcol);

	lvcol.cx = 130;
	lvcol.pszText = "�X�V����";
	lvcol.iSubItem = 3;
	ListView_InsertColumn(hList,3,&lvcol);
	
	return;
}

/*
	���z�t���b�s�[�f�B�X�N�t�@�C���I���_�C�A���O���o���B
	����
		�Ȃ�
	�߂�l
		�Ȃ�
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

	if (GetOpenFileName(&ofn)){
		openSavFile(GotFileName);
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

/* �t�@�C���]�����̃G���[���b�Z�[�W�\�� */
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

	// �I�����ꂽ�t�@�C���������āA�t�@�C����W�J���Ă����B
	count = ListView_GetItemCount(hList);
	for (i = 0;i < count;i++){
		// ���X�g�r���[�̑I����Ԃ��擾����B
		state = ListView_GetItemState(hList,i,LVIS_SELECTED);
		if (state){
			extractFile(buf,i);
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

	if (result == IDYES){
		deleteSelectedFiles(hList);

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