// #define _DEBUG
#include "resource.h"
#define CLASS_NAME					L"Bittypak"
#define INPUT_POPUP_CLASS_NAME		L"InputPopup"
#define IDC_CBFIRST					0x500
#define IDC_LVFIRST					0x600
#define IDC_STFIRST					0x800
#define IDM_ITEM_DELETE				0x900
#define IDM_CREATE_PLAYLIST         0x901
#define IDM_DELETE_PLAYLIST			0x902
#define IDM_RANDOM_PLAY             0x903
#define IDM_LOOP_PLAY               0x904
#define TEMPFILENAME				L"IsRecordingTempFile_Dont_Delete.wav"
#define KEY_PATH_POSITION			L"Software\\Bittypak\\InitInfo\\LastPosition"
#define KEY_PATH_PLAYLIST           L"Software\\Bittypak\\InitInfo\\Playlist"
#define INPUT_POPUP_TEMPLATE1       L"파일 이름을 입력하세요."
#define INPUT_POPUP_TEMPLATE2       L"플레이리스트 이름을 입력하세요."

#define DEFAULT_MAINWINDOW_WIDTH	400
#define DEFAULT_MAINWINDOW_HEIGHT	200

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define GET_X_LPARAM(pt) ((int)(short)LOWORD(pt))
#define GET_Y_LPARAM(pt) ((int)(short)HIWORD(pt))

#define WM_PLAYNEXT					WM_APP + 1
#define WM_NEWPOSITION				WM_APP + 2

#define FFT_SIZE 1024
#define BINS 160
#define PCM_BITS 16

// volatile 키워드를 사용하면 변수의 값을 레지스터나 캐시에 저장해 속도를 높이는 최적화를 막는다.
// 즉, 캐싱이나 컴파일러에 의한 최적화를 막아 항상 메모리에서 직접 최신 값을 읽도록 한다.
// 이는 단순히 읽고 쓰는 용도의 전역 변수를 멀티 스레드 환경에서 사용하고자 할 때 유용하다.
class PlayerCallback;
class DeviceCallback;

HANDLE hRecordStopEvent = NULL;
HANDLE hSpectrumStopEvent = NULL;
HANDLE hDeviceChangedEvent = NULL;
DWORD WINAPI RecordThread(LPVOID lParam);
DWORD WINAPI SpectrumThread(LPVOID lParam);
static volatile double Spectrum[BINS];
static volatile BOOL bSpectrumReady = FALSE;

HRESULT Initialize();
void Cleanup();
void OpenFiles(HWND hWnd);
void AppendFile(HWND hListView, WCHAR* Path);
void PlaySelectedItem(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer **pPlayer, WCHAR* Return, BOOL bPaused = FALSE);
void PlayNextOrPrev(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer** pPlayer, BOOL bLoop, BOOL bRandom, BOOL bNext);
BOOL WriteWavHeader(HANDLE hFile, DWORD DataSize, WORD Channels, DWORD SampleRate, WORD BitsPerSample);
BOOL ShowInputPopup(HWND hParent, WCHAR* Out, int MaxLength, int iMode);
POINT GetMonitorCenter(HWND hWnd);
SIZE GetScaledWindowSize(HWND hWnd);
BOOL SetWindowCenter(HWND hParent, HWND hChild);
void TraverseDirTree(HWND hWnd, WCHAR* Path);
BOOL IsAudioFile(const WCHAR* Path);
BOOL MatchPattern(const WCHAR* Path, const WCHAR* Pattern);
std::mt19937& GetRandomEngine();
int GetRandomInt(int Min, int Max);
void CreatePlaylist(HWND hWnd, WCHAR* Name, BOOL bDelete = FALSE);
BOOL DestroyPlaylist(HWND hWnd);
void SavePlaylist(HWND hWnd);
void LoadPlaylist(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputPopupWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

// 장치 변경 콜백
class DeviceCallback : public IMMNotificationClient {
    private:
        LONG RefCount;
        HANDLE hDeviceChangedEvent;
    public:
        DeviceCallback(HANDLE hEvent) : RefCount(1), hDeviceChangedEvent(hEvent) {;}
        virtual ~DeviceCallback() {;}

        virtual ULONG __stdcall AddRef(){
            return InterlockedIncrement(&RefCount);
        }
        virtual ULONG __stdcall Release(){
            ULONG Count = InterlockedDecrement(&RefCount);
            if(Count == 0){ delete this; }
            return Count;
        }
        virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvInterface){
            if(ppvInterface == NULL){ return E_POINTER; }

            if(riid == IID_IUnknown || riid == IID_IMMNotificationClient){
                *ppvInterface = (IMMNotificationClient*)(this);
                AddRef();
                return S_OK;
            }

            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }

        virtual HRESULT __stdcall OnDefaultDeviceChanged(EDataFlow Flow, ERole Role, LPCWSTR lpwszDeviceID){
            if(Flow == eRender && Role == eConsole){
                SetEvent(hDeviceChangedEvent);
            }
            return S_OK;
        }
        virtual HRESULT __stdcall OnDeviceAdded(LPCWSTR lpwszDeviceID){

        }
        virtual HRESULT __stdcall OnDeviceRemoved(LPCWSTR lpwszDeviceID){

        }
        virtual HRESULT __stdcall OnDeviceStateChanged(LPCWSTR lpwszDeviceID, DWORD dwNewState){

        }
        virtual HRESULT __stdcall OnPropertyValueChanged(LPCWSTR lpwszDeviceID, const PROPERTYKEY Key){

        }
};
static DeviceCallback* pDeviceNotifier = NULL;

// 콜백 클래스
class PlayerCallback : public IMFPMediaPlayerCallback{
    LONG RefCount = 1;
    HWND hWnd;
    IMFPMediaPlayer* pPlayer = NULL;
    WCHAR Debug[0x100];
    MFP_MEDIAPLAYER_STATE CurrentState = MFP_MEDIAPLAYER_STATE_EMPTY;

