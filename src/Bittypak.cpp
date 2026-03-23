#include "..\\include\\Bittypak.h"
#include "..\\include\\RegistryUtility.h"
#define LAST_ITEM_INDEX         100000
#define POPUP_CLASS_NAME		L"InputPopup"
#define DEFAULT_PLAYLIST_NAME   L"Default Playlist"
#define OFN_FILTER_NAME         L"Media Files\0*.mp3;*.wav\0All Files(*.*)\0*.*\0";
#define INPUT_POPUP_TEMPLATE1   L"Enter a name for the file to save."
#define INPUT_POPUP_TEMPLATE2   L"Enter a name for the playlist you want to create"
#define INPUT_POPUP_TEMPLATE3   L"Enter the timer using the \"MM:SS\" format."

LRESULT CALLBACK InputPopupWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

typedef struct _INPUT_POPUP_DATA {
    WCHAR* pOut;
    const WCHAR* Title;
    const WCHAR* Prompt;
    int Mode;
} INPUT_POPUP_DATA;

HRESULT Initialize()
{
    HRESULT hr = CoInitializeEx(
            NULL,
            COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
            );

    if(FAILED(hr))
    {
        return hr;
    }

    hr = MFStartup(MF_VERSION);
    if(FAILED(hr))
    { 
        return hr;
    }

    return S_OK;
}

void Cleanup()
{
    MFShutdown();
    CoUninitialize();
}

POINT GetMonitorCenter(HWND hWnd){
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(hMonitor, &mi);

    POINT Center = { 
        (mi.rcWork.left + mi.rcWork.right) / 2,
        (mi.rcWork.top + mi.rcWork.bottom) / 2
    };

    return Center;
}

SIZE GetScaledWindowSize(HWND hWnd)
{
    RECT wrt;
    GetWindowRect(hWnd, &wrt);

    int Width = wrt.right - wrt.left;
    int Height = wrt.bottom - wrt.top;

    UINT DPI = 96;
    SIZE Scaled = {0, 0};
    float ScaleFactor = 0.f;

    HMODULE hUser32 = LoadLibrary(TEXT("User32.dll"));
    if(hUser32)
    {
        typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
        GetDpiForWindowFunc pGetDpiForWindow = (GetDpiForWindowFunc)GetProcAddress(hUser32, "GetDpiForWindow");

        if(pGetDpiForWindow)
        {
            DPI = pGetDpiForWindow(hWnd);
        }

        FreeLibrary(hUser32);
    }

    if(DPI != 0)
    {
        ScaleFactor = DPI / 96.0f;
        Scaled.cx = (int)(Width  * ScaleFactor);
        Scaled.cy = (int)(Height * ScaleFactor);
    }

    return Scaled;
}

BOOL SetWindowCenter(HWND hParent, HWND hChild)
{
    if(hChild == NULL)
    {
        return FALSE;
    }

    POINT Center = GetMonitorCenter(hParent);
    SIZE WindowSize = GetScaledWindowSize(hChild);

    int Left = Center.x - (WindowSize.cx / 2);
    int Top  = Center.y - (WindowSize.cy / 2);

    return SetWindowPos(hChild,
            NULL,
            Left, Top, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE
            );
}

std::mt19937& GetRandomEngine() {
    static std::random_device RandomDevice;
    static std::mt19937 Generator(RandomDevice());
    return Generator;
}

int GetRandomInt(int Min, int Max){
    std::uniform_int_distribution<> Dist(Min, Max);
    return Dist(GetRandomEngine());
}

