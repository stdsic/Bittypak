#include "CustomButton.h"

LRESULT CALLBACK CustomButtonProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

class CustomButtonRegister {
	public:
		CustomButtonRegister() {
			WNDCLASS twc;
			if (!GetClassInfo(GetModuleHandle(NULL), BTN_CLASS_NAME, &twc)) {
				WNDCLASSEX wcex = {
					sizeof(wcex),
					0,
					CustomButtonProc,
					0, sizeof(LONG_PTR),
					GetModuleHandle(NULL),
					NULL, LoadCursor(NULL, IDC_ARROW),
					NULL, // (HBRUSH)(COLOR_BTNFACE + 1),
					NULL,
					BTN_CLASS_NAME,
					NULL
				};
				RegisterClassEx(&wcex);
			}
			else {
				MessageBox(NULL, L"Class already exists.", L"Info - ButtonClass", MB_ICONINFORMATION);
			}
		}
}GLOBAL_CST_BUTTON_OBJECT;

struct tag_CustomButtonData {
	HBITMAP hBitmap;
	enum tag_ButtonIndex Index;
	enum tag_ButtonState State;
};

BOOL IsPtOnMe(HWND hWnd, LPARAM lParam);
HBITMAP GetStandardBitmap(HWND hParent);
void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBitmap);