    public:
    virtual ULONG __stdcall AddRef() { return InterlockedIncrement(&RefCount); }
    virtual ULONG __stdcall Release() {
        ULONG Count = InterlockedDecrement(&RefCount);
        if(Count == 0){ delete this; }
        return Count;
    }

    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppv){
        if(riid == __uuidof(IUnknown) || riid == __uuidof(IMFPMediaPlayerCallback)){
            *ppv = (IMFPMediaPlayerCallback*)this;
            AddRef();
            return S_OK;
        }

        *ppv = NULL;
        return E_NOINTERFACE;
    }

    virtual void __stdcall OnMediaPlayerEvent(MFP_EVENT_HEADER* pEventHeader){
        if(pPlayer == NULL){ return; }

        switch(pEventHeader->eEventType){
            case MFP_EVENT_TYPE_PLAY:
                pPlayer->GetState(&CurrentState);
                KillTimer(hWnd, 1);
                SetTimer(hWnd, 1, 50, NULL);
                break;

            case MFP_EVENT_TYPE_PAUSE:
                pPlayer->GetState(&CurrentState);
                KillTimer(hWnd, 1);
                break;

            case MFP_EVENT_TYPE_STOP:
                pPlayer->GetState(&CurrentState);
                KillTimer(hWnd, 1);
                break;

            case MFP_EVENT_TYPE_POSITION_SET:
                pPlayer->GetState(&CurrentState);
                PostMessage(hWnd, WM_NEWPOSITION, 0, 0);
                break;

            case MFP_EVENT_TYPE_RATE_SET:
                break;

            case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
                pPlayer->GetState(&CurrentState);
                break;

            case MFP_EVENT_TYPE_MEDIAITEM_CLEARED:
                // EMPTY
                pPlayer->GetState(&CurrentState);
                break;

            case MFP_EVENT_TYPE_MEDIAITEM_SET:
                if(pPlayer){
                    MFP_MEDIAPLAYER_STATE State;
                    HRESULT hr = pPlayer->GetState(&State);

                    if(SUCCEEDED(hr)){
                        switch(State){
                            case MFP_MEDIAPLAYER_STATE_EMPTY:
                                break;

                            case MFP_MEDIAPLAYER_STATE_STOPPED:
                                break;

                            case MFP_MEDIAPLAYER_STATE_PLAYING:
                                break;

                            case MFP_MEDIAPLAYER_STATE_PAUSED:
                                break;

                            default:
                                wprintf(L"플레이어 상태: 알 수 없음\n");
                                break;
                        }
                    }else{
                        wsprintf(Debug, L"GetState 호출 실패: 0x%08X\n", hr);
                        MessageBox(hWnd, Debug, L"Error", MB_OK | MB_ICONERROR);
                    }
                }
                break;

            case MFP_EVENT_TYPE_FRAME_STEP:
                break;

            case MFP_EVENT_TYPE_MF:
                break;

            case MFP_EVENT_TYPE_ERROR:
                pPlayer->GetState(&CurrentState);
                wsprintf(Debug, L"오류 발생: 0x%08X\n", pEventHeader->hrEvent);
                MessageBox(hWnd, Debug, L"Error", MB_OK | MB_ICONERROR);
                break;

            case MFP_EVENT_TYPE_PLAYBACK_ENDED:
                pPlayer->GetState(&CurrentState);
                PostMessage(hWnd, WM_PLAYNEXT, 0, 0);
                break;

            case MFP_EVENT_TYPE_ACQUIRE_USER_CREDENTIAL:
                break;
        }
    }

    void SetPlayer(IMFPMediaPlayer* pNewPlayer) {
        if(pPlayer){
            pPlayer->Release();
            pPlayer = NULL;
        }

        if(pNewPlayer){
            pNewPlayer->AddRef();
            pPlayer = pNewPlayer;
        }
    }

    MFP_MEDIAPLAYER_STATE GetCurrentState() {
        return CurrentState;
    }

    public:
    PlayerCallback(HWND _hWnd) : hWnd(_hWnd) {;}
    ~PlayerCallback() {
        if(pPlayer){
            pPlayer->Release();
            pPlayer = NULL;
        }
    }
};

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow){
#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    printf("디버그 콘솔 활성화\n");
#endif
    HBRUSH hBkBrush = CreateSolidBrush(RGB(240, 250, 255));

    WNDCLASSEX wcex = {
        sizeof(wcex),
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0,0,
        hInst,
        LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)),
        LoadCursor(NULL, IDC_ARROW),
        hBkBrush,
        NULL,
        CLASS_NAME,
        (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON), LR_DEFAULTCOLOR)
    };
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindowEx(
            WS_EX_CLIENTEDGE,  //| WS_EX_ACCEPTFILES,
            CLASS_NAME,
            CLASS_NAME,
            WS_POPUP | WS_BORDER | WS_VISIBLE | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, DEFAULT_MAINWINDOW_WIDTH, DEFAULT_MAINWINDOW_HEIGHT,
            NULL,
            (HMENU)NULL,
            hInst,
            NULL
            );

    ShowWindow(hWnd, nCmdShow);

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hBkBrush);
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam){
    static HWND hBtns[BTN_COUNT], hSystemBtns[3], hComboBox, hListView, hProgress, hVolume;
    static int iWidth, iHeight, ButtonWidth, ButtonHeight, Padding;
    static const int TitleButtonWidth = GetSystemMetrics(SM_CXSIZE),
                 TitleButtonHeight = GetSystemMetrics(SM_CYSIZE),
                 Diameter = min(TitleButtonWidth, TitleButtonHeight),
                 BORDER = 12,
                 EDGE = BORDER / 2;
    static RECT rcClose, rcMinimize;
    static const WCHAR Description[0x10] = L"Playlist";
    static const WCHAR TimeSample[0x20] = L"[00:00:00 / 00:00:00]";
    static const int MaxCount = 10, ListBoxItemHeight = 24;
    static SIZE TextSize, TimeTextSize;
    static RECT rcBtnNewFile, ewrt, rtText;
    static BOOL bFirst = TRUE;
    static HBITMAP hBitmap;
    static WCHAR TimeLine[64];
    static int MinWidth;	// BTN_COUNT * ButtonWidth + BTN_COUNT * Padding;
    static int MinHeight;	// (rcClose.bottom + ButtonHeight + Padding * 2) * 2;
    enum tag_ButtonState ButtonState;

    HDC hdc, hMemDC;
    PAINTSTRUCT ps;
    RECT crt, srt, wrt, trt;
    HGDIOBJ hOld;
    BITMAP bmp;
    HPEN hPen, hOldPen;

    LVCOLUMN COL;

    LVITEM LI;
    LPNMHDR lphdr;
    LPNMITEMACTIVATE lpnia;
    LPNMLISTVIEW lpnlv;

    HRESULT hr;

    static IMFPMediaPlayer* pPlayer = NULL;
    static PlayerCallback* pCallback = NULL;
    static BOOL bSeeking = FALSE;
    WCHAR Debug[256];

    static HANDLE hRecordThread  = NULL;
    static HANDLE hSpectrumThread  = NULL;
    static DWORD dwThreadID[2];

    static RECT rcSpectrum;

    static BOOL bExtended;
    static BOOL bSpectrum;
    DWORD dwType;
    RECT DefaultRect;
    WINDOWPLACEMENT WindowPlacement;

    static LARGE_INTEGER StartTime, Frequency;
    static BOOL bOnWindow;
    static WCHAR CurrentItem[MAX_PATH];
    static BOOL bLoop, bRandom;

    int nCount;
    WCHAR Name[0x40];
    WCHAR PlaylistName[MAX_PATH];
    static HBRUSH hBkBrush;

    switch (iMessage){
        case WM_CREATE:
            {
                hr = Initialize();
                if(FAILED(hr)){ return -1; }

                hdc = GetDC(hWnd);
                GetTextExtentPoint32(hdc, Description, wcslen(Description), &TextSize);
                GetTextExtentPoint32(hdc, TimeSample, wcslen(TimeSample), &TimeTextSize);
                wcscpy(TimeLine, TimeSample);
                ReleaseDC(hWnd, hdc);

                hBtns[0] = CreateWindow(BTN_CLASS_NAME, NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | BS_PUSHBUTTON | PUSH, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_BTNFIRST), GetModuleHandle(NULL), NULL);
                hBtns[1] = CreateWindow(BTN_CLASS_NAME, NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | BS_PUSHBUTTON | CHECK, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_BTNFIRST + 1), GetModuleHandle(NULL), NULL);

                for (int i = 2; i < 5; i++) {
                    hBtns[i] = CreateWindow(BTN_CLASS_NAME, NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | BS_PUSHBUTTON | PUSH, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_BTNFIRST + i), GetModuleHandle(NULL), NULL);
                }

                hBtns[5] = CreateWindow(BTN_CLASS_NAME, NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | BS_PUSHBUTTON | CHECK, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_BTNFIRST + 5), GetModuleHandle(NULL), NULL);
                hBtns[6] = CreateWindow(BTN_CLASS_NAME, NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | BS_PUSHBUTTON | CHECK, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_BTNFIRST + 6), GetModuleHandle(NULL), NULL);
                hBtns[7] = CreateWindow(BTN_CLASS_NAME, NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | BS_PUSHBUTTON | CHECK, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_BTNFIRST + 7), GetModuleHandle(NULL), NULL);

                hProgress = CreateWindow(SCROLLBAR_CLASS_NAME, NULL, WS_CHILD | WS_VISIBLE | CSS_HORZ, 0,0,0,0, hWnd, (HMENU)(INT_PTR)(IDC_SCRLFIRST), GetModuleHandle(NULL), NULL);
                hVolume = CreateWindow(SCROLLBAR_CLASS_NAME, NULL, WS_CHILD | WS_VISIBLE | CSS_HORZ, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_SCRLFIRST + 1), GetModuleHandle(NULL), NULL);

                // 콤보 박스 컨트롤의 리스트 박스 컨트롤은 별도로 핸들을 구해야만 크기를 조정할 수 있다.
                // 일반적인 방법은 다음과 같이 CreateWindow로 컨트롤을 생성할 때 정적으로 크기를 초기화하는 것이다.
                hComboBox = CreateWindow(L"combobox", NULL, WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 100, 210, hWnd, (HMENU)(INT_PTR)(IDC_CBFIRST), GetModuleHandle(NULL), NULL);
                hListView = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_LVFIRST), GetModuleHandle(NULL), NULL);
                ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT);

                COL.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
                COL.fmt = LVCFMT_LEFT;
                COL.cx = 255;
                COL.pszText = (LPWSTR)L"파일명";
                COL.iSubItem = 0;
                ListView_InsertColumn(hListView, 0, &COL);

                pCallback = new PlayerCallback(hWnd);
                DragAcceptFiles(hWnd, TRUE);
                ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
                // WM_COPYGLOBALDATA: 본래 숨겨진 메시지로 내부적으로만 사용됨
                // 관리자 권한으로 실행된 프로세스는 다른 프로세스와 격리되므로 일반 권한의 탐색기에서 관리자 권한의 프로그램으로 Drop 동작시 통신에 필요
                ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
                bOnWindow = FALSE;
                bLoop = FALSE;
                bRandom = FALSE;

                hBkBrush = CreateSolidBrush(RGB(240, 250, 255));
            }
            return 0;

        case WM_LBUTTONDOWN:
            {
                POINT Mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                CopyRect(&srt, &rcClose);
                CopyRect(&trt, &rcMinimize);
                InflateRect(&srt, -2, -2);
                InflateRect(&trt, -2, -2);
                if (PtInRect(&srt, Mouse)) {
                    SendMessage(hWnd, WM_CLOSE, 0,0);
                }
                if (PtInRect(&trt, Mouse)) {
                    ShowWindow(hWnd, SW_MINIMIZE);
                }
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)){
                case IDC_BTNFIRST:
                    if(HIWORD(wParam) == PRESSED){
                        if(pPlayer){
                            MFP_MEDIAPLAYER_STATE State;
                            State = pCallback->GetCurrentState();
                            if(State == MFP_MEDIAPLAYER_STATE_PLAYING || State == MFP_MEDIAPLAYER_STATE_PAUSED){
                                pPlayer->Stop();
                                pPlayer->Shutdown();
                                pPlayer->Release();
                                pPlayer = NULL;
                                SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                                wcscpy(TimeLine, TimeSample);
                                InvalidateRect(hWnd, NULL, FALSE);
                            }
                        }
                        bOnWindow = FALSE;
                        wcscpy(TimeLine, TimeSample);
                        SendMessage(hProgress, CSM_SETPOSITION, (WPARAM)0, (LPARAM)0);
                        SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                        InvalidateRect(hWnd, NULL, FALSE);
                        KillTimer(hWnd, 3);
                    }
                    break;

                case IDC_BTNFIRST + 1:
                    // 리스트 뷰 활용 -> 선택한 항목 문자열 불러온 후 파일 재생
                    if(HIWORD(wParam) == PRESSED){
                        memset(CurrentItem, 0, sizeof(CurrentItem));
                        PlaySelectedItem(hWnd, pCallback, &pPlayer, CurrentItem);
                        if(!pPlayer){
                            bOnWindow = FALSE;
                            SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                            wcscpy(TimeLine, TimeSample);
                            InvalidateRect(hWnd, NULL, FALSE);

                            if(CurrentItem != NULL && wcscmp(CurrentItem, L"") != 0){
                                MessageBox(hWnd, CurrentItem, L"?", MB_OK);
                                if(IDYES == MessageBox(hWnd, L"파일을 찾을 수 없습니다.\r\n해당 항목을 삭제하시겠습니까?", L"Error", MB_YESNO | MB_ICONERROR)){
                                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_ITEM_DELETE, 0), (LPARAM)0);
                                }else{
                                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 3, PRESSED), (LPARAM)hBtns[3]);
                                }
                            }
                        }else{
                            bOnWindow = TRUE;
                            SetTimer(hWnd, 3, 30000, NULL);
                        }
                    }else{
                        memset(CurrentItem, 0, sizeof(CurrentItem));
                        PlaySelectedItem(hWnd, pCallback, &pPlayer, CurrentItem, TRUE);
                        bOnWindow = FALSE;
                        KillTimer(hWnd, 3);
                    }
                    break;

                case IDC_BTNFIRST + 2:
                    if(HIWORD(wParam) == PRESSED){
                        SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                        wcscpy(TimeLine, TimeSample);
                        InvalidateRect(hWnd, NULL, FALSE);
                        PlayNextOrPrev(hWnd, pCallback, &pPlayer, FALSE, bRandom, FALSE);
                    }
                    break;

                case IDC_BTNFIRST + 3:
                    if(HIWORD(wParam) == PRESSED){
                        SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                        wcscpy(TimeLine, TimeSample);
                        InvalidateRect(hWnd, NULL, FALSE);
                        PlayNextOrPrev(hWnd, pCallback, &pPlayer, FALSE, bRandom, TRUE);
                    }
                    break;

                case IDC_BTNFIRST + 4:
                    if(HIWORD(wParam) == PRESSED){
                        OpenFiles(hWnd);
                    }
                    break;

                case IDC_BTNFIRST + 5:
                    {
                        GetWindowRect(hWnd, &wrt);

                        int CurrentWidth = wrt.right - wrt.left;
                        int CurrentHeight = wrt.bottom - wrt.top;
                        int ExtendedHeight = MaxCount * ListBoxItemHeight;
                        int MaxAnimationScene = 8;
                        int Scene = ExtendedHeight / MaxAnimationScene;

                        if(HIWORD(wParam) == RELEASED){
                            bExtended = FALSE;
                            SetWindowPos(hComboBox, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
                            SetWindowPos(hListView, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);

                            for(int i = 1; i <= MaxAnimationScene; i++){
                                if(CurrentWidth >= MinWidth && (CurrentHeight - Scene * i) >= MinHeight){
                                    SetWindowPos(hWnd, NULL, 0, 0, CurrentWidth, CurrentHeight - Scene * i, SWP_NOMOVE | SWP_NOZORDER);
                                    InvalidateRect(hWnd, NULL, FALSE);
                                    Sleep(max(10, min(MaxAnimationScene * 10, (MaxAnimationScene - (i + 2)) * 10)));
                                }
                            }
                        }

                        if(HIWORD(wParam) == PRESSED){
                            bExtended = TRUE;
                            for(int i = 1; i <= MaxAnimationScene; i++){
                                SetWindowPos(hWnd, NULL, 0, 0, CurrentWidth, CurrentHeight + Scene * i, SWP_NOMOVE | SWP_NOZORDER);
                                InvalidateRect(hWnd, NULL, FALSE);
                                Sleep(max(10, min(MaxAnimationScene * 10, (MaxAnimationScene - (i + 2)) * 10)));
                            }

                            GetClientRect(hWnd, &crt);
                            GetWindowRect(hBtns[0], &wrt);
                            ScreenToClient(hWnd, (LPPOINT)&wrt);
                            ScreenToClient(hWnd, (LPPOINT)&wrt + 1);

                            SetWindowPos(hComboBox, NULL, rtText.right + Padding, rtText.top, crt.right - (rtText.right + Padding * 2), rtText.bottom - rtText.top, SWP_NOZORDER | SWP_SHOWWINDOW);
                            SetWindowPos(hListView, NULL, rtText.left, rtText.bottom + Padding, rtText.left + (rtText.right - rtText.left) + crt.right - (rtText.right + Padding * 2), crt.bottom - (wrt.top + ButtonHeight + Padding * 2 + (rtText.bottom - rtText.top)), SWP_NOZORDER | SWP_SHOWWINDOW);
                            ListView_SetColumnWidth(hListView, 0, rtText.left + (rtText.right - rtText.left) + crt.right - (rtText.right + Padding * 2));

                            GetWindowRect(hWnd, &ewrt);
                        }

                        if(hBitmap != NULL){
                            DeleteObject(hBitmap);
                            hBitmap = NULL;
                        }
                    }
                    break;

                case IDC_BTNFIRST + 6:
                    if(HIWORD(wParam) == PRESSED){
                        QueryPerformanceFrequency(&Frequency);
                        QueryPerformanceCounter(&StartTime);
                        SetTimer(hWnd, 2, 100, NULL);
                        hRecordStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                        hRecordThread = CreateThread(NULL, 0, RecordThread, (LPVOID)NULL, 0, &dwThreadID[0]);
                    }else{
                        SetEvent(hRecordStopEvent);
                        WaitForSingleObject(hRecordThread, INFINITE);
                        KillTimer(hWnd, 2);
                        CloseHandle(hRecordThread);
                        CloseHandle(hRecordStopEvent);
                        hRecordThread = hRecordStopEvent = NULL;

                        // 입력 윈도우 추가 후 FileName 전달 받고 
                        WCHAR FileName[MAX_PATH] = L"";
retry:
                        if(ShowInputPopup(hWnd, FileName, MAX_PATH, 0)){
                            if(GetFileAttributes(FileName) != INVALID_FILE_ATTRIBUTES){
                                int ret = MessageBox(HWND_DESKTOP, L"이미 같은 이름의 파일이 존재합니다.\n덮어쓰시겠습니까?", L"Warning", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
                                if(ret != IDYES){
                                    goto retry;
                                }
                            }

                            if(!MoveFileEx(TEMPFILENAME, FileName, MOVEFILE_REPLACE_EXISTING)){
                                // 취소 버튼을 누르거나 파일 이름이 한 글자도 입력되지 않은 경우
                                DWORD dwRet = GetLastError();
                                wsprintf(Debug, L"파일 저장에 실패했습니다.\r\n에러 코드: 0x%08X", dwRet);
                                MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
                            }
                        }else{
                            DWORD dwRet = GetFileAttributes(TEMPFILENAME);
                            if(dwRet != INVALID_FILE_ATTRIBUTES && !(dwRet & FILE_ATTRIBUTE_DIRECTORY)){
                                DeleteFile(TEMPFILENAME);
                            }
                        }
                        wcscpy(TimeLine, TimeSample);
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    break;

                case IDC_BTNFIRST + 7:
                    if(HIWORD(wParam) == PRESSED){
                        bSpectrum = TRUE;
                        hSpectrumStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                        hSpectrumThread = CreateThread(NULL, 0, SpectrumThread, (LPVOID)NULL, 0, &dwThreadID[1]);
                    }else{
                        bSpectrum = FALSE;
                        SetEvent(hSpectrumStopEvent);
                        WaitForSingleObject(hSpectrumThread, INFINITE);
                        CloseHandle(hSpectrumThread);
                        CloseHandle(hSpectrumStopEvent);
                        hSpectrumThread = hSpectrumStopEvent = NULL;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    break;

                case IDC_CBFIRST:
                    switch(HIWORD(wParam)){
                        case CBN_SELCHANGE:
                            if(pPlayer){
                                pPlayer->Stop();
                                pPlayer->Shutdown();
                                pPlayer->Release();
                                pPlayer = NULL;
                                SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                                wcscpy(TimeLine, TimeSample);
                                InvalidateRect(hWnd, NULL, FALSE);
                            }
                            LoadPlaylist(hWnd);
                            SendMessage(hBtns[1], CBM_SETSTATE, DOWN, (LPARAM)0);
                            SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hBtns[1]);
                            break;
                    }
                    break;

                case IDM_ITEM_DELETE:
                    {
                        // 특수 키 입력과 마우스 관련 이벤트를 리스트 뷰가 알아서 처리한다.
                        for(int i = ListView_GetItemCount(hListView) - 1; i>=0; i--){
                            if(!(ListView_GetItemState(hListView, i, LVIS_SELECTED) & LVIS_SELECTED)){ continue; }

                            memset(&LI, 0, sizeof(LI));
                            LI.mask = LVIF_PARAM;
                            LI.iItem = i;
                            LI.iSubItem = 0;

                            BOOL bCurrent = FALSE;
                            BOOL bCompare = FALSE;
                            BOOL bPlaying = FALSE;

                            if(ListView_GetItem(hListView, &LI)){
                                bCurrent = (wcscmp(CurrentItem, (WCHAR*)LI.lParam) == 0);
                            }

                            if(bCurrent && pPlayer && pCallback){
                                MFP_MEDIAPLAYER_STATE State = pCallback->GetCurrentState();
                                bPlaying = (State == MFP_MEDIAPLAYER_STATE_PLAYING);
                            }

                            if(bCurrent && bPlaying){
                                MessageBox(HWND_DESKTOP, L"실행중인 파일은 삭제할 수 없습니다.", L"Information", MB_OK | MB_ICONINFORMATION);
                                continue;
                            }

                            if(bCurrent && pPlayer){
                                pPlayer->Stop();
                                pPlayer->Shutdown();
                                pPlayer->Release();
                                pPlayer = NULL;
                                SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                                wcscpy(TimeLine, TimeSample);
                                InvalidateRect(hWnd, NULL, FALSE);
                            }

                            ListView_DeleteItem(hListView, i);
                        }
                        SavePlaylist(hWnd);
                    }
                    break;

                case IDM_CREATE_PLAYLIST:
                    {
                        if(ShowInputPopup(hWnd, PlaylistName, MAX_PATH, 1)){
                            SavePlaylist(hWnd);
                            CreatePlaylist(hWnd, PlaylistName, TRUE);
                        }
                    }
                    break;

                case IDM_DELETE_PLAYLIST:
                    if(DestroyPlaylist(hWnd)){
                        if(pPlayer){
                            pPlayer->Stop();
                            pPlayer->Shutdown();
                            pPlayer->Release();
                            pPlayer = NULL;
                            SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
                            wcscpy(TimeLine, TimeSample);
                            InvalidateRect(hWnd, NULL, FALSE);
                        }

                        LoadPlaylist(hWnd);
                        SendMessage(hBtns[1], CBM_SETSTATE, DOWN, (LPARAM)0);
                        SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hBtns[1]);
                    }
                    break;

                case IDM_RANDOM_PLAY:
                    bRandom = !bRandom;
                    if(bLoop){ bLoop = FALSE; }
                    break;

                case IDM_LOOP_PLAY:
                    bLoop = !bLoop;
                    if(bRandom){ bRandom = FALSE; }
                    break;
            }
            return 0;

        case WM_NCHITTEST:
            {

                POINT Mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
#ifdef _DEBUG
                printf("[WM_NCHITTEST] CursorPos: x = %d, y = %d\n", Mouse.x, Mouse.y);
#endif
                GetWindowRect(hWnd, &wrt);

                if(SendMessage(hBtns[5], CBM_GETSTATE, 0, 0) != DOWN){
                    SendMessage(hBtns[5], CBM_SETSTATE, NORMAL, (LPARAM)0);
                }

                if(Mouse.x >= wrt.left && Mouse.x < wrt.left + BORDER && Mouse.y >= wrt.top + EDGE && Mouse.y < wrt.bottom - EDGE){
                    return HTLEFT;
                }
                if(Mouse.x <= wrt.right && Mouse.x > wrt.right - BORDER && Mouse.y >= wrt.top + EDGE && Mouse.y < wrt.bottom - EDGE){
                    return HTRIGHT;
                }
                if(Mouse.y >= wrt.top && Mouse.y < wrt.top + BORDER && Mouse.x >= wrt.left + EDGE && Mouse.x < wrt.right - EDGE){
                    return HTTOP;
                }
                if(Mouse.y <= wrt.bottom && Mouse.y > wrt.bottom - BORDER && Mouse.x >= wrt.left + EDGE && Mouse.x < wrt.right - EDGE){
                    return HTBOTTOM;
                }
                if(Mouse.x >= wrt.left && Mouse.x < wrt.left + EDGE && Mouse.y >= wrt.top && Mouse.y < wrt.top + EDGE){
                    return HTTOPLEFT;
                }
                if(Mouse.x <= wrt.right && Mouse.x > wrt.right - EDGE && Mouse.y >= wrt.top && Mouse.y < wrt.top + EDGE){
                    return HTTOPRIGHT;
                }
                if(Mouse.x >= wrt.left && Mouse.x < wrt.left + EDGE && Mouse.y <= wrt.bottom && Mouse.y > wrt.bottom - EDGE){
                    return HTBOTTOMLEFT;
                }
                if(Mouse.x <= wrt.right && Mouse.x > wrt.right - EDGE && Mouse.y <= wrt.bottom && Mouse.y > wrt.bottom - EDGE){
                    return HTBOTTOMRIGHT;
                }
                if(Mouse.x >= wrt.left + BORDER && Mouse.x <= wrt.right - BORDER && Mouse.y >= wrt.top + BORDER && Mouse.y <= wrt.bottom - BORDER){
                    POINT LocalMouse = Mouse;
                    ScreenToClient(hWnd, &LocalMouse);

                    CopyRect(&srt, &rcClose);
                    CopyRect(&trt, &rcMinimize);
                    InflateRect(&srt, -2, -2);
                    InflateRect(&trt, -2, -2);
                    if (!PtInRect(&srt, LocalMouse) && !PtInRect(&trt, LocalMouse)) {
                        return HTCAPTION;
                    }
                }
            }
            break;

        case WM_WINDOWPOSCHANGING:
            {
                MONITORINFOEX miex;
                memset(&miex, 0, sizeof(miex));
                miex.cbSize = sizeof(miex);

                HMONITOR hCurrentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                GetMonitorInfo(hCurrentMonitor, &miex);

                LPWINDOWPOS lpwp = (LPWINDOWPOS)lParam;
                int SideSnap = 10;

                if (abs(lpwp->x - miex.rcMonitor.left) < SideSnap) {
                    lpwp->x = miex.rcMonitor.left;
                } else if (abs(lpwp->x + lpwp->cx - miex.rcMonitor.right) < SideSnap) {
                    lpwp->x = miex.rcMonitor.right - lpwp->cx;
                } 
                if (abs(lpwp->y - miex.rcMonitor.top) < SideSnap) {
                    lpwp->y = miex.rcMonitor.top;
                } else if (abs(lpwp->y + lpwp->cy - miex.rcMonitor.bottom) < SideSnap) {
                    lpwp->y = miex.rcMonitor.bottom - lpwp->cy;
                }
            }
            return 0;

        case WM_ACTIVATEAPP:
            if(bFirst){
                bFirst = !bFirst;

                SendMessage(hProgress, CSM_SETRANGE, (WPARAM)0, (LPARAM)1000);
                SendMessage(hProgress, CSM_SETTHUMBSIZE, (WPARAM)20, (LPARAM)0);
                SendMessage(hProgress, CSM_SETTHUMBCOLOR, (WPARAM)RGB(255, 255, 255), (LPARAM)0);
                SendMessage(hProgress, CSM_SETBKCOLOR, (WPARAM)RGB(240, 250, 255), (LPARAM)0);
                SendMessage(hProgress, CSM_SETGAP, (WPARAM)5, (LPARAM)0);

                SendMessage(hVolume, CSM_SETRANGE, (WPARAM)0, (LPARAM)255);
                SendMessage(hVolume, CSM_SETTHUMBSIZE, (WPARAM)10, (LPARAM)0);
                SendMessage(hVolume, CSM_SETTHUMBCOLOR, (WPARAM)RGB(255, 255, 255), (LPARAM)0);
                SendMessage(hVolume, CSM_SETBKCOLOR, (WPARAM)RGB(240, 250, 255), (LPARAM)0);
                SendMessage(hVolume, CSM_SETGAP, (WPARAM)5, (LPARAM)0);
                SendMessage(hVolume, CSM_SETPOSITION, (WPARAM)255, 0);

                WindowPlacement = {
                    .length = sizeof(WINDOWPLACEMENT),
                    .flags = 0,
                };
                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_READ, L"CurrentState", &WindowPlacement.showCmd, sizeof(UINT), SW_SHOW);
                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_READ, L"Left", &WindowPlacement.rcNormalPosition.left, sizeof(LONG), 30);
                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_READ, L"Top", &WindowPlacement.rcNormalPosition.top, sizeof(LONG), 30);
                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_READ, L"Right", &WindowPlacement.rcNormalPosition.right, sizeof(LONG), DEFAULT_MAINWINDOW_WIDTH);
                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_READ, L"Bottom", &WindowPlacement.rcNormalPosition.bottom, sizeof(LONG), DEFAULT_MAINWINDOW_HEIGHT);
                if(WindowPlacement.showCmd == SW_SHOWMINIMIZED){
                    WindowPlacement.showCmd = SW_RESTORE;
                }
                WindowPlacement.ptMinPosition.x = WindowPlacement.ptMinPosition.y = 0;
                WindowPlacement.ptMaxPosition.x = WindowPlacement.ptMaxPosition.y = 0;
                SetWindowPlacement(hWnd, &WindowPlacement);

                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_READ, L"bSpectrum", &bSpectrum, sizeof(LONG), TRUE);
                if(bSpectrum){
                    SendMessage(hBtns[7], CBM_SETSTATE, DOWN, (LPARAM)0);
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 7, PRESSED), (LPARAM)hBtns[7]);
                }

                dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, KEY_READ, L"PlaylistCount", &nCount, sizeof(LONG), 0);
                for(int i=0; i<nCount; i++){
                    wsprintf(Name, L"%d", i);
                    ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, KEY_READ, Name, PlaylistName, sizeof(PlaylistName), (INT_PTR)L"");
                    SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)PlaylistName);
                }
                if(nCount != 0){
                    SendMessage(hComboBox, CB_SETCURSEL, 0,0);
                    LoadPlaylist(hWnd);
                }else{
                    CreatePlaylist(hWnd, L"Default Playlist", TRUE);
                }
            }
            return 0;

        case WM_SIZE:
            if(wParam != SIZE_MINIMIZED){
                if (hBitmap != NULL) {
                    DeleteObject(hBitmap);
                    hBitmap = NULL;
                }

                ButtonWidth = 20;
                ButtonHeight = 20;
                Padding = 10;

                GetClientRect(hWnd, &crt);
                iWidth = crt.right - crt.left;
                iHeight = crt.bottom - crt.top;

                GetWindowRect(hBtns[0], &wrt);
                ScreenToClient(hWnd, (LPPOINT)&wrt);
                ScreenToClient(hWnd, (LPPOINT)&wrt + 1);

                ButtonState = (tag_ButtonState)SendMessage(hBtns[5], CBM_GETSTATE, 0, 0);

                if(ButtonState == DOWN || ButtonState == UP){
                    SetWindowPos(hProgress, NULL, wrt.left, wrt.top - (ButtonHeight + Padding), crt.right - Padding * 2, ButtonHeight, SWP_NOZORDER);
                    SetWindowPos(hVolume, NULL, wrt.left + ((crt.right - Padding * 2) / 4 * 3), wrt.top - (ButtonHeight + Padding) * 2, (crt.right - Padding * 2) / 4, ButtonHeight, SWP_NOZORDER );
                }
                else{
                    SetWindowPos(hProgress, NULL, Padding, iHeight - (ButtonHeight + Padding) * 2, crt.right - Padding * 2, ButtonHeight, SWP_NOZORDER );
                    SetWindowPos(hVolume, NULL, Padding + ((crt.right - Padding * 2) / 4 * 3), iHeight - (ButtonHeight + Padding) * 3, (crt.right - Padding * 2) / 4, ButtonHeight, SWP_NOZORDER);
                }

                GetWindowRect(hProgress, &rcSpectrum);
                ScreenToClient(hWnd, (LPPOINT)&rcSpectrum);
                ScreenToClient(hWnd, (LPPOINT)&rcSpectrum + 1);
                SetRect(&rcSpectrum, rcSpectrum.left, rcSpectrum.top - (TimeTextSize.cy + ButtonHeight + Padding * 3), rcSpectrum.left + BINS, rcSpectrum.top - (TimeTextSize.cy + Padding * 2));

                HDWP hdwp = BeginDeferWindowPos(BTN_COUNT);
                for(int i = 0; i < BTN_COUNT; i++){
                    if(ButtonState == DOWN || ButtonState == UP){
                        hdwp = DeferWindowPos(hdwp, hBtns[i], NULL, (Padding * (i + 1)) + (i * ButtonWidth), wrt.top, ButtonWidth, ButtonHeight, SWP_NOZORDER);
                    }
                    else{
                        hdwp = DeferWindowPos(hdwp, hBtns[i], NULL, (Padding * (i + 1)) + (i * ButtonWidth), iHeight - (Padding + ButtonHeight), ButtonWidth, ButtonHeight, SWP_NOZORDER);
                    }
                }
                EndDeferWindowPos(hdwp);
            }
            return 0;

        case WM_GETMINMAXINFO:
            {
                ButtonState = (tag_ButtonState)SendMessage(hBtns[5], CBM_GETSTATE, 0, 0);

                LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
                if(ButtonState != DOWN){
                    MinWidth = lpmmi->ptMinTrackSize.x = BTN_COUNT * ButtonWidth + (BTN_COUNT + 2) * Padding;
                    MinHeight = lpmmi->ptMinTrackSize.y = rcClose.bottom + (ButtonHeight + Padding) * 4;
                }
                else{
                    lpmmi->ptMinTrackSize.x = ewrt.right - ewrt.left;
                    lpmmi->ptMinTrackSize.y = ewrt.bottom - ewrt.top;
                    lpmmi->ptMaxTrackSize.x = ewrt.right - ewrt.left;
                    lpmmi->ptMaxTrackSize.y = ewrt.bottom - ewrt.top;
                }
            }
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            {
                // Draw Something
                GetClientRect(hWnd, &crt);

                iWidth = crt.right - crt.left;
                iHeight = crt.bottom - crt.top;

                hMemDC = CreateCompatibleDC(hdc);
                if(hBitmap == NULL){
                    hBitmap = CreateCompatibleBitmap(hdc, iWidth, iHeight);
                }
                hOld = SelectObject(hMemDC, hBitmap);
                FillRect(hMemDC, &crt, hBkBrush);

                SetBkMode(hMemDC, TRANSPARENT);
                SetRect(&rcClose, iWidth - (Diameter + EDGE), crt.top + EDGE, 0, 0);
                SetRect(&rcClose, rcClose.left, rcClose.top, rcClose.left + Diameter, rcClose.top + Diameter);
                SetRect(&rcMinimize, rcClose.left - (Diameter + EDGE), rcClose.top, 0, 0);
                SetRect(&rcMinimize, rcMinimize.left, rcMinimize.top, rcMinimize.left + Diameter, rcMinimize.top + Diameter);

                HBRUSH hBrush = CreateSolidBrush(RGB(250, 99, 71));		// tomato
                                                                        // HBRUSH hBrush = CreateSolidBrush(RGB(250, 69, 0));	// oragne red
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);
                Ellipse(hMemDC, rcClose.left, rcClose.top, rcClose.right, rcClose.bottom);
                DeleteObject(SelectObject(hMemDC, hOldBrush));

                hBrush = CreateSolidBrush(RGB(255, 204, 0));			// golden yellow(Amber)
                                                                        // hBrush = CreateSolidBrush(RGB(255, 223, 79));
                                                                        // hBrush = CreateSolidBrush(RGB(255, 235, 153));
                hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);
                Ellipse(hMemDC, rcMinimize.left, rcMinimize.top, rcMinimize.right, rcMinimize.bottom);
                DeleteObject(SelectObject(hMemDC, hOldBrush));

                GetWindowRect(hBtns[0], &wrt);
                ScreenToClient(hWnd, (LPPOINT)&wrt);
                ScreenToClient(hWnd, (LPPOINT)&wrt + 1);

                rtText.left = wrt.left;
                rtText.top = wrt.top - (ButtonHeight + Padding) * 2;
                rtText.right = rtText.left + TimeTextSize.cx;
                rtText.bottom = rtText.top + TimeTextSize.cy;

                if(TimeLine != NULL){
                    DrawText(hMemDC, TimeLine, -1, &rtText, DT_BOTTOM | DT_SINGLELINE | DT_CENTER);
                }

                rtText.left = wrt.left;
                rtText.top = wrt.bottom + Padding;
                rtText.right = rtText.left + TextSize.cx;
                rtText.bottom = rtText.top + TextSize.cy;

                if(SendMessage(hBtns[5], CBM_GETSTATE, 0, 0) == DOWN){
                    DrawText(hMemDC, Description, -1, &rtText, DT_BOTTOM | DT_SINGLELINE | DT_CENTER);
                }

                if(bOnWindow && bSpectrum && bSpectrumReady && hBitmap){
                    // 블렌딩으로 고르게 표현하기 위해 정규화
                    double FrameMax = 1e-6, DynamicMax = 1e-6;
                    for(int i=0; i<BINS; i++){
                        if(Spectrum[i] > FrameMax){ FrameMax = Spectrum[i]; }
                    }
                    DynamicMax = (((FrameMax) > (DynamicMax)) ? (FrameMax) : (((DynamicMax) * 0.95) + ((FrameMax) * 0.05)));

                    for(int i=0; i<BINS; i++){
                        Spectrum[i] /= DynamicMax;
                    }

                    // 블렌딩 인수 적용
                    for(int i=0; i<BINS; i++){
                        // 0.5도 적당한 편이나, 더 시각적 효과를 주기 위해 pow 적용
                        Spectrum[i] = pow(Spectrum[i], 0.4);
                    }

                    int Height = rcSpectrum.bottom - rcSpectrum.top;
                    for(int x = rcSpectrum.left, i=0; x < (x + BINS) / 2; x++, i++){
                        float Hue = (float)i / BINS;

                        Color Rainbow(Hue, 0.5f, 0.75f, TRUE);
                        COLORREF cr = Rainbow.ToColor().ToColorRef();

                        hPen = CreatePen(PS_SOLID, 1, cr);
                        hOldPen = (HPEN)SelectObject(hMemDC, hPen);
                        MoveToEx(hMemDC, x, rcSpectrum.bottom, NULL);
                        LineTo(hMemDC, x, (int)(rcSpectrum.bottom - min(Spectrum[i] * Height, Height)));
                        DeleteObject(SelectObject(hMemDC, hOldPen));
                    }
                }

                GetObject(hBitmap, sizeof(BITMAP), &bmp);
                BitBlt(hdc, 0, 0, bmp.bmWidth, bmp.bmHeight, hMemDC, 0, 0, SRCCOPY);
                SelectObject(hMemDC, hOld);
                DeleteDC(hMemDC);
            }
            EndPaint(hWnd, &ps);
            return 0;

        case WM_NOTIFY:
            lphdr = (LPNMHDR)lParam;

            if(lphdr->hwndFrom == hListView){
                switch(lphdr->code){
                    case NM_DBLCLK:
                        lpnia = (LPNMITEMACTIVATE)lParam;
                        if(lpnia->iItem != -1){
                            if(pPlayer){
                                pPlayer->Stop();
                                pPlayer->Shutdown();
                                pPlayer->Release();
                                pPlayer = NULL;
                            }
                            SendMessage(hBtns[1], CBM_SETSTATE, DOWN, (LPARAM)0);
                            SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hBtns[1]);
                        }
                        break;

                    case LVN_DELETEITEM:
                        lpnlv = (LPNMLISTVIEW)lParam;
                        memset(&LI, 0, sizeof(LI));
                        LI.mask = LVIF_PARAM;
                        LI.iItem = lpnlv->iItem;
                        ListView_GetItem(hListView, &LI);
                        free((LPVOID)LI.lParam);
                        break;
                }
            }
            return 0;

        case WM_PLAYNEXT:
            SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
            wcscpy(TimeLine, TimeSample);
            InvalidateRect(hWnd, NULL, FALSE);
            PlayNextOrPrev(hWnd, pCallback, &pPlayer, bLoop, bRandom, TRUE);
            return 0;

        case WM_TIMER:
            switch(wParam){
                case 1:
                    if(bSeeking){ break; }
                    if(pPlayer){
                        // PROPVARIANT: 메타 데이터 컨테이너 -> 쉘이나 COM 프로그래밍 토픽 참고
                        PROPVARIANT PropPosition, PropDuration;
                        PropVariantInit(&PropDuration);
                        PropVariantInit(&PropPosition);

                        if(SUCCEEDED(pPlayer->GetPosition(MFP_POSITIONTYPE_100NS, &PropPosition)) && SUCCEEDED(pPlayer->GetDuration(MFP_POSITIONTYPE_100NS, &PropDuration))){
                            // Duration: 전체 재생 시간, Position: 현재 위치
                            // 단위: 나노초, 1초: 천만, 1분: 6억
                            double Ratio = (double)PropPosition.uhVal.QuadPart / PropDuration.uhVal.QuadPart;
                            int Range = SendMessage(hProgress, CSM_GETRANGEMAX, 0,0);

                            int CurrentPosition = Ratio * Range;
                            SendMessage(hProgress, CSM_SETPOSITION, CurrentPosition, 0);

                            ULONGLONG Current = PropPosition.uhVal.QuadPart / 10000000; // 초 단위
                            ULONGLONG Total = PropDuration.uhVal.QuadPart / 10000000;

                            int cc = Current % 60;
                            int cm = (Current / 60) % 60;
                            int ch = Current / 3600;
                            int tc = Total % 60;
                            int tm = (Total / 60) % 60;
                            int th = Total / 3600;

                            wsprintf(TimeLine, L"[%02d:%02d:%02d / %02d:%02d:%02d]", ch, cm, cc, th, tm, tc);
                        }
                        PropVariantClear(&PropPosition);
                        PropVariantClear(&PropDuration);
                    }
                    break;

                case 2:
                    {
                        LARGE_INTEGER Now;
                        QueryPerformanceCounter(&Now);

                        LONGLONG Elapsed = Now.QuadPart - StartTime.QuadPart;
                        double Seconds = (double)Elapsed / Frequency.QuadPart;

                        int hh = (int)(Seconds / 3600);
                        int mm = (int)((Seconds - hh * 3600) / 60);
                        int ss = (int)(Seconds) % 60;
                        wsprintf(TimeLine, L"[%02d:%02d:%02d / %02d:%02d:%02d]", 0, 0, 0, hh, mm, ss);
                    }
                    break;

                case 3:
                    {
                        SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
                    }
                    break;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case WM_HSCROLL:
            if(pPlayer == NULL){ return 0; }
            switch(LOWORD(wParam)){
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION:
                    bSeeking = TRUE;
                    KillTimer(hWnd, 1);

                    if((HWND)lParam == hProgress){
                        int Range = SendMessage(hProgress, CSM_GETRANGEMAX, 0, 0);
                        int CurrentPosition = SendMessage(hProgress, CSM_GETPOSITION, 0, 0);

                        PROPVARIANT PropDuration;
                        PropVariantInit(&PropDuration);

                        if(SUCCEEDED(pPlayer->GetDuration(MFP_POSITIONTYPE_100NS, &PropDuration))){
                            ULONGLONG Duration = PropDuration.uhVal.QuadPart;
                            ULONGLONG NewPosition = (ULONGLONG)((double)CurrentPosition / Range * Duration);

                            // wsprintf(Debug, L"Range=%d, Current=%d, Duration=%I64u, NewPosition=%I64u", Range, CurrentPosition, Duration, NewPosition);
                            // MessageBox(HWND_DESKTOP, Debug, L"Value", MB_OK);

                            PROPVARIANT PropPosition;
                            PropVariantInit(&PropPosition);
                            // COM 기반 API에서는 PROPVARIANT의 VT 값과 내부 데이터 일치가 매우 중요하다.
                            // VT_UI8로 설정 시 Mismatch로 잘못된 인수 E_INVALIDARG 오류가 발생한다.
                            PropPosition.vt = VT_I8;
                            PropPosition.uhVal.QuadPart = NewPosition;

                            HRESULT hr = pPlayer->SetPosition(MFP_POSITIONTYPE_100NS, &PropPosition);
                            if(FAILED(hr)){
                                wsprintf(Debug, L"SetPosition 실패: 0x%08X", hr);
                                MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK);
                            }
                            PropVariantClear(&PropPosition);
                        }
                        PropVariantClear(&PropDuration);
                    }
                    if((HWND)lParam == hVolume){
                        int CurrentPosition = SendMessage(hVolume, CSM_GETPOSITION, 0,0);
                        float fUserVolume = (float)CurrentPosition / 255.f;
                        pPlayer->SetVolume(fUserVolume);
                    }
                    bSeeking = FALSE;
                    SetTimer(hWnd, 1, 50, NULL);
                    break;
            }
            return 0;

        case WM_NEWPOSITION:
            if(pPlayer){
                MFP_MEDIAPLAYER_STATE State;
                State = pCallback->GetCurrentState();
                ButtonState = (enum tag_ButtonState)SendMessage(hBtns[1], CBM_GETSTATE, 0,0);

                if(State != MFP_MEDIAPLAYER_STATE_PAUSED && ButtonState == DOWN){
                    pPlayer->Play();
                }
            }
            return 0;

        case WM_CLOSE:
            if(bExtended){
                if(pCallback && pPlayer){
                    MFP_MEDIAPLAYER_STATE State;
                    State = pCallback->GetCurrentState();

                    if(State == MFP_MEDIAPLAYER_STATE_PLAYING){
                        pPlayer->Stop();
                        pPlayer->Shutdown();
                        pPlayer->Release();
                        pPlayer = NULL;
                    }
                }

                GetWindowRect(hWnd, &wrt);
                ShowWindow(hWnd, SW_HIDE);

                int CurrentWidth = wrt.right - wrt.left;
                int CurrentHeight = wrt.bottom - wrt.top;
                int ExtendedHeight = MaxCount * ListBoxItemHeight;
                int MaxAnimationScene = 8;
                int Scene = ExtendedHeight / MaxAnimationScene;

                bExtended = FALSE;
                SetWindowPos(hComboBox, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
                SetWindowPos(hListView, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);

                for(int i = 1; i <= MaxAnimationScene; i++){
                    if(CurrentWidth >= MinWidth && (CurrentHeight - Scene * i) >= MinHeight){
                        SetWindowPos(hWnd, NULL, 0, 0, CurrentWidth, CurrentHeight - Scene * i, SWP_NOMOVE | SWP_NOZORDER | SWP_HIDEWINDOW);
                        InvalidateRect(hWnd, NULL, FALSE);
                        Sleep(max(10, min(MaxAnimationScene * 10, (MaxAnimationScene - (i + 2)) * 10)));
                    }
                }
            }
            DestroyWindow(hWnd);
            return 0;

        case WM_CONTEXTMENU:
            if((HWND)wParam == hListView){
                POINT Mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hListView, &Mouse);

                LVHITTESTINFO lvhi = {0};

                lvhi.pt = Mouse;
                int iHit = ListView_HitTest(hListView, &lvhi);
                if(iHit == -1){
                    ClientToScreen(hListView, &Mouse);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenu(hMenu, MF_STRING, IDM_CREATE_PLAYLIST, L"새 재생목록 생성");
                    AppendMenu(hMenu, MF_STRING, IDM_DELETE_PLAYLIST, L"현재 재생목록 삭제");
                    AppendMenu(hMenu, MF_STRING | ((bRandom == FALSE) ? MF_UNCHECKED : MF_CHECKED), IDM_RANDOM_PLAY, L"랜덤 재생");
                    AppendMenu(hMenu, MF_STRING | ((bLoop == FALSE) ? MF_UNCHECKED : MF_CHECKED), IDM_LOOP_PLAY, L"반복 재생");
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, Mouse.x, Mouse.y, 0, hWnd, NULL);
                    DestroyMenu(hMenu);
                }else{
                    int nItems = 0;
                    for(int i=0; i<ListView_GetItemCount(hListView); i++){
                        if(ListView_GetItemState(hListView, i, LVIS_SELECTED) & LVIS_SELECTED){
                            nItems++;
                        }
                    }

                    if(!(ListView_GetItemState(hListView, iHit, LVIS_SELECTED) & LVIS_SELECTED)){
                        ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED);
                        ListView_SetItemState(hListView, iHit, LVIS_SELECTED, LVIS_SELECTED);
                        nItems = 1;
                    }

                    ClientToScreen(hListView, &Mouse);
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenu(hMenu, MF_STRING, IDM_CREATE_PLAYLIST, L"새 재생목록 생성");
                    AppendMenu(hMenu, MF_STRING, IDM_DELETE_PLAYLIST, L"현재 재생목록 삭제");
                    AppendMenu(hMenu, MF_STRING | ((bRandom == FALSE) ? MF_UNCHECKED : MF_CHECKED), IDM_RANDOM_PLAY, L"랜덤 재생");
                    AppendMenu(hMenu, MF_STRING | ((bLoop == FALSE) ? MF_UNCHECKED : MF_CHECKED), IDM_LOOP_PLAY, L"반복 재생");
                    AppendMenu(hMenu, MF_STRING, IDM_ITEM_DELETE, L"삭제");
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, Mouse.x, Mouse.y, 0, hWnd, NULL);
                    DestroyMenu(hMenu);
                }
            }
            return 0;

        case WM_DROPFILES:
            {
                WCHAR Path[MAX_PATH];
                int nDrop = DragQueryFile((HDROP)wParam, -1, Path, MAX_PATH);

                for(int i=0; i<nDrop; i++){
                    DragQueryFile((HDROP)wParam, i, Path, MAX_PATH);

                    DWORD Attr = GetFileAttributes(Path);
                    if(Attr == INVALID_FILE_ATTRIBUTES){continue;}

                    if(Attr & FILE_ATTRIBUTE_DIRECTORY){
                        // 디렉토리 내부에 있는 파일 탐색
                        TraverseDirTree(hWnd, Path);
                    }else if(IsAudioFile(Path)){
                        // 파일의 확장자 비교 후 AppendFile
                        AppendFile(hListView, Path);
                    }
                }
                DragFinish((HDROP)wParam);

                SendMessage(hBtns[1], CBM_SETSTATE, DOWN, (LPARAM)0);
                SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hBtns[1]);

                SavePlaylist(hWnd);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            KillTimer(hWnd, 2);
            KillTimer(hWnd, 3);
            DeleteObject(hBkBrush);
            if(pPlayer){ 
                pPlayer->Shutdown();
                pPlayer->Release();
                pPlayer = NULL;
            }
            if(pCallback){
                pCallback->Release();
                pCallback = NULL;
            }
            if(hRecordThread){ CloseHandle(hRecordThread); }
            if(hSpectrumThread){ CloseHandle(hSpectrumThread); }
            if(hRecordStopEvent){ CloseHandle(hRecordStopEvent); }
            if(hSpectrumStopEvent){ CloseHandle(hSpectrumStopEvent); }
            if(hDeviceChangedEvent){ CloseHandle(hDeviceChangedEvent); }
            if(hBitmap){ DeleteObject(hBitmap); }
            Cleanup();

            WindowPlacement = {
                .length = sizeof(WINDOWPLACEMENT),
            };

            GetWindowPlacement(hWnd, &WindowPlacement);
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_WRITE, L"CurrentState", REG_DWORD, &WindowPlacement.showCmd, sizeof(UINT));
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_WRITE, L"Left", REG_DWORD, &WindowPlacement.rcNormalPosition.left, sizeof(LONG));
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_WRITE, L"Top", REG_DWORD, &WindowPlacement.rcNormalPosition.top, sizeof(LONG));
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_WRITE, L"Right", REG_DWORD, &WindowPlacement.rcNormalPosition.right, sizeof(LONG));
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_WRITE, L"Bottom", REG_DWORD, &WindowPlacement.rcNormalPosition.bottom, sizeof(LONG));
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_POSITION, KEY_WRITE, L"bSpectrum", REG_DWORD, &bSpectrum, sizeof(LONG));
            SavePlaylist(hWnd);
            nCount=SendMessage(hComboBox,CB_GETCOUNT,0,0);
            WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, KEY_WRITE, L"PlaylistCount", REG_DWORD, &nCount, sizeof(LONG));
            for(int i=0; i<nCount; i++) {
                wsprintf(Name, L"%d", i);
                SendMessage(hComboBox, CB_GETLBTEXT, i, (LPARAM)PlaylistName);
                WriteRegistryData(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, KEY_WRITE, Name, REG_SZ, PlaylistName, sizeof(WCHAR) * (wcslen(PlaylistName) + 1));
            }
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

