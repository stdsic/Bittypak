#include "resource.h"
#define CLASS_NAME					L"MusicPlayer"
#define IDC_CBFIRST					0x500
#define IDC_LBFIRST					0x600
#define IDC_STFIRST					0x800

#define DEFAULT_MAINWINDOW_WIDTH	300
#define DEFAULT_MAINWINDOW_HEIGHT	150

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

#define WM_PLAYNEXT					WM_APP + 1
#define WM_NEWPOSITION				WM_APP + 2

// 콜백용 클래스 생성
class PlayerCallback : public IMFPMediaPlayerCallback{
	LONG RefCount = 1;
	HWND hWnd;
	IMFPMediaPlayer* pPlayer = NULL;
	WCHAR Debug[0x100];

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
				break;

			case MFP_EVENT_TYPE_PAUSE:
				break;

			case MFP_EVENT_TYPE_STOP:
				if(pPlayer){
					pPlayer->Shutdown();
					pPlayer->Release();
					pPlayer = NULL;
				}
				break;

			case MFP_EVENT_TYPE_POSITION_SET:
				PostMessage(hWnd, WM_NEWPOSITION, 0, 0);
				break;

			case MFP_EVENT_TYPE_RATE_SET:
				break;

			case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
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

			case MFP_EVENT_TYPE_MEDIAITEM_CLEARED:
				break;

			case MFP_EVENT_TYPE_MF:
				break;

			case MFP_EVENT_TYPE_ERROR:
				wsprintf(Debug, L"오류 발생: 0x%08X\n", pEventHeader->hrEvent);
				MessageBox(hWnd, Debug, L"Error", MB_OK | MB_ICONERROR);
				break;

			case MFP_EVENT_TYPE_PLAYBACK_ENDED:
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

	public:
	PlayerCallback(HWND _hWnd) : hWnd(_hWnd){;}
	~PlayerCallback() {
		if(pPlayer){
			pPlayer->Release();
			pPlayer = NULL;
		}
	}

};