LRESULT CALLBACK CustomButtonProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	static const int EDGEFRAME = 1;
	static enum tag_ButtonState CheckState;
	static HBITMAP hStandardBitmap;
	static HWND hParent;

	struct tag_CustomButtonData* pData = NULL, * pTemp = NULL;

	LPCREATESTRUCT pCS;
	HDC hdc, hMemDC;
	PAINTSTRUCT ps;
	RECT crt, srt, wrt;
	HGDIOBJ hOld;
	BITMAP bmp;
	DWORD dwStyle;

	HPEN hPen, hOldPen;
	HBRUSH hBrush, hOldBrush;
	int w, h, ID;

	WCHAR Debug[0x100];
	HWND hTarget;

	pData = (struct tag_CustomButtonData*)GetWindowLongPtr(hWnd, 0);

	switch (iMessage) {
		case WM_CREATE:
			hParent = GetParent(hWnd);
			pData = (tag_CustomButtonData*)calloc(1, sizeof(struct tag_CustomButtonData));
			for (int i = 0; i < BTN_COUNT; i++) {
				HWND hTarget = GetDlgItem(hParent, IDC_BTNFIRST + i);
				if (hTarget == hWnd) {
					pData->Index = (enum tag_ButtonIndex)i;
					pData->State = NORMAL;
					break;
				}
			}
			hStandardBitmap = GetStandardBitmap(hWnd);
			SetWindowLongPtr(hWnd, 0, (LONG_PTR)pData);
			return 0;

		case WM_LBUTTONDOWN:
			if ((GetWindowLongPtr(hWnd, GWL_STYLE) & CHECK) == 0) {
				pData->State = DOWN;
			}
			else {
				CheckState = pData->State;
				if (CheckState == NORMAL || CheckState == UP) {
					pData->State = DOWN;
				}
				else {
					pData->State = UP;
				}
			}
			InvalidateRect(hWnd, NULL, FALSE);
			SetCapture(hWnd);
			return 0;

		case WM_LBUTTONUP:
			if (GetCapture() == hWnd) {
				ReleaseCapture();

				if (IsPtOnMe(hWnd, lParam)) {
					if ((GetWindowLongPtr(hWnd, GWL_STYLE) & CHECK) == 0) {
						pData->State = UP;
						SendMessage(hParent, WM_COMMAND, MAKEWPARAM(GetWindowLongPtr(hWnd, GWLP_ID), PRESSED), (LPARAM)hWnd);
					}
					else {
						SendMessage(hParent, WM_COMMAND, MAKEWPARAM(GetWindowLongPtr(hWnd, GWLP_ID), pData->State == DOWN ? PRESSED : RELEASED), (LPARAM)hWnd);
					}
				}
				InvalidateRect(hWnd, NULL, FALSE);
			}
			return 0;

		case WM_MOUSEMOVE:
			if (GetCapture() == hWnd) {
				if ((GetWindowLongPtr(hWnd, GWL_STYLE) & CHECK) == 0) {
					if (IsPtOnMe(hWnd, lParam)) {
						pData->State = DOWN;
					}
					else {
						pData->State = UP;
					}
				}
				else {
					if (IsPtOnMe(hWnd, lParam)) {
						if (CheckState == NORMAL || CheckState == UP) {
							pData->State = DOWN;
						}
						else {
							pData->State = UP;
						}
					}
					else {
						pData->State = CheckState;
					}
				}

			}
			InvalidateRect(hWnd, NULL, FALSE);
			return 0;

		case WM_TIMER:
			return 0;

		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED) {
				if (pData->hBitmap != NULL) {
					DeleteObject(pData->hBitmap);
					pData->hBitmap = NULL;
				}
			}
			return 0;

		case WM_ENABLE:
			if (wParam == TRUE) {
				// 활성
			}
			else {
				// 비활성
			}
			return 0;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			hMemDC = CreateCompatibleDC(hdc);
			GetClientRect(hWnd, &crt);
			if (pData->hBitmap == NULL) {
				pData->hBitmap = CreateCompatibleBitmap(hdc, crt.right, crt.bottom);
			}
			hOld = SelectObject(hMemDC, pData->hBitmap);
			FillRect(hMemDC, &crt, GetSysColorBrush(COLOR_BTNFACE));

			// StringCbPrintf(Debug, sizeof(Debug), L"%d", pData->Index);
			// TextOut(hMemDC, 0, 0, Debug, wcslen(Debug));

			w = crt.right - crt.left;
			h = crt.bottom - crt.top;

			if (pData->State == DOWN) {
				// 눌린 상태: 테두리 스타일을 SUNKEN으로 변경
				DrawEdge(hMemDC, &crt, EDGE_SUNKEN, BF_RECT);

				// 내부 강조 효과: 눌림 느낌을 주기 위해 사각형 축소 및 어두운 색상
				//RECT innerRect = crt;
				//InflateRect(&innerRect, -crt.right / 6, -crt.bottom / 6); // 버튼 크기 조정
				//hBrush = CreateSolidBrush(RGB(100, 100, 100)); // 어두운 색상 브러시
				//hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);
				//FillRect(hMemDC, &innerRect, hBrush); 
				//DeleteObject(SelectObject(hMemDC, hOldBrush));
			}
			else if (pData->State == NORMAL || pData->State == UP) {
				// 기본 상태: 테두리 스타일을 RAISED로 설정
				DrawEdge(hMemDC, &crt, EDGE_RAISED, BF_RECT);
			}

			switch (pData->Index) {
				case STOP:
					{
						CopyRect(&srt, &crt);
						int rw = w / 6,
							rh = h / 6;
						InflateRect(&srt, -rw, -rh);

						hBrush = CreateSolidBrush(RGB(0, 0, 0));
						hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);
						Rectangle(hMemDC, srt.left, srt.top, srt.right, srt.bottom);
						DeleteObject(SelectObject(hMemDC, hOldBrush));
					}
					break;

				case START:
					{
						// 삼각형의 꼭짓점 좌표 설정
						if(pData->State == DOWN){
							RECT rect1 = {
								crt.left + w / 4,
								crt.top + h / 10,
								crt.left + w / 4 + w / 5,
								crt.bottom - h / 10
							};

							RECT rect2 = {
								crt.left + w / 2 + w / 10,
								crt.top + h / 10,
								crt.left + w / 2 + w / 10 + w / 5,
								crt.bottom - h / 10
							};

							hBrush = CreateSolidBrush(RGB(0, 0, 0));
							hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);
							Rectangle(hMemDC, rect1.left, rect1.top, rect1.right, rect1.bottom);
							Rectangle(hMemDC, rect2.left, rect2.top, rect2.right, rect2.bottom);
							DeleteObject(SelectObject(hMemDC, hOldBrush));
						}else{
							POINT points[] = {
								{crt.left + w / 4, crt.top + h / 4},    // 왼쪽 꼭짓점
								{crt.left + w / 4, crt.bottom - h / 4}, // 아래쪽 꼭짓점
								{crt.right - w / 4, crt.top + h / 2}    // 오른쪽 꼭짓점
							};

							// 브러시 설정 및 삼각형 그리기
							hBrush = CreateSolidBrush(RGB(0, 0, 0)); // 초록색 브러시
							hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);

							Polygon(hMemDC, points, 3); // 삼각형 그리기

							// 리소스 해제
							DeleteObject(SelectObject(hMemDC, hOldBrush));
						}
					}
					break;

				case BACKWARD:
					{// 첫 번째 삼각형 좌표, x값을 왼쪽으로 이동
						POINT triangle1[] = {
							{crt.right - w / 4 - w / 10, crt.top + h / 4},
							{crt.right - w / 4 - w / 10, crt.bottom - h / 4},
							{crt.right - w / 2 - w / 10, crt.top + h / 2}
						};

						// 두 번째 삼각형 좌표, x값을 왼쪽으로 이동
						POINT triangle2[] = {
							{crt.right - w / 2 - w / 10, crt.top + h / 4},
							{crt.right - w / 2 - w / 10, crt.bottom - h / 4},
							{crt.left + w / 4 - w / 10, crt.top + h / 2}
						};

						hBrush = CreateSolidBrush(RGB(0, 0, 0)); // 주황색 브러시
						hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);

						Polygon(hMemDC, triangle1, 3); // 첫 번째 삼각형 그리기
						Polygon(hMemDC, triangle2, 3); // 두 번째 삼각형 그리기

						DeleteObject(SelectObject(hMemDC, hOldBrush));
					}
					break;

				case FORWARD:
					{// 첫 번째 삼각형 좌표
						POINT triangle1[] = {
							{crt.left + w / 4, crt.top + h / 4},
							{crt.left + w / 4, crt.bottom - h / 4},
							{crt.left + w / 2, crt.top + h / 2}
						};

						// 두 번째 삼각형 좌표
						POINT triangle2[] = {
							{crt.left + w / 2, crt.top + h / 4},
							{crt.left + w / 2, crt.bottom - h / 4},
							{crt.right - w / 4, crt.top + h / 2}
						};

						hBrush = CreateSolidBrush(RGB(0, 0, 0)); // 파란색 브러시
						hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);

						Polygon(hMemDC, triangle1, 3); // 첫 번째 삼각형 그리기
						Polygon(hMemDC, triangle2, 3); // 두 번째 삼각형 그리기

						DeleteObject(SelectObject(hMemDC, hOldBrush));
					}
					break;

				case NEWFILE:
					{
						HDC hBmpDC = CreateCompatibleDC(hdc);
						hOld = SelectObject(hBmpDC, hStandardBitmap);
						BitBlt(hMemDC, (w - 16) / 2, (h - 16) / 2, 16, 16, hBmpDC, 0, 0, SRCCOPY);
						SelectObject(hBmpDC, hOld);
						DeleteDC(hBmpDC);
					}
					break;

				case PLAYLIST:
					{
						// 삼각형의 꼭짓점 좌표 설정
						POINT points[] = {
							{crt.left + w / 4, crt.top + h / 4},		// 왼쪽 꼭짓점
							{crt.right - w / 4, crt.top + h / 4},		// 오른쪽 꼭짓점
							{crt.left + w / 2, crt.bottom - h / 4}		// 하단 꼭짓점
						};

						// 브러시 설정 및 삼각형 그리기
						hBrush = CreateSolidBrush(RGB(0, 0, 0)); // 초록색 브러시
						hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);

						Polygon(hMemDC, points, 3); // 삼각형 그리기

						// 리소스 해제
						DeleteObject(SelectObject(hMemDC, hOldBrush));
					}
					break;
			}

			GetObject(pData->hBitmap, sizeof(BITMAP), &bmp);
			BitBlt(hdc, 0, 0, bmp.bmWidth, bmp.bmHeight, hMemDC, 0, 0, SRCCOPY);

			SelectObject(hMemDC, hOld);
			DeleteDC(hMemDC);
			EndPaint(hWnd, &ps);
			return 0;

		case CBM_SETSTATE:
			if ((GetWindowLongPtr(hWnd, GWL_STYLE) & CHECK) != 0) {
				pData->State = (tag_ButtonState)(wParam);
			}
			InvalidateRect(hWnd, NULL, FALSE);
			return 0;

		case CBM_GETSTATE:
			return pData->State;

		case WM_DESTROY:
			if (hStandardBitmap) { DeleteObject(hStandardBitmap); }
			if (pData->hBitmap) { DeleteObject(pData->hBitmap); }
			if (pData) { free(pData); }
			return 0;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}


