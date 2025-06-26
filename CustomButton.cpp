#include "CustomButton.h"
#include "Color.h"
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

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
void DrawSpectrumCircle(HDC hdc, POINT Origin, int Radius);

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

				case CAPTURE:
					{
						hBrush = CreateSolidBrush(RGB(255,0,0));
						hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);

						POINT Origin = { (crt.right - crt.left) / 2, (crt.bottom - crt.top) / 2 };
						Ellipse(hMemDC, Origin.x - Origin.x / 2, Origin.y - Origin.y / 2, Origin.x + Origin.x / 2, Origin.y + Origin.y / 2);
						DeleteObject(SelectObject(hMemDC, hOldBrush));
					}
					break;

				case SPECTRUM:
					{
						GetClientRect(hWnd, &crt);
						int iWidth = crt.right - crt.left;
						int iHeight = crt.bottom - crt.top;
						int iRadius = min(iWidth, iHeight) / 2;
						POINT Origin = { iWidth / 2, iHeight / 2 };
						DrawSpectrumCircle(hMemDC, Origin, iRadius);
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

void DrawSpectrumCircle(HDC hdc, POINT Origin, int Radius){
	// 버튼 크기가 16x16
	// 중심이 8,8에 반지름이 8 픽셀
	// 원의 둘레는 2 * PI * R이므로 약 50.26
	// 즉 현재 버튼 영역에 맞게 원을 그린다고 가정할 때, 이 원의 둘레를 따라 최대 50개의 픽셀을 표현할 수 있다.
	// 곧, 원의 둘레에만 색상을 표현한다고 해도 최소 간격이 1픽셀 이상이어야 한다.

	// 반지름 8픽셀, 원의 중심각을 델타 세타(rad)로 잡고 중심각에 대응하는 호의 길이를 구해보면 다음과 같다.
	// 픽셀 단위에서의 좌표간의 거리(=호의 길이): Radius * 중심각

	// 두 점이 너무 가까우면 정수화시 같은 픽셀 좌표가 되므로 최소 2 ~ 4픽셀 차이가 나게끔 잡는다.
	// 따라서, 중심각을 0.3 ~ 0.5(rad)로 하면 2 ~ 4픽셀 차이가 나고 제대로 표현 가능하다.
	
	// 0.3 ~ 0.5(rad)는 약 20도에서 30도 사이이므로 최소 20도로 잡고 원을 조각내보면 18 ~ 12조각으로 나눌 수 있다.
	// 여기서는 실제로 가장 안전한 값인 12조각으로 나눈다.

	// 실행해보니 역시 인생사 기합과 계산만으로 전부 해결되진 않는다.
	// 이쁘지 않으므로 8 ~ 10 조각으로 나누기로 하자.
	const int Slices = 8;
	double PI = atan(1.0) * 4.0;

	int Left   = Origin.x - Radius;
	int Top    = Origin.y - Radius;
	int Right  = Origin.x + Radius;
	int Bottom = Origin.y + Radius;

	for(int i = 0; i < Slices; i++){
		float Hue = (float)i / (float)(Slices-1);
		Color c(Hue, 1.0f, 0.85f, TRUE);
		COLORREF color = c.ToColor().ToColorRef();

		// 중심각(=rad)
		double AngleOne = (i * 45) * PI / 180.0;
		double AngleTwo = ((i + 1) * 45) * PI / 180.0;

		int x1 = (int)(Origin.x + Radius * cos(AngleOne));
		int y1 = (int)(Origin.y - Radius * sin(AngleOne));
		int x2 = (int)(Origin.x + Radius * cos(AngleTwo));
		int y2 = (int)(Origin.y - Radius * sin(AngleTwo));

		if(x1 == x2 && y1 == y2){ continue; }
		// Pie는 사각형 기준의 원으로 그리므로 바운딩 박스가 필요하다
		HBRUSH hBrush = CreateSolidBrush(color);
		HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

		// 각도 기준으로 원형 조각 그리기
		Pie(hdc, Left, Top, Right, Bottom, x1, y1, x2, y2);

		SelectObject(hdc, hOldBrush);
		DeleteObject(hBrush);
	}
}