void OpenFiles(HWND hWnd)
{
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    OPENFILENAME OFN;
    WCHAR lpstrFile[0x8000] = L"";
    WCHAR lpszName[MAX_PATH];
    WCHAR lpszDir[MAX_PATH];
    WCHAR lpszTemp[MAX_PATH];
    WCHAR* ptr;

    memset(&OFN, 0, sizeof(OFN));
    OFN.lStructSize = sizeof(OFN);
    OFN.hwndOwner = hWnd;
    OFN.lpstrFile = lpstrFile;
    OFN.lpstrFilter = OFN_FILTER_NAME;
    OFN.nMaxFile = 0x300;
    OFN.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT;

    if(GetOpenFileName(&OFN) != 0)
    {
        ptr = lpstrFile;
        wcscpy(lpszName, ptr);

        ptr += wcslen(lpszName) + 1;
        if(*ptr != 0)
        {
            wcscpy(lpszDir, lpszName);

            while(*ptr)
            {
                wcscpy(lpszName, ptr);
                wsprintf(lpszTemp, L"%s\\%s", lpszDir, lpszName);
                AppendFile(hListView, lpszTemp);
                ptr += wcslen(lpszName) + 1;
            }
        }
        else
        {
            AppendFile(hListView, lpstrFile);
        }

        HWND hButton = GetDlgItem(hWnd, IDC_BTNSTOP + 1);
        SendMessage(hButton, CBM_SETSTATE, DOWN, (LPARAM)0);
        SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNSTOP + 1, PRESSED), (LPARAM)hButton);

        SavePlaylist(hWnd);
    }
    else
    {
        if(CommDlgExtendedError() == FNERR_BUFFERTOOSMALL)
        {
            MessageBox(HWND_DESKTOP, L"선택한 파일이 너무 많습니다.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void AppendFile(HWND hListView, WCHAR* Path)
{
    LVITEM LI = {0};
    WCHAR FileName[MAX_PATH];
    WCHAR* Param;

    LI.mask = LVIF_TEXT | LVIF_PARAM;
    LI.iSubItem = 0;
    LI.iItem = LAST_ITEM_INDEX;
    _wsplitpath(Path, NULL, NULL, FileName, NULL);

    LI.pszText = FileName;
    Param = (WCHAR*)malloc(sizeof(WCHAR) * (wcslen(Path) + 1));

    wcscpy(Param, Path);
    LI.lParam = (LPARAM)Param;
    ListView_InsertItem(hListView, &LI);
}

void TraverseDirTree(HWND hWnd, WCHAR* Path)
{
    WIN32_FIND_DATA wfd;

    WCHAR CopyPath[MAX_PATH];
    wsprintf(CopyPath, L"%s\\*", Path);

    HANDLE hFind = FindFirstFile(CopyPath, &wfd);
    if(hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do{
        if(wcscmp(wfd.cFileName, L".") == 0 
                || wcscmp(wfd.cFileName, L"..") == 0)
        {
            continue;
        }

        WCHAR FullPath[MAX_PATH];
        wsprintf(FullPath, L"%s\\%s", Path, wfd.cFileName);

        if(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            TraverseDirTree(hWnd, FullPath);
        }
        else
        {
            if(IsAudioFile(FullPath))
            {
                AppendFile(GetDlgItem(hWnd, IDC_LVFIRST), FullPath);
            }
        }
    }while(FindNextFile(hFind, &wfd));

    FindClose(hFind);
}

BOOL IsAudioFile(const WCHAR* Path)
{
    const WCHAR* Extension = PathFindExtension(Path);

    if(Extension != NULL)
    { 
        if(_wcsicmp(Extension, L".wav") == 0 
                || _wcsicmp(Extension, L".mp3") == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

// 탐색 동작 확장할 생각 있으면 추가
BOOL MatchPattern(const WCHAR* Path, const WCHAR* Pattern) {
    if(*Pattern == 0)
    { 
        return *Path == 0;
    }

    if(*Pattern == '*')
    {
        while(*Pattern == '*')
        { 
            Pattern++;
        }
        
        if(*Pattern == 0)
        { 
            return TRUE;
        }

        while(*Path)
        {
            if(MatchPattern(Path, Pattern))
            {
                return TRUE;
            }
            
            Path++;
        }

        return FALSE;
    }

    if(*Pattern == '?')
    {
        return *Path ? MatchPattern(Path + 1, Pattern + 1) : FALSE;
    }

    if(*Path == *Pattern)
    {
        return MatchPattern(Path + 1, Pattern + 1);
    }

    return FALSE;
}

BOOL WriteWavHeader(HANDLE hFile, DWORD DataSize, WORD Channels, DWORD SampleRate, WORD BitsPerSample)
{
    WAVHEADER Header = { 0 };

    memcpy(Header.ChunkID, "RIFF", 4);
    Header.ChunkSize = 36 + DataSize;
    memcpy(Header.Format, "WAVE", 4);

    memcpy(Header.Subchunk1ID, "fmt ", 4);
    Header.Subchunk1Size = 16;
    Header.AudioFormat = 1;
    Header.NumChannels = Channels;
    Header.SampleRate = SampleRate;
    Header.AvgByteRate = SampleRate * Channels * BitsPerSample / 8;
    Header.BlockAlign = Channels * BitsPerSample / 8;
    Header.BitsPerSample = BitsPerSample;

    memcpy(Header.Subchunk2ID, "data", 4);
    Header.Subchunk2Size = DataSize;

    SetFilePointer(
            hFile,
            0,
            NULL,
            FILE_BEGIN
            );

    DWORD Written = 0;
    BOOL OK = WriteFile(
            hFile,
            &Header,
            sizeof(Header),
            &Written,
            NULL
            );

    return OK && Written == sizeof(Header);
}

BOOL ShowInputPopup(HWND hParent, WCHAR* Out, int MaxLength, int iMode){
    WNDCLASS twc;
    BOOL bAlreadyRegister = GetClassInfo(GetModuleHandle(NULL), POPUP_CLASS_NAME, &twc);

    if(!bAlreadyRegister)
    {
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = InputPopupWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = POPUP_CLASS_NAME;
        RegisterClass(&wc);
    }

    INPUT_POPUP_DATA data = { 0 };

    data.pOut = Out;
    data.Mode = iMode;

    switch(iMode){
        case 0:
            data.Title  = L"녹음 파일 저장";
            data.Prompt = INPUT_POPUP_TEMPLATE1;
            break;

        case 1:
            data.Title  = L"플레이리스트 이름 설정";
            data.Prompt = INPUT_POPUP_TEMPLATE2;
            break;

        case 2:
            data.Title  = L"타이머 설정";
            data.Prompt = INPUT_POPUP_TEMPLATE3;
            break;
    }

    HWND hPopup = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        POPUP_CLASS_NAME,
        data.Title,
        WS_POPUP | WS_BORDER | WS_CAPTION,
        100, 100, 380, 150,
        hParent, NULL, GetModuleHandle(NULL),
        (LPVOID)&data
    );

    if(hPopup == NULL)
    { 
        return FALSE;
    }

    ShowWindow(hPopup, SW_SHOW);
    UpdateWindow(hPopup);

    RECT srt;
    SetWindowCenter(hParent, hPopup);
    EnableWindow(hParent, FALSE);

    MSG msg;
    while(IsWindow(hPopup) &&
            GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    return wcslen(Out) > 0;
}

void CreatePlaylist(HWND hWnd, WCHAR* Name, BOOL bDelete /*= FALSE */){
    if(Name == NULL || wcslen(Name) == 0)
    {
        return;
    }

    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    WCHAR PlaylistName[MAX_PATH];
    wcscpy(PlaylistName, Name);

    if(bDelete)
    { 
        ListView_DeleteAllItems(hListView);
    }

    SendMessage(hComboBox, CB_INSERTSTRING, 0, (LPARAM)PlaylistName);
    SendMessage(hComboBox, CB_SETCURSEL, 0,0);

    WCHAR Path[MAX_PATH];
    int Count = SendMessage(hComboBox, CB_GETCOUNT, 0,0);

    if(Count > 20)
    {
        SendMessage(hComboBox, CB_GETLBTEXT, 20, (LPARAM)PlaylistName);
        SendMessage(hComboBox, CB_DELETESTRING, 20, 0);
        wsprintf(Path, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);
        SHDeleteKey(HKEY_CURRENT_USER, Path);
    }
}

BOOL DestroyPlaylist(HWND hWnd)
{
    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    WCHAR PlaylistName[MAX_PATH];
    int Select = SendMessage(hComboBox, CB_GETCURSEL, 0,0);
    SendMessage(hComboBox, CB_GETLBTEXT, Select, (LPARAM)PlaylistName);

    if(wcscmp(PlaylistName, DEFAULT_PLAYLIST_NAME) == 0)
    { 
        return FALSE;
    }

    ListView_DeleteAllItems(hListView);
    SendMessage(hComboBox, CB_DELETESTRING, Select, 0);
    SendMessage(hComboBox, CB_SETCURSEL, Select, 0);

    WCHAR Path[MAX_PATH];
    wsprintf(Path, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);
    SHDeleteKey(HKEY_CURRENT_USER, Path);

    HKEY hKey;
    LONG ret = RegOpenKeyEx(
            HKEY_CURRENT_USER,
            KEY_PATH_PLAYLIST,
            0,
            KEY_SET_VALUE,
            &hKey
            );
    
    if(ret != ERROR_SUCCESS)
    {
        return FALSE;
    }

    int nCount;
    DWORD dwType = ReadRegistryData(
            HKEY_CURRENT_USER,
            KEY_PATH_PLAYLIST,
            KEY_READ,
            L"PlaylistCount",
            &nCount,
            sizeof(LONG),
            0
            );

    WCHAR Name[0x40];
    WCHAR Data[MAX_PATH];
    for(int i=0; i<nCount; i++){
        wsprintf(Name, L"%d", i);
        ReadRegistryData(
                HKEY_CURRENT_USER,
                KEY_PATH_PLAYLIST,
                KEY_READ,
                Name,
                Data,
                sizeof(Data),
                (INT_PTR)L""
                );

        if(wcscmp(PlaylistName, Data) == 0)
        {
            break;
        }
    }

    RegDeleteValue(hKey, Name);
    RegCloseKey(hKey);

    return TRUE;
}

void SavePlaylist(HWND hWnd)
{
    WCHAR PlaylistName[MAX_PATH];
    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);

    int Select = SendMessage(hComboBox, CB_GETCURSEL, 0,0);
    SendMessage(hComboBox, CB_GETLBTEXT, Select, (LPARAM)PlaylistName);

    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);
    int nCount = ListView_GetItemCount(hListView);

    WCHAR PATH[MAX_PATH];
    wsprintf(PATH, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);

    DeleteRegistryValues(HKEY_CURRENT_USER, PATH);

    WriteRegistryData(
            HKEY_CURRENT_USER,
            PATH,
            KEY_WRITE,
            L"Count",
            REG_DWORD,
            &nCount,
            sizeof(LONG)
            );

    LVITEM LI;
    WCHAR Name[0x40];
    for(int i=0; i<nCount; i++)
    {
        LI.mask = LVIF_PARAM;
        LI.iItem = i;
        ListView_GetItem(hListView, &LI);
        wsprintf(Name, L"%d", i);
        WriteRegistryData(
                HKEY_CURRENT_USER,
                PATH,
                KEY_WRITE,
                Name,
                REG_SZ,
                (WCHAR*)LI.lParam,
                sizeof(WCHAR) * (wcslen((WCHAR*)LI.lParam) + 1)
                );
    }
}

void LoadPlaylist(HWND hWnd)
{
    WCHAR PlaylistName[MAX_PATH];

    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);
    ListView_DeleteAllItems(hListView);

    int Select = SendMessage(hComboBox, CB_GETCURSEL, 0,0);
    SendMessage(hComboBox, CB_GETLBTEXT, Select, (LPARAM)PlaylistName);

    WCHAR PATH[MAX_PATH];
    wsprintf(PATH, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);

    int nCount;
    DWORD dwType = ReadRegistryData(
            HKEY_CURRENT_USER,
            PATH,
            KEY_READ,
            L"Count",
            &nCount,
            sizeof(LONG),
            0
            );

    WCHAR Name[0x40];
    WCHAR Data[MAX_PATH];

    for(int i=0; i<nCount; i++){
        wsprintf(Name, L"%d", i);
        dwType = ReadRegistryData(
                HKEY_CURRENT_USER,
                PATH,
                KEY_READ,
                Name,
                Data,
                sizeof(Data),
                (INT_PTR)L""
                );

        if(dwType != REG_NONE)
        {
            AppendFile(hListView, Data);
        }
    }
}

void SavePosition(HWND hWnd, HKEY hKey, LPCWSTR hSubKey, int ExtendSize)
{
    WINDOWPLACEMENT WindowPlacement = {
        .length = sizeof(WINDOWPLACEMENT),
    };

    GetWindowPlacement(hWnd, &WindowPlacement);
    WindowPlacement.rcNormalPosition.bottom -= ExtendSize;

    WriteRegistryData(
            hKey,
            hSubKey,
            KEY_WRITE,
            L"CurrentState",
            REG_DWORD,
            &WindowPlacement.showCmd,
            sizeof(UINT)
            );

    WriteRegistryData(
            hKey,
            hSubKey,
            KEY_WRITE,
            L"Left",
            REG_DWORD,
            &WindowPlacement.rcNormalPosition.left,
            sizeof(LONG)
            );

    WriteRegistryData(
            hKey,
            hSubKey,
            KEY_WRITE,
            L"Top",
            REG_DWORD,
            &WindowPlacement.rcNormalPosition.top,
            sizeof(LONG)
            );

    WriteRegistryData(
            hKey, 
            hSubKey,
            KEY_WRITE,
            L"Right",
            REG_DWORD,
            &WindowPlacement.rcNormalPosition.right, 
            sizeof(LONG)
            );

    WriteRegistryData(
            hKey,
            hSubKey,
            KEY_WRITE,
            L"Bottom", 
            REG_DWORD,
            &WindowPlacement.rcNormalPosition.bottom,
            sizeof(LONG)
            );
}

void LoadPosition(HWND hWnd, HKEY hKey, LPCWSTR hSubKey)
{
    WINDOWPLACEMENT WindowPlacement = {
        .length = sizeof(WINDOWPLACEMENT),
        .flags = 0,
    };

    DWORD dwType;
    dwType = ReadRegistryData(
            hKey,
            hSubKey,
            KEY_READ,
            L"CurrentState",
            &WindowPlacement.showCmd,
            sizeof(UINT),
            SW_SHOW
            );

    dwType = ReadRegistryData(
            hKey,
            hSubKey,
            KEY_READ,
            L"Left",
            &WindowPlacement.rcNormalPosition.left,
            sizeof(LONG),
            30
            );

    dwType = ReadRegistryData(
            hKey, 
            hSubKey,
            KEY_READ,
            L"Top",
            &WindowPlacement.rcNormalPosition.top,
            sizeof(LONG),
            30
            );

    dwType = ReadRegistryData(
            hKey,
            hSubKey,
            KEY_READ, 
            L"Right",
            &WindowPlacement.rcNormalPosition.right,
            sizeof(LONG),
            DEFAULT_MAINWINDOW_WIDTH
            );

    dwType = ReadRegistryData(
            hKey,
            hSubKey,
            KEY_READ, 
            L"Bottom",
            &WindowPlacement.rcNormalPosition.bottom,
            sizeof(LONG), 
            DEFAULT_MAINWINDOW_HEIGHT
            );

    if(WindowPlacement.showCmd == SW_SHOWMINIMIZED)
    {
        WindowPlacement.showCmd = SW_RESTORE;
    }

    // 메인 윈도우에서 최대,최소 크기 설정하므로 0으로 초기화
    WindowPlacement.ptMinPosition.x = WindowPlacement.ptMinPosition.y = 0;
    WindowPlacement.ptMaxPosition.x = WindowPlacement.ptMaxPosition.y = 0;
    SetWindowPlacement(hWnd, &WindowPlacement);
}

void PlaySelectedItem(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer **pPlayer, WCHAR* Return, BOOL bPaused){
    if(pCallback == NULL)
    {
        return;
    }

    HWND hButton = GetDlgItem(hWnd, IDC_BTNSTOP + 1);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);
    HWND hVolume = GetDlgItem(hWnd, IDC_SCRLFIRST + 1);

    int nCount = ListView_GetItemCount(hListView);
    int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if(SelectItem == -1 && nCount == 0)
    { 
        SendMessage(hButton, CBM_SETSTATE, UP, (LPARAM)0);
        return;
    }

    if(SelectItem == -1 && nCount != 0)
    {
        SelectItem = 0;
    }

    LVITEM LI = { 0 };
    LI.mask = LVIF_PARAM;
    LI.iItem = SelectItem;
    ListView_GetItem(hListView, &LI);

    WCHAR* Path = (WCHAR*)LI.lParam;
    wcscpy(Return, (WCHAR*)LI.lParam);

    if(Path == NULL)
    { 
        return;
    }

    if(*pPlayer)
    {
        MFP_MEDIAPLAYER_STATE State;
        State = pCallback->GetCurrentState();

        if(State == MFP_MEDIAPLAYER_STATE_PLAYING && bPaused == TRUE)
        {
            (*pPlayer)->Pause();
        }
        else if(State == MFP_MEDIAPLAYER_STATE_PAUSED && bPaused == FALSE)
        {
            (*pPlayer)->Play();
        }

        return;
    }

    if(SUCCEEDED(MFPCreateMediaPlayer(Path, FALSE, 0, pCallback, hWnd, pPlayer)))
    {
        pCallback->SetPlayer(*pPlayer);

        int CurrentPosition = SendMessage(hVolume, CSM_GETPOSITION, 0,0);
        float fUserVolume = (float)CurrentPosition / 255.f;
        (*pPlayer)->SetVolume(fUserVolume);
        (*pPlayer)->Play();
    }
}

void PlayNextOrPrev(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer** pPlayer, BOOL bLoop, BOOL bRandom, BOOL bNext)
{
    if(*pPlayer){
        (*pPlayer)->Stop();
        (*pPlayer)->Shutdown();
        (*pPlayer)->Release();
        *pPlayer = NULL;
    }

    HWND hButton = GetDlgItem(hWnd, IDC_BTNSTOP + 1);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    int nCount = ListView_GetItemCount(hListView);
    int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if(SelectItem == -1 && nCount != 0)
    { 
        SelectItem = 0;
    }

    if(SelectItem != -1)
    {
        if(bRandom)
        {
            int NewItem;
            do{
                NewItem = GetRandomInt(0, nCount);
            }while(nCount > 1 && NewItem == SelectItem);

            SelectItem = NewItem;
        }
        else if(bLoop)
        {
            SelectItem = SelectItem;
        }
        else
        {
            if(bNext)
            {
                SelectItem = (SelectItem + 1) % nCount;
            }
            else
            {
                SelectItem = (SelectItem - 1 + nCount) % nCount;
            }
        }
    }

    ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(hListView, SelectItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    SendMessage(hButton, CBM_SETSTATE, DOWN, (LPARAM)0);
    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNSTOP + 1, PRESSED), (LPARAM)hButton);
}

LRESULT CALLBACK InputPopupWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam){
    static WCHAR Title[MAX_PATH];
    LPCREATESTRUCT cs;
    INPUT_POPUP_DATA *pData;

    switch(iMessage){
        case WM_CREATE:
            cs = (LPCREATESTRUCT)lParam;
            pData = (INPUT_POPUP_DATA*)cs->lpCreateParams;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pData);

            wcscpy(Title, pData->Title);
            CreateWindow(
                    L"static", 
                    pData->Prompt, 
                    WS_CHILD | WS_VISIBLE,
                    10, 10, 360, 20, 
                    hWnd, 
                    (HMENU)2001,
                    NULL, 
                    NULL
                    );

            CreateWindow(
                    L"edit",
                    L"",
                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                    10, 40, 360, 24,
                    hWnd,
                    (HMENU)1001,
                    NULL, 
                    NULL
                    );

            CreateWindow(
                    L"button",
                    L"확인",
                    WS_CHILD | WS_VISIBLE,
                    85, 85, 80, 24, 
                    hWnd,
                    (HMENU)1002,
                    NULL, 
                    NULL
                    );

            CreateWindow(
                    L"button",
                    L"취소",
                    WS_CHILD | WS_VISIBLE,
                    190, 85, 80, 24, 
                    hWnd, 
                    (HMENU)1003, 
                    NULL,
                    NULL
                    );
            return 0;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case 1002:
                {
                    pData = (INPUT_POPUP_DATA*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

                    HWND hEdit = GetDlgItem(hWnd, 1001);
                    GetWindowText(hEdit, pData->pOut, MAX_PATH);

                    if(pData->Mode == 0)
                    {
                        wcscat(pData->pOut, L".wav");
                    }

                    DestroyWindow(hWnd);
                }
                break;

                case 1003:
                {
                    DestroyWindow(hWnd);
                }
                break;

            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }

    return DefWindowProc(hWnd, iMessage, wParam, lParam);
}