BOOL IsPtOnMe(HWND hWnd, LPARAM lParam) {
	RECT wrt;
	POINT pt;

	pt.x = (int)(short)LOWORD(lParam);
	pt.y = (int)(short)HIWORD(lParam);
	ClientToScreen(hWnd, &pt);
	GetWindowRect(hWnd, &wrt);

	return PtInRect(&wrt, pt);
}

void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBitmap) {
	HDC hMemDC = CreateCompatibleDC(hdc);
	HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);

	BITMAP bmp;
	GetObject(hBitmap, sizeof(BITMAP), &bmp);

	BitBlt(hdc, x, y, bmp.bmWidth, bmp.bmHeight, hMemDC, 0, 0, SRCCOPY);

	SelectObject(hMemDC, hOld);
	DeleteDC(hMemDC);
}

HBITMAP GetStandardBitmap(HWND hParent) {
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_BAR_CLASSES; // 툴바 클래스 포함
	InitCommonControlsEx(&iccex);

	// TBBUTTON 정의
	TBBUTTON tbButtons[] = {
		{ MAKELONG(STD_FILENEW, 0), 0, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0 }
	};

	HWND hTempToolbar = CreateToolbarEx(
			hParent,				// Parent
			WS_CHILD,				// Style
			0x701,					// ID
			0,						// nBitmap
			HINST_COMMCTRL,			// 리소스를 가진 인스턴스 핸들
			IDB_STD_SMALL_COLOR,	// Standard Bitmap을 툴바에 로드
			tbButtons,				// TBBUTTON 구조체 배열
			sizeof(tbButtons) / sizeof(TBBUTTON),	// 툴 버튼 개수
			0, 0, 0, 0, sizeof(TBBUTTON)			// 버튼 및 비트맵 가로 세로 크기, 구조체 사이즈(버전 확인용)
			);

	if (hTempToolbar) {
		// TB_GETBITMAP 메시지를 사용하여 비트맵 얻기
		LRESULT bitmapIndex = SendMessage(hTempToolbar, TB_GETBITMAP, 0, 0); // 첫 번째 버튼의 비트맵 인덱스
		HIMAGELIST hImageList = (HIMAGELIST)SendMessage(hTempToolbar, TB_GETIMAGELIST, 0, 0);

		if (hImageList) {
			HBITMAP hBitmap = NULL;
			HICON hIcon = ImageList_GetIcon(hImageList, bitmapIndex, ILD_TRANSPARENT);
			if (hIcon) {
				ICONINFO If;
				if (GetIconInfo(hIcon, &If)) {
					hBitmap = If.hbmColor;
					DeleteObject(If.hbmMask);
				}
				DestroyIcon(hIcon);
				return hBitmap;
			}
		}
	}

	return NULL;
}