HRESULT Initialize();
void Cleanup();
void OpenFiles(HWND hWnd, HWND hListView);
void AppendFile(HWND hListView, WCHAR* Path);
void PlaySelectedItem(HWND hWnd, HWND hListView, HWND hVolume, PlayerCallback* pCallback, IMFPMediaPlayer **pPlayer, BOOL bPaused = FALSE);
void PlayNextOrPrev(HWND hWnd, HWND hBtn, HWND hListView, HWND hVolume, IMFPMediaPlayer* pPlayer, BOOL bNext);
BOOL SystemAudioCapture(const WCHAR* FileName, int Seconds = 5);
void WriteWavHeader(HANDLE hFile, DWORD DataSize, WORD Channels, DWORD SampleRate, WORD BitsPerSample);

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow){
	HRESULT hr = Initialize();
	if(FAILED(hr)){ return 0; }

	WNDCLASSEX wcex = {
		sizeof(wcex),
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,0,
		hInst,
		LoadIcon(NULL, IDI_APPLICATION), LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		NULL,
		CLASS_NAME,
		LoadIcon(NULL, IDI_APPLICATION)
	};

	RegisterClassEx(&wcex);

	HWND hWnd = CreateWindowEx(
			WS_EX_CLIENTEDGE,
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

	Cleanup();
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
	static int MinWidth = BTN_COUNT * ButtonWidth + BTN_COUNT * Padding;
	static int MinHeight = (rcClose.bottom + ButtonHeight + Padding * 2) * 2;

	HDC hdc, hMemDC;
	PAINTSTRUCT ps;
	RECT crt, srt, wrt, trt;
	HGDIOBJ hOld;
	BITMAP bmp;

	LVCOLUMN COL;

	LVITEM li;
	LPNMHDR lphdr;
	LPNMITEMACTIVATE lpnia;
	LPNMLISTVIEW lpnlv;

	HRESULT hr;

	static IMFPMediaPlayer* pPlayer = NULL;
	static PlayerCallback* pCallback = NULL;
	static BOOL bSeeking = FALSE;
	WCHAR Debug[256];

	switch (iMessage){
		case WM_CREATE:
			pCallback = new PlayerCallback(hWnd);
			{
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

				hProgress = CreateWindow(SCROLLBAR_CLASS_NAME, NULL, WS_CHILD | WS_VISIBLE | CSS_HORZ, 0,0,0,0, hWnd, (HMENU)(INT_PTR)IDC_SCRLFIRST, GetModuleHandle(NULL), NULL);
				hVolume = CreateWindow(SCROLLBAR_CLASS_NAME, NULL, WS_CHILD | WS_VISIBLE | CSS_HORZ, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_SCRLFIRST + 1, GetModuleHandle(NULL), NULL);

				hComboBox = CreateWindow(L"combobox", NULL, WS_CHILD | CBS_DROPDOWNLIST, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_CBFIRST), GetModuleHandle(NULL), NULL);
				hListView = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)(IDC_LBFIRST), GetModuleHandle(NULL), NULL);
				ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT);

				COL.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
				COL.fmt = LVCFMT_LEFT;
				COL.cx = 255;
				COL.pszText = (LPWSTR)L"파일명";
				COL.iSubItem = 0;
				ListView_InsertColumn(hListView, 0, &COL);

				DragAcceptFiles(hWnd, TRUE);
			}
			return 0;

		case WM_LBUTTONDOWN:
			{
				POINT Mouse = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };

				CopyRect(&srt, &rcClose);
				CopyRect(&trt, &rcMinimize);
				InflateRect(&srt, -2, -2);
				InflateRect(&trt, -2, -2);
				if (PtInRect(&srt, Mouse)) {
					DestroyWindow(hWnd);
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
							HRESULT hr = pPlayer->GetState(&State);
							if(State == MFP_MEDIAPLAYER_STATE_PLAYING || State == MFP_MEDIAPLAYER_STATE_PAUSED){
								pPlayer->Stop();
							}
						}

						wcscpy(TimeLine, TimeSample);
						SendMessage(hProgress, CSM_SETPOSITION, (WPARAM)0, (LPARAM)0);
						SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
					}
					InvalidateRect(hWnd, NULL, FALSE);
					break;

				case IDC_BTNFIRST + 1:
					// 리스트 뷰 활용 -> 선택한 항목 문자열 불러온 후 파일 재생
					KillTimer(hWnd, 1);
					if(HIWORD(wParam) == PRESSED){
						int nCount = ListView_GetItemCount(hListView);
						int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
						if(SelectItem == -1 && nCount == 0){ 
							SendMessage(hBtns[1], CBM_SETSTATE, UP, (LPARAM)0);
							return 0;
						}
						SetTimer(hWnd, 1, 50, NULL);
						PlaySelectedItem(hWnd, hListView, hVolume, pCallback, &pPlayer);
					}

					if(HIWORD(wParam) == RELEASED){
						PlaySelectedItem(hWnd, hListView, hVolume, pCallback, &pPlayer, TRUE);
					}
					break;

				case IDC_BTNFIRST + 2:
					if(HIWORD(wParam) == PRESSED){
						PlayNextOrPrev(hWnd, hBtns[1], hListView, hVolume, pPlayer, FALSE);
					}
					break;

				case IDC_BTNFIRST + 3:
					if(HIWORD(wParam) == PRESSED){
						PlayNextOrPrev(hWnd, hBtns[1], hListView, hVolume, pPlayer, TRUE);
					}
					break;

				case IDC_BTNFIRST + 4:
					if(HIWORD(wParam) == PRESSED){
						OpenFiles(hWnd, hListView);
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
							SetWindowPos(hComboBox, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
							SetWindowPos(hListView, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
						}

						// 애니메이션 효과를 위해 상수값 사용
						for(int i = 1; i <= MaxAnimationScene; i++){
							if(HIWORD(wParam) == PRESSED){
								SetWindowPos(hWnd, NULL, 0, 0, CurrentWidth, CurrentHeight + Scene * i, SWP_NOMOVE | SWP_NOZORDER);
							}
							else if(HIWORD(wParam) == RELEASED){
								if(CurrentWidth >= MinWidth && (CurrentHeight - Scene * i) >= MinHeight){
									SetWindowPos(hWnd, NULL, 0, 0, CurrentWidth, CurrentHeight - Scene * i, SWP_NOMOVE | SWP_NOZORDER);
								}
							}

							InvalidateRect(hWnd, NULL, FALSE);
							Sleep(max(10, min(MaxAnimationScene * 10, (MaxAnimationScene - (i + 2)) * 10)));
						}

						if(hBitmap != NULL){
							DeleteObject(hBitmap);
							hBitmap = NULL;
						}

						if(HIWORD(wParam) == PRESSED){
							GetClientRect(hWnd, &crt);
							GetWindowRect(hBtns[0], &wrt);
							ScreenToClient(hWnd, (LPPOINT)&wrt);
							ScreenToClient(hWnd, (LPPOINT)&wrt + 1);

							SetWindowPos(hComboBox, NULL, rtText.right + Padding, rtText.top, crt.right - (rtText.right + Padding * 2), rtText.bottom - rtText.top, SWP_NOZORDER | SWP_SHOWWINDOW);
							SetWindowPos(hListView, NULL, rtText.left, rtText.bottom + Padding, rtText.left + (rtText.right - rtText.left) + crt.right - (rtText.right + Padding * 2), crt.bottom - (wrt.top + ButtonHeight + Padding * 2 + (rtText.bottom - rtText.top)), SWP_NOZORDER | SWP_SHOWWINDOW);
						}

						GetWindowRect(hWnd, &ewrt);
					}
					break;

				case IDC_BTNFIRST + 6:
					SystemAudioCapture(L"AudioCaptureTest.wav");
					break;

				case IDC_BTNFIRST + 7:
					break;
			}
			return 0;

		case WM_NCHITTEST:
			{
				POINT Mouse = { LOWORD(lParam), HIWORD(lParam) };
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
					ScreenToClient(hWnd, &Mouse);

					CopyRect(&srt, &rcClose);
					CopyRect(&trt, &rcMinimize);
					InflateRect(&srt, -2, -2);
					InflateRect(&trt, -2, -2);
					if (!PtInRect(&srt, Mouse) && !PtInRect(&trt, Mouse)) {
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

		case WM_ACTIVATE:
			if(bFirst){
				bFirst = !bFirst;

				SendMessage(hProgress, CSM_SETRANGE, (WPARAM)0, (LPARAM)1000);
				SendMessage(hProgress, CSM_SETTHUMBSIZE, (WPARAM)20, (LPARAM)0);
				SendMessage(hProgress, CSM_SETTHUMBCOLOR, (WPARAM)RGB(255, 255, 255), (LPARAM)0);
				SendMessage(hProgress, CSM_SETGAP, (WPARAM)5, (LPARAM)0);

				SendMessage(hVolume, CSM_SETRANGE, (WPARAM)0, (LPARAM)255);
				SendMessage(hVolume, CSM_SETTHUMBSIZE, (WPARAM)10, (LPARAM)0);
				SendMessage(hVolume, CSM_SETTHUMBCOLOR, (WPARAM)RGB(255, 255, 255), (LPARAM)0);
				SendMessage(hVolume, CSM_SETGAP, (WPARAM)5, (LPARAM)0);
				SendMessage(hVolume, CSM_SETPOSITION, (WPARAM)255, 0);
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

				tag_ButtonState CurrentState = (tag_ButtonState)SendMessage(hBtns[5], CBM_GETSTATE, 0, 0);

				if(CurrentState == DOWN || CurrentState == UP){
					SetWindowPos(hProgress, NULL, wrt.left, wrt.top - (ButtonHeight + Padding), crt.right - Padding * 2, ButtonHeight, SWP_NOZORDER);
					SetWindowPos(hVolume, NULL, wrt.left + ((crt.right - Padding * 2) / 4 * 3), wrt.top - (ButtonHeight + Padding) * 2, (crt.right - Padding * 2) / 4, ButtonHeight, SWP_NOZORDER );
				}
				else{
					SetWindowPos(hProgress, NULL, Padding, iHeight - (ButtonHeight + Padding) * 2, crt.right - Padding * 2, ButtonHeight, SWP_NOZORDER );
					SetWindowPos(hVolume, NULL, Padding + ((crt.right - Padding * 2) / 4 * 3), iHeight - (ButtonHeight + Padding) * 3, (crt.right - Padding * 2) / 4, ButtonHeight, SWP_NOZORDER);
				}

				HDWP hdwp = BeginDeferWindowPos(BTN_COUNT);
				for(int i = 0; i < BTN_COUNT; i++){
					if(CurrentState == DOWN || CurrentState == UP){
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
				tag_ButtonState CurrentState = (tag_ButtonState)SendMessage(hBtns[5], CBM_GETSTATE, 0, 0);

				LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
				if(CurrentState != DOWN){
					MinWidth = lpmmi->ptMinTrackSize.x = BTN_COUNT * ButtonWidth + (BTN_COUNT + 2) * Padding;
					MinHeight = lpmmi->ptMinTrackSize.y = (rcClose.bottom + ButtonHeight + Padding * 2) * 2;
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
				FillRect(hMemDC, &crt, GetSysColorBrush(COLOR_WINDOW));

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
							}
							SendMessage(hBtns[1], CBM_SETSTATE, DOWN, (LPARAM)0);
							SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hBtns[1]);
						}
						break;

					case LVN_DELETEITEM:
						lpnlv = (LPNMLISTVIEW)lParam;
						li.mask = LVIF_PARAM;
						li.iItem = lpnlv->iItem;
						ListView_GetItem(hListView, &li);
						free((LPVOID)li.lParam);
						break;
				}
			}
			return 0;

		case WM_PLAYNEXT:
			PlayNextOrPrev(hWnd, hBtns[1], hListView, hVolume, pPlayer, TRUE);
			return 0;

		case WM_TIMER:
			switch(wParam){
				case 1:
					if(bSeeking){ break; }

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
							}else{
								pPlayer->Pause();
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
				enum tag_ButtonState State = (enum tag_ButtonState)SendMessage(hBtns[1], CBM_GETSTATE, 0,0);
				if(State == DOWN){
					pPlayer->Play();
				}
			}
			return 0;

		case WM_DESTROY:
			KillTimer(hWnd, 1);
			if(pPlayer){ 
				pPlayer->Shutdown();
				pPlayer->Release();
				pPlayer = NULL;
			}
			if(pCallback){
				pCallback->Release();
				pCallback = NULL;
			}
			if(hBitmap){ DeleteObject(hBitmap); }
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

HRESULT Initialize() {
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) { return hr; }

	hr = MFStartup(MF_VERSION);
	if (FAILED(hr)) { return hr;  }

	return S_OK;
}