HRESULT Initialize(){
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(FAILED(hr)){ return hr; }

    hr = MFStartup(MF_VERSION);
    if(FAILED(hr)){ return hr;  }

    return S_OK;
}

void Cleanup() {
    MFShutdown();
    CoUninitialize();
}

void OpenFiles(HWND hWnd){
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
    OFN.lpstrFilter = L"미디어 파일\0*.mp3;*.wav\0모든 파일(*.*)\0*.*\0";
    OFN.nMaxFile = 0x300;
    OFN.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT;

    if(GetOpenFileName(&OFN) != 0){
        ptr = lpstrFile;
        wcscpy(lpszName, ptr);

        ptr += wcslen(lpszName) + 1;
        if(*ptr != 0){
            wcscpy(lpszDir, lpszName);

            while(*ptr){
                wcscpy(lpszName, ptr);
                wsprintf(lpszTemp, L"%s\\%s", lpszDir, lpszName);
                AppendFile(hListView, lpszTemp);
                ptr += wcslen(lpszName) + 1;
            }
        }else{
            AppendFile(hListView, lpstrFile);
        }

        HWND hButton = GetDlgItem(hWnd, IDC_BTNFIRST + 1);
        SendMessage(hButton, CBM_SETSTATE, DOWN, (LPARAM)0);
        SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hButton);

        SavePlaylist(hWnd);
    }else{
        if(CommDlgExtendedError() == FNERR_BUFFERTOOSMALL){
            MessageBox(HWND_DESKTOP, L"선택한 파일이 너무 많습니다.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void AppendFile(HWND hListView, WCHAR* Path) {
    LVITEM LI = {0};
    WCHAR FileName[MAX_PATH];
    WCHAR* Param;

    LI.mask = LVIF_TEXT | LVIF_PARAM;
    LI.iSubItem = 0;
    LI.iItem = 100000;
    _wsplitpath(Path, NULL, NULL, FileName, NULL);
    LI.pszText = FileName;
    Param = (WCHAR*)malloc(sizeof(WCHAR) * (wcslen(Path) + 1));
    wcscpy(Param, Path);
    LI.lParam = (LPARAM)Param;
    ListView_InsertItem(hListView, &LI);
}

void PlaySelectedItem(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer **pPlayer, WCHAR* Return, BOOL bPaused){
    if(pCallback == NULL){ return; }

    HWND hButton = GetDlgItem(hWnd, IDC_BTNFIRST + 1);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);
    HWND hVolume = GetDlgItem(hWnd, IDC_SCRLFIRST + 1);

    int nCount = ListView_GetItemCount(hListView);
    int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if(SelectItem == -1 && nCount == 0){ 
        SendMessage(hButton, CBM_SETSTATE, UP, (LPARAM)0);
        return;
    }

    if(SelectItem == -1 && nCount != 0){
        SelectItem = 0;
    }

    LVITEM LI = { 0 };
    LI.mask = LVIF_PARAM;
    LI.iItem = SelectItem;
    ListView_GetItem(hListView, &LI);
    WCHAR* Path = (WCHAR*)LI.lParam;
    wcscpy(Return, (WCHAR*)LI.lParam);
    if(!Path){ return; }

    if(*pPlayer){
        MFP_MEDIAPLAYER_STATE State;
        State = pCallback->GetCurrentState();

        if(State == MFP_MEDIAPLAYER_STATE_PLAYING && bPaused == TRUE){
            (*pPlayer)->Pause();
        }else if(State == MFP_MEDIAPLAYER_STATE_PAUSED && bPaused == FALSE){
            (*pPlayer)->Play();
        }
        return;
    }

    if(SUCCEEDED(MFPCreateMediaPlayer(Path, FALSE, 0, pCallback, hWnd, pPlayer))){
        pCallback->SetPlayer(*pPlayer);

        int CurrentPosition = SendMessage(hVolume, CSM_GETPOSITION, 0,0);
        float fUserVolume = (float)CurrentPosition / 255.f;
        (*pPlayer)->SetVolume(fUserVolume);
        (*pPlayer)->Play();
    }
}

void PlayNextOrPrev(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer** pPlayer, BOOL bLoop, BOOL bRandom, BOOL bNext){
    if(*pPlayer){
        (*pPlayer)->Stop();
        (*pPlayer)->Shutdown();
        (*pPlayer)->Release();
        *pPlayer = NULL;
    }

    HWND hButton = GetDlgItem(hWnd, IDC_BTNFIRST + 1);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    int nCount = ListView_GetItemCount(hListView);
    int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if(SelectItem == -1 && nCount != 0){ SelectItem = 0; }

    if(SelectItem != -1){
        if(bRandom){
            int NewItem;
            do{
                NewItem = GetRandomInt(0, nCount);
            }while(nCount > 1 && NewItem == SelectItem);
            SelectItem = NewItem;
        }else if(bLoop){
            SelectItem = SelectItem;
        }else{
            if(bNext){
                SelectItem = (SelectItem + 1) % nCount;
            }else{
                SelectItem = (SelectItem - 1 + nCount) % nCount;
            }
        }
    }

    ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(hListView, SelectItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    SendMessage(hButton, CBM_SETSTATE, DOWN, (LPARAM)0);
    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hButton);
}

DWORD WINAPI RecordThread(LPVOID lParam){
    WCHAR Debug[0x100];
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    const WCHAR *FileName = TEMPFILENAME;

    WAVEFORMATEX* pwfx = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;

    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE* pData;
    DWORD dwFlags;
    UINT32 PacketLength = 0;
    DWORD TotalBytesWritten = 0;

    HANDLE hCaptureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    HANDLE Waits[2] = { hCaptureEvent, hRecordStopEvent };

    do{
        // 기본 렌더러 가져옴(스피커)
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if(FAILED(hr)){
            wsprintf(Debug, L"CoCreateInstance Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if(FAILED(hr)){
            wsprintf(Debug, L"DefaultAudioEndpoint Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 기본 장치에서 AudioClient 인터페이스 활성화
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 스테레오 믹스 오디오 포맷 가져옴
        hr = pAudioClient->GetMixFormat(&pwfx);
        if(FAILED(hr)){
            wsprintf(Debug, L"GetMixFormat Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 루프백 및 콜백 사용 캡처 초기화, 이벤트 객체 적용
        REFERENCE_TIME BufferDuration = 10000000;
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, BufferDuration, 0, pwfx, NULL);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient Initialize Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        hr = pAudioClient->SetEventHandle(hCaptureEvent);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient SetEventHandle Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 오디오 캡처 클라이언트 인터페이스 가져옴
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient GetService Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // WAV 헤더 비워두고 샘플 데이터 작성
        // 메모리로 관리하는 것보다 실시간으로 파일에 작성하는 것이 합리적
        hFile = CreateFile(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hFile == INVALID_HANDLE_VALUE){
            wsprintf(Debug, L"CreateFile Failed: %d", GetLastError());
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        DWORD Written = 0;
        BYTE Dummy[44] = {0,};
        if(!WriteFile(hFile, &Dummy, 44/* wav header */, &Written, NULL) || Written != 44){
            wsprintf(Debug, L"WriteFile Failed: %d", GetLastError());
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 캡처 시작
        hr = pAudioClient->Start();
        if(FAILED(hr)){
            wsprintf(Debug, L"pAudioClient Start Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        while(1){
            DWORD ret = WaitForMultipleObjects(2, Waits, FALSE, INFINITE);
            if(ret == WAIT_OBJECT_0 + 1){
                break;
            }

            if(ret == WAIT_OBJECT_0){
                pCaptureClient->GetNextPacketSize(&PacketLength);

                while(PacketLength > 0){
                    UINT32 NumFrames;
                    pCaptureClient->GetBuffer(&pData, &NumFrames, &dwFlags, NULL, NULL);

                    // WAVE_FORMAT_PCM(0x0001): 16비트 정수
                    // WAVE_FORMAT_IEEE_FLOAT(0x0003): 32비트 float
                    // WAVE_FORMAT_EXTENSIBLE(0xFFFE): 확장한 복잡한 형식, 하위에 실제 포맷 포함
                    // 대부분 float 형식을 반환하기 때문에 더 정밀한 분기 처리를 하지 않고 float으로 간주하여 처리한다.
                    if(pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pwfx->wBitsPerSample == 32)){
                        // 총 샘플 수 계산
                        UINT32 SamplesPerFrame = NumFrames * pwfx->nChannels;
                        const float* pFloat = (const float*)pData;

                        // 임시 버퍼 생성후 PCM 변환
                        short* PcmBuffer = (short*)malloc(sizeof(short) * SamplesPerFrame);
                        for(int i = 0; i < SamplesPerFrame; ++i){
                            // 정규화된 값, 즉 -1.0f ~ 1.0f 범위를 가지므로 이를 정수형으로 변환(short)
                            float Sample = pFloat[i];

                            // 값 잘라냄(클리핑)
                            if(Sample > 1.0f){ Sample = 1.0f; }
                            else if(Sample < -1.0f){ Sample = -1.0f; }

                            // short 범위로 매핑
                            PcmBuffer[i] = (short)(Sample * 32767.0f);
                        }

                        WriteFile(hFile, PcmBuffer, sizeof(short) * SamplesPerFrame, &Written, NULL);
                        TotalBytesWritten += Written;

                        free(PcmBuffer);
                    }else{
                        // PCM인 경우 그대로 저장
                        DWORD BytesToWrite = NumFrames * pwfx->nBlockAlign;
                        WriteFile(hFile, pData, BytesToWrite, &Written, NULL);
                        TotalBytesWritten += Written;
                    }

                    pCaptureClient->ReleaseBuffer(NumFrames);
                    pCaptureClient->GetNextPacketSize(&PacketLength);
                }
            }
        }
        pAudioClient->Stop();

        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        if(!WriteWavHeader(hFile, TotalBytesWritten, pwfx->nChannels, pwfx->nSamplesPerSec, PCM_BITS/* pwfx->wBitsPerSample: 32비트 값으로 읽었으나 PCM 포맷은 short 단위이므로 16으로 고정*/)){
            wsprintf(Debug, L"WriteWavHeader Failed");
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
        }
    }while(FALSE);

    if(hFile != INVALID_HANDLE_VALUE){ CloseHandle(hFile); }
    if(pwfx){ CoTaskMemFree(pwfx); }
    if(pCaptureClient){ pCaptureClient->Release(); }
    if(hCaptureEvent){ CloseHandle(hCaptureEvent); }
    if(pAudioClient){ pAudioClient->Release(); }
    if(pDevice){ pDevice->Release(); }
    if(pEnumerator){ pEnumerator->Release(); }

    CoUninitialize();
    return 0;
}

BOOL WriteWavHeader(HANDLE hFile, DWORD DataSize, WORD Channels, DWORD SampleRate, WORD BitsPerSample){
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

    DWORD Written = 0;
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    BOOL OK = WriteFile(hFile, &Header, sizeof(Header), &Written, NULL);
    return OK && Written == sizeof(Header);
}

DWORD WINAPI SpectrumThread(LPVOID lParam){
    WCHAR Debug[0x100];
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    WAVEFORMATEX* pwfx = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;

    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE* pData;
    DWORD dwFlags;
    UINT32 PacketLength = 0;
    DWORD TotalBytesWritten = 0;

    HANDLE hCaptureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    HANDLE Waits[2] = { hCaptureEvent, hSpectrumStopEvent };

    do{
        // 기본 렌더러 가져옴(스피커)
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if(FAILED(hr)){
            wsprintf(Debug, L"CoCreateInstance Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if(FAILED(hr)){
            wsprintf(Debug, L"DefaultAudioEndpoint Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 기본 장치에서 AudioClient 인터페이스 활성화
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 스테레오 믹스 오디오 포맷 가져옴
        hr = pAudioClient->GetMixFormat(&pwfx);
        if(FAILED(hr)){
            wsprintf(Debug, L"GetMixFormat Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 루프백 및 콜백 사용 캡처 초기화, 이벤트 객체 적용
        REFERENCE_TIME BufferDuration = 10000000;
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, BufferDuration, 0, pwfx, NULL);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient Initialize Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        hr = pAudioClient->SetEventHandle(hCaptureEvent);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient SetEventHandle Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 오디오 캡처 클라이언트 인터페이스 가져옴
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
        if(FAILED(hr)){
            wsprintf(Debug, L"AudioClient GetService Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 캡처 시작
        hr = pAudioClient->Start();
        if(FAILED(hr)){
            wsprintf(Debug, L"pAudioClient Start Failed: 0x%08X", hr);
            MessageBox(HWND_DESKTOP, Debug, L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        // 오디오 입력, 주파수 결과
        double Input[FFT_SIZE];
        fftw_complex Output[FFT_SIZE / 2 + 1];

        // 초기화
        fftw_plan Plan;
        Plan = fftw_plan_dft_r2c_1d(FFT_SIZE, Input, Output, FFTW_ESTIMATE);

        static int SampleIndex = 0;
        while(1){
            DWORD ret = WaitForMultipleObjects(2, Waits, FALSE, INFINITE);
            if(ret == WAIT_OBJECT_0 + 1 || ret == WAIT_FAILED){
                break;
            }

            if(ret == WAIT_OBJECT_0){
                pCaptureClient->GetNextPacketSize(&PacketLength);

                while(PacketLength > 0){
                    UINT32 NumFrames;
                    pCaptureClient->GetBuffer(&pData, &NumFrames, &dwFlags, NULL, NULL);

                    int Offset = 0;
                    while(Offset < NumFrames){
                        int Remaining = FFT_SIZE - SampleIndex;
                        int CopyCount = min(Remaining, NumFrames - Offset);
                        int Channels = pwfx->nChannels;

                        if(pwfx->wBitsPerSample == 16 && pwfx->wFormatTag == WAVE_FORMAT_PCM){
                            short* Samples = (short*)pData;
                            for(int i=0; i<CopyCount; i++){
                                double Sum = 0;
                                for(int j=0; j<Channels; j++){
                                    Sum += Samples[(Offset + i) * Channels + j];

                                }
                                Input[SampleIndex++] = (Sum / Channels) / 32768.0;
                            }
                        }else if(pwfx->wBitsPerSample == 32 && pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE){
                            float* Samples = (float*)pData;
                            for(int i=0; i<CopyCount; i++){
                                double Sum = 0;
                                for(int j=0; j<Channels; j++){
                                    Sum += Samples[(Offset + i) * Channels + j];
                                }
                                Input[SampleIndex++] = Sum / Channels;
                            }
                        }else if(pwfx->wBitsPerSample == 32 && pwfx->wFormatTag == WAVE_FORMAT_PCM){
                            int* Samples = (int*)pData;
                            for(int i=0; i<CopyCount; i++){
                                double Sum = 0;
                                for(int j=0; j<Channels; j++){
                                    Sum += Samples[(Offset + i) * Channels + j];
                                }
                                Input[SampleIndex++] = (Sum / Channels) / 2147483648.0;
                            }
                        }
                        Offset += CopyCount;

                        if(SampleIndex >= FFT_SIZE) {
                            // Input -> Output 푸리에 변환
                            fftw_execute(Plan);

                            for(int i = 0; i < BINS; i++){
                                int Start = i * ((FFT_SIZE / 2 + 1) / BINS);
                                int End = (i + 1) * ((FFT_SIZE / 2 + 1) / BINS);
                                if(End > (FFT_SIZE / 2 + 1)){
                                    End = (FFT_SIZE / 2 + 1);
                                }

                                double Sum = 0;
                                for(int j = Start; j < End; j++){
                                    double Real = Output[j][0];
                                    double Imagine = Output[j][1];
                                    Sum += sqrt(Real * Real + Imagine * Imagine);
                                }
                                // 평균값 계산
                                Spectrum[i] = Sum / (End - Start);
                            }
                            SampleIndex = 0;
                            bSpectrumReady = TRUE;
                        }
                    }

                    pCaptureClient->ReleaseBuffer(NumFrames);
                    pCaptureClient->GetNextPacketSize(&PacketLength);
                }
            }
        }

        pAudioClient->Stop();
        fftw_destroy_plan(Plan);
        bSpectrumReady = FALSE;
    }while(FALSE);

    if(pwfx){ CoTaskMemFree(pwfx); }
    if(pCaptureClient){ pCaptureClient->Release(); }
    if(hCaptureEvent){ CloseHandle(hCaptureEvent); }
    if(pAudioClient){ pAudioClient->Release(); }
    if(pDevice){ pDevice->Release(); }
    if(pEnumerator){ pEnumerator->Release(); }

    CoUninitialize();
    return 0;
}

LRESULT CALLBACK InputPopupWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam){
    static WCHAR Title[MAX_PATH];

    switch(iMessage){
        case WM_CREATE:
            GetWindowText(hWnd, Title, MAX_PATH);

            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
            CreateWindow(L"edit", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 10, 260, 24, hWnd, (HMENU)1001, NULL, NULL);
            CreateWindow(L"button", L"저장", WS_CHILD | WS_VISIBLE, 30, 45, 80, 24, hWnd, (HMENU)1002, NULL, NULL);
            CreateWindow(L"button", L"취소", WS_CHILD | WS_VISIBLE, 120, 45, 80, 24, hWnd, (HMENU)1003, NULL, NULL);
            return 0;

        case WM_COMMAND:
            if(LOWORD(wParam) == 1002){
                WCHAR* pOut = (WCHAR*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
                HWND hEdit = GetDlgItem(hWnd, 1001);
                GetWindowText(hEdit, pOut, MAX_PATH);
                if(wcscmp(Title, INPUT_POPUP_TEMPLATE1) == 0){
                    wcscat(pOut, L".wav");
                }
                DestroyWindow(hWnd);
            }

            if(LOWORD(wParam) == 1003){
                DestroyWindow(hWnd);
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }

    return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

BOOL ShowInputPopup(HWND hParent, WCHAR* Out, int MaxLength, int iMode){
    WNDCLASS twc;
    if(!GetClassInfo(GetModuleHandle(NULL), INPUT_POPUP_CLASS_NAME, &twc)){
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = InputPopupWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = INPUT_POPUP_CLASS_NAME;
        RegisterClass(&wc);
    }

    HWND hPopup = NULL;
    switch(iMode){
        case 0:
            hPopup = CreateWindowEx(WS_EX_TOOLWINDOW, INPUT_POPUP_CLASS_NAME, INPUT_POPUP_TEMPLATE1, WS_POPUP | WS_BORDER | WS_CAPTION, 100, 100, 280, 110, hParent, NULL, GetModuleHandle(NULL), (LPVOID)Out);
            break;

        case 1:
            hPopup = CreateWindowEx(WS_EX_TOOLWINDOW, INPUT_POPUP_CLASS_NAME, INPUT_POPUP_TEMPLATE2, WS_POPUP | WS_BORDER | WS_CAPTION, 100, 100, 280, 110, hParent, NULL, GetModuleHandle(NULL), (LPVOID)Out);
            break;
    }

    if(!hPopup){ return FALSE; }

    ShowWindow(hPopup, SW_SHOW);
    UpdateWindow(hPopup);

    RECT srt;
    SetWindowCenter(hParent, hPopup);
    EnableWindow(hParent, FALSE);

    MSG msg;
    while(IsWindow(hPopup) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    return wcslen(Out) > 0;
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

SIZE GetScaledWindowSize(HWND hWnd){
    RECT wrt;
    GetWindowRect(hWnd, &wrt);

    int Width = wrt.right - wrt.left;
    int Height = wrt.bottom - wrt.top;

    UINT DPI = GetDpiForWindow(hWnd);
    float ScaleFactor = DPI / 96.0f;

    SIZE Scaled = {
        (int)(Width  * ScaleFactor),
        (int)(Height * ScaleFactor)
    };

    return Scaled;
}

BOOL SetWindowCenter(HWND hParent, HWND hChild){
    if(!hChild){ return FALSE; }

    POINT Center = GetMonitorCenter(hParent);
    SIZE WindowSize = GetScaledWindowSize(hChild);

    int Left = Center.x - (WindowSize.cx / 2);
    int Top  = Center.y - (WindowSize.cy / 2);

    return SetWindowPos(hChild, NULL, Left, Top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

void TraverseDirTree(HWND hWnd, WCHAR* Path){
    WIN32_FIND_DATA wfd;

    WCHAR CopyPath[MAX_PATH];
    wsprintf(CopyPath, L"%s\\*", Path);

    HANDLE hFind = FindFirstFile(CopyPath, &wfd);
    if(hFind == INVALID_HANDLE_VALUE){ return; }

    do{
        if(wcscmp(wfd.cFileName, L".") == 0 || wcscmp(wfd.cFileName, L"..") == 0){ continue; }

        WCHAR FullPath[MAX_PATH];
        wsprintf(FullPath, L"%s\\%s", Path, wfd.cFileName);

        if(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            TraverseDirTree(hWnd, FullPath);
        }else{
            if(IsAudioFile(FullPath)){
                AppendFile(GetDlgItem(hWnd, IDC_LVFIRST), FullPath);
            }
        }
    }while(FindNextFile(hFind, &wfd));

    FindClose(hFind);
}

BOOL IsAudioFile(const WCHAR* Path){
    const WCHAR* Extension = PathFindExtension(Path);

    if(Extension != NULL){
        if(_wcsicmp(Extension, L".wav") == 0 || _wcsicmp(Extension, L".mp3") == 0){
            return TRUE;
        }
    }

    return FALSE;
}

// 탐색 동작 확장할 생각 있으면 추가
BOOL MatchPattern(const WCHAR* Path, const WCHAR* Pattern) {
    if(*Pattern == 0){ return *Path == 0; }

    if(*Pattern == '*'){
        while(*Pattern == '*'){ Pattern++; }
        if(*Pattern == 0){ return TRUE; }

        while(*Path){
            if(MatchPattern(Path, Pattern)){ return TRUE; }
            Path++;
        }
        return FALSE;
    }

    if(*Pattern == '?'){
        return *Path ? MatchPattern(Path + 1, Pattern + 1) : FALSE;
    }

    if(*Path == *Pattern){
        return MatchPattern(Path + 1, Pattern + 1);
    }

    return FALSE;
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

void CreatePlaylist(HWND hWnd, WCHAR* Name, BOOL bDelete){
    if(Name == NULL || wcslen(Name) == 0){ return; }

    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    WCHAR PlaylistName[MAX_PATH];
    wcscpy(PlaylistName, Name);

    if(bDelete){ ListView_DeleteAllItems(hListView); }

    SendMessage(hComboBox, CB_INSERTSTRING, 0, (LPARAM)PlaylistName);
    SendMessage(hComboBox, CB_SETCURSEL, 0,0);

    WCHAR Path[MAX_PATH];
    int Count = SendMessage(hComboBox, CB_GETCOUNT, 0,0);

    if(Count > 20){
        SendMessage(hComboBox, CB_GETLBTEXT, 20, (LPARAM)PlaylistName);
        SendMessage(hComboBox, CB_DELETESTRING, 20, 0);
        wsprintf(Path, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);
        SHDeleteKey(HKEY_CURRENT_USER, Path);
    }
}

void SavePlaylist(HWND hWnd){
    WCHAR PlaylistName[MAX_PATH];
    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    int Select = SendMessage(hComboBox, CB_GETCURSEL, 0,0);
    SendMessage(hComboBox, CB_GETLBTEXT, Select, (LPARAM)PlaylistName);

    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);
    int nCount = ListView_GetItemCount(hListView);

    WCHAR PATH[MAX_PATH];
    wsprintf(PATH, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);
    WriteRegistryData(HKEY_CURRENT_USER, PATH, KEY_WRITE, L"Count", REG_DWORD, &nCount, sizeof(LONG));

    LVITEM LI;
    WCHAR Name[0x40];
    for(int i=0; i<nCount; i++){
        LI.mask = LVIF_PARAM;
        LI.iItem = i;
        ListView_GetItem(hListView, &LI);
        wsprintf(Name, L"%d", i);
        WriteRegistryData(HKEY_CURRENT_USER, PATH, KEY_WRITE, Name, REG_SZ, (WCHAR*)LI.lParam, sizeof(WCHAR) * (wcslen((WCHAR*)LI.lParam) + 1));
    }
}

BOOL DestroyPlaylist(HWND hWnd){
    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);

    WCHAR PlaylistName[MAX_PATH];
    int Select = SendMessage(hComboBox, CB_GETCURSEL, 0,0);
    SendMessage(hComboBox, CB_GETLBTEXT, Select, (LPARAM)PlaylistName);
    if(wcscmp(PlaylistName, L"Default Playlist") == 0){ return FALSE; }

    ListView_DeleteAllItems(hListView);
    SendMessage(hComboBox, CB_DELETESTRING, Select, 0);
    SendMessage(hComboBox, CB_SETCURSEL, Select, 0);

    WCHAR Path[MAX_PATH];
    wsprintf(Path, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);
    SHDeleteKey(HKEY_CURRENT_USER, Path);

    HKEY hKey;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS){ return FALSE; }

    int nCount;
    DWORD dwType = ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, KEY_READ, L"PlaylistCount", &nCount, sizeof(LONG), 0);

    WCHAR Name[0x40];
    WCHAR Data[MAX_PATH];
    for(int i=0; i<nCount; i++){
        wsprintf(Name, L"%d", i);
        ReadRegistryData(HKEY_CURRENT_USER, KEY_PATH_PLAYLIST, KEY_READ, Name, Data, sizeof(Data), (INT_PTR)L"");
        if(wcscmp(PlaylistName, Data) == 0){
            break;
        }
    }
    RegDeleteValue(hKey, Name);
    RegCloseKey(hKey);
    return TRUE;
}

void LoadPlaylist(HWND hWnd){
    WCHAR PlaylistName[MAX_PATH];

    HWND hComboBox = GetDlgItem(hWnd, IDC_CBFIRST);
    HWND hListView = GetDlgItem(hWnd, IDC_LVFIRST);
    ListView_DeleteAllItems(hListView);

    int Select = SendMessage(hComboBox, CB_GETCURSEL, 0,0);
    SendMessage(hComboBox, CB_GETLBTEXT, Select, (LPARAM)PlaylistName);

    WCHAR PATH[MAX_PATH];
    wsprintf(PATH, L"%s\\%s", KEY_PATH_PLAYLIST, PlaylistName);

    int nCount;
    DWORD dwType = ReadRegistryData(HKEY_CURRENT_USER, PATH, KEY_READ, L"Count", &nCount, sizeof(LONG), 0);

    WCHAR Name[0x40];
    WCHAR Data[MAX_PATH];
    for(int i=0; i<nCount; i++){
        wsprintf(Name, L"%d", i);
        dwType = ReadRegistryData(HKEY_CURRENT_USER, PATH, KEY_READ, Name, Data, sizeof(Data), (INT_PTR)L"");
        if(dwType != NULL){
            AppendFile(hListView, Data);
        }
    }
}