void Cleanup() {
	MFShutdown();
	CoUninitialize();
}

void OpenFiles(HWND hWnd, HWND hListView){
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

			while (*ptr) {
				wcscpy(lpszName, ptr);
				wsprintf(lpszTemp, L"%s\\%s", lpszDir, lpszName);
				AppendFile(hListView, lpszTemp);
				ptr += wcslen(lpszName) + 1;
			}
		}else{
			AppendFile(hListView, lpstrFile);
		}
	}else{
		if(CommDlgExtendedError() == FNERR_BUFFERTOOSMALL){
			MessageBox(HWND_DESKTOP, L"선택한 파일이 너무 많습니다.", L"Error", MB_OK | MB_ICONERROR);
		}
	}
}

void AppendFile(HWND hListView, WCHAR* Path) {
	LVITEM LI;
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

void PlaySelectedItem(HWND hWnd, HWND hListView, HWND hVolume, PlayerCallback* pCallback, IMFPMediaPlayer **pPlayer, BOOL bPaused){
	if(pCallback == NULL){ return ; }

	static WCHAR LastPath[MAX_PATH] = L"";
	int nCount = ListView_GetItemCount(hListView);
	int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
	if(SelectItem == -1 && nCount == 0){ return; }
	if(SelectItem == -1 && nCount != 0){ SelectItem = 0; }

	LVITEM li = { 0 };
	li.mask = LVIF_PARAM;
	li.iItem = SelectItem;
	ListView_GetItem(hListView, &li);

	WCHAR* Path = (WCHAR*)li.lParam;
	if(!Path){ return; }

	if(*pPlayer){
		MFP_MEDIAPLAYER_STATE State;
		(*pPlayer)->GetState(&State);

		if(wcscmp(Path, LastPath) == 0){
			if(State == MFP_MEDIAPLAYER_STATE_PLAYING && bPaused == TRUE){
				(*pPlayer)->Pause();
			}else if(State == MFP_MEDIAPLAYER_STATE_PAUSED && bPaused == FALSE){
				(*pPlayer)->Play();
			}
			return;
		}
			
		// 다른 파일이면 기존 플레이어 정리
		(*pPlayer)->Shutdown();
		(*pPlayer)->Release();
		*pPlayer = NULL;
	}

	if(SUCCEEDED(MFPCreateMediaPlayer(Path, FALSE, 0, pCallback, hWnd, pPlayer))){
		pCallback->SetPlayer(*pPlayer);

		int CurrentPosition = SendMessage(hVolume, CSM_GETPOSITION, 0,0);
		float fUserVolume = (float)CurrentPosition / 255.f;
		(*pPlayer)->SetVolume(fUserVolume);
		(*pPlayer)->Play();
		wcscpy_s(LastPath, Path);
	}
}

// void PlayNextOrPrev(HWND hWnd, HWND hBtn, HWND hListView, HWND hVolume, PlayerCallback* pCallback, IMFPMediaPlayer** pPlayer, BOOL bNext){
void PlayNextOrPrev(HWND hWnd, HWND hBtn, HWND hListView, HWND hVolume, IMFPMediaPlayer* pPlayer, BOOL bNext){
	if(pPlayer){ 
		MFP_MEDIAPLAYER_STATE State;
		HRESULT hr = pPlayer->GetState(&State);
		if(State == MFP_MEDIAPLAYER_STATE_PLAYING || State == MFP_MEDIAPLAYER_STATE_PAUSED){
			pPlayer->Stop();
		}
	}
	SendMessage(hBtn, CBM_SETSTATE, UP, (LPARAM)0);

	int nCount = ListView_GetItemCount(hListView);
	int SelectItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);

	if(SelectItem == -1 && nCount != 0){
		SelectItem = 0;
	}

	if(SelectItem != -1){
		if(bNext){
			if(SelectItem + 1 >= nCount){
				SelectItem = 0;
			}else{
				SelectItem += 1;
			}
		}else{
			if(SelectItem - 1 <= -1){
				SelectItem = nCount - 1;
			}else{
				SelectItem -= 1;
			}
		}

		ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		ListView_SetItemState(hListView, SelectItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		SendMessage(hBtn, CBM_SETSTATE, DOWN, (LPARAM)0);
		SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTNFIRST + 1, PRESSED), (LPARAM)hBtn);
	}
}

BOOL SystemAudioCapture(const WCHAR* FileName, int Seconds){
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

	BOOL Success = FALSE;

	do{
		// 기본 렌더러 가져옴(스피커)
		if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator))){ break; }
		if(FAILED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))){ break; }

		// 기본 장치에서 AudioClient 인터페이스 활성화
		if(FAILED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient))){ break; }
		// 스테레오 믹스 오디오 포맷 가져옴
		if(FAILED(pAudioClient->GetMixFormat(&pwfx))){ break; }

		// 루프백 + 캡처 초기화
		if(FAILED(pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, NULL))){ break; }

		// 오디오 캡처 클라이언트 인터페이스 가져옴
		if(FAILED(pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient))){ break; }

		// 캡처 시작
		if(FAILED(pAudioClient->Start())){ break; }

		// WAV 헤더 비워두고 샘플 데이터 작성
		hFile = CreateFileW(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hFile == INVALID_HANDLE_VALUE){ break; }

		DWORD dwDummy = 0;
		WriteFile(hFile, &dwDummy, 44/* wav header */, &dwDummy, NULL);

		DWORD StartTime = GetTickCount();
		while(GetTickCount() - StartTime < (DWORD)(Seconds * 1000)){
			pCaptureClient->GetNextPacketSize(&PacketLength);

			while(PacketLength > 0){
				UINT32 NumFrames;
				pCaptureClient->GetBuffer(&pData, &NumFrames, &dwFlags, NULL, NULL);

				DWORD Written = 0;
				DWORD BytesToWrite = NumFrames * pwfx->nBlockAlign;
				WriteFile(hFile, pData, BytesToWrite, &Written, NULL);
				TotalBytesWritten += Written;

				pCaptureClient->ReleaseBuffer(NumFrames);
				pCaptureClient->GetNextPacketSize(&PacketLength);
			}
			Sleep(10);
		}
		pAudioClient->Stop();

		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		WriteWavHeader(hFile, TotalBytesWritten, pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);

		Success = TRUE;
	}while(FALSE);

	if(hFile != INVALID_HANDLE_VALUE){ CloseHandle(hFile); }
	if(pwfx){ CoTaskMemFree(pwfx); }
	if(pCaptureClient){ pCaptureClient->Release(); }
	if(pAudioClient){ pAudioClient->Release(); }
	if(pDevice){ pDevice->Release(); }
	if(pEnumerator){ pEnumerator->Release(); }

	return Success;
}

void WriteWavHeader(HANDLE hFile, DWORD DataSize, WORD Channels, DWORD SampleRate, WORD BitsPerSample){
	WAVHEADER Header;

	memcpy(Header.ChunkID, "RIFF", 4);
	Header.ChunkSize = 36 + DataSize;
	memcpy(Header.Format, "WAVE", 4);

	memcpy(Header.Subchunk1ID, "fmt ", 4);
	Header.Subchunk1Size = 16;
	Header.AudioFormat = 1;
	Header.NumChannels = Channels;
	Header.SampleRate = SampleRate;
	Header.ByteRate = SampleRate * Channels * BitsPerSample / 8;
	Header.BlockAlign = Channels * BitsPerSample / 8;
	Header.BitsPerSample = BitsPerSample;

	memcpy(Header.Subchunk2ID, "data", 4);
	Header.Subchunk2Size = DataSize;

	DWORD Written = 0;
	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	WriteFile(hFile, &Header, sizeof(WAVHEADER), &Written, NULL);
}

