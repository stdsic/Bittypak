#include "CustomScrollBar.h"
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

LRESULT CALLBACK CustomScrollBarWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

struct GSCROLLBARINFO {
	int Min;                // 최소 범위
	int Max;                // 최대 범위
	int Position;           // 현재 Thumb의 위치
	int ThumbSize;          // 스크롤 트랙 영역
	int ThumbGap;           // Thumb 크기
	COLORREF BkColor;       // 스크롤 트랙 영역의 색상
	COLORREF ThumbColor;	// Thumb 색상
};

class CustomScrollBar {
	public:
		CustomScrollBar() {
			WNDCLASS twc;
			if (!GetClassInfo(GetModuleHandle(NULL), SCROLLBAR_CLASS_NAME, &twc)) {
				WNDCLASS wc = {
					CS_HREDRAW | CS_VREDRAW,
					CustomScrollBarWndProc,
					0, sizeof(LONG_PTR),
					GetModuleHandle(NULL),
					NULL, LoadCursor(NULL, IDC_ARROW),
					NULL,
					NULL,
					SCROLLBAR_CLASS_NAME
				};

				RegisterClass(&wc);
			}
			else {
				MessageBox(NULL, L"Class already exists.", L"Info - ScrollBarClass", MB_ICONINFORMATION);
			}
		}
} GLOBARL_CST_SCROLLBAR_OBJECT;

void PosFromPixel(HWND hWnd, int Pixel);
void GetThumbRect(HWND hWnd, RECT* trt);

LRESULT CALLBACK CustomScrollBarWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	HBITMAP hBitmap;
	HGDIOBJ hOld;

	PAINTSTRUCT ps;
	HDC hdc, hMemDC;

	RECT crt, trt;
	static HPEN hPen[5];
	POINT pt;
	static int offset;

	GSCROLLBARINFO* pInfo = (GSCROLLBARINFO*)GetWindowLongPtr(hWnd, 0);
	switch (iMessage) {
		case WM_CREATE:
			// 유연성 확보를 위해 첫 번째 인수를 구조체 크기로 전달
			pInfo = (GSCROLLBARINFO*)calloc(1, sizeof(GSCROLLBARINFO));

			// 기본값 설정
			// 단, 윈도우 생성시 크기를 0으로 전달하면 앱 활성 후 재설정 필요
			pInfo->BkColor = GetSysColor(COLOR_BTNFACE);
			pInfo->ThumbColor = RGB(255, 255, 255);
			pInfo->ThumbSize = 20;
			pInfo->ThumbGap = 2;
			SetWindowLongPtr(hWnd, 0, (LONG_PTR)pInfo);

			if (hPen[0] == NULL) {
				for (int i = 0; i < 5; i++) {
					hPen[i] = CreatePen(PS_SOLID, 1, RGB(i * 32 + 32, i * 32 + 32, i * 32 + 32));
				}
			}
			return 0;

		case CSM_SETRANGE:
			pInfo->Min = wParam;
			pInfo->Max = lParam;

			// 범위 변경 후 다시 그려야 됨
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case CSM_GETRANGEMIN:
			return pInfo->Min;

		case CSM_GETRANGEMAX:
			return pInfo->Max;

		case CSM_SETPOSITION:
			// 범위 벗어나지 않도록 설정
			pInfo->Position = max(pInfo->Min, min(pInfo->Max, wParam));
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case CSM_GETPOSITION:
			return pInfo->Position;

		case CSM_SETTHUMBSIZE:
			GetClientRect(hWnd, &crt);

			pInfo->ThumbSize = wParam;
			if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
				pInfo->ThumbSize = max(4, min(crt.right - 10, pInfo->ThumbSize));
			}
			else {
				pInfo->ThumbSize = max(4, min(crt.bottom - 10, pInfo->ThumbSize));
			}
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case CSM_GETTHUMBSIZE:
			return pInfo->ThumbSize;

		case CSM_SETGAP:
			GetClientRect(hWnd, &crt);
			pInfo->ThumbGap = wParam;

			if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
				pInfo->ThumbGap = max(0, min(crt.bottom / 2 - 2, pInfo->ThumbGap));
			}
			else {
				pInfo->ThumbGap = max(0, min(crt.right / 2 - 2, pInfo->ThumbGap));
			}
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case CSM_GETGAP:
			return pInfo->ThumbGap;

		case CSM_SETTHUMBCOLOR:
			pInfo->ThumbColor = (COLORREF)wParam;
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case CSM_SETBKCOLOR:
			pInfo->BkColor = (COLORREF)wParam;
			InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case WM_SIZE:
			SendMessage(hWnd, CSM_SETTHUMBSIZE, pInfo->ThumbSize, 0);
			SendMessage(hWnd, CSM_SETGAP, pInfo->ThumbGap, 0);
			return 0;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			GetClientRect(hWnd, &crt);
			hMemDC = CreateCompatibleDC(hdc);
			hBitmap = CreateCompatibleBitmap(hdc, crt.right, crt.bottom);
			hOld = SelectObject(hMemDC, hBitmap);

			{
				HBRUSH hBrush = CreateSolidBrush(pInfo->BkColor);
				FillRect(hMemDC, &crt, hBrush);
				DeleteObject(hBrush);

				if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
					for (int i = 0; i < 5; i++) {
						SelectObject(hMemDC, hPen[i]);
						MoveToEx(hMemDC, crt.left, crt.bottom / 2 - 2 + i, NULL);
						LineTo(hMemDC, crt.right, crt.bottom / 2 - 2 + i);
					}
				}
				else {
					for (int i = 0; i < 5; i++) {
						SelectObject(hMemDC, hPen[i]);
						MoveToEx(hMemDC, crt.right / 2 - 2 + i, crt.top, NULL);
						LineTo(hMemDC, crt.right / 2 - 2 + i, crt.bottom);
					}
				}

				GetThumbRect(hWnd, &trt);
				SelectObject(hMemDC, GetStockObject(BLACK_PEN));
				HBRUSH ThumbBrush = CreateSolidBrush(pInfo->ThumbColor);
				HBRUSH OldBrush = (HBRUSH)SelectObject(hMemDC, ThumbBrush);
				RoundRect(hMemDC, trt.left, trt.top, trt.right, trt.bottom, 10, 10);
				DeleteObject(SelectObject(hMemDC, OldBrush));
			}

			BitBlt(hdc, 0, 0, crt.right, crt.bottom, hMemDC, 0, 0, SRCCOPY);

			SelectObject(hMemDC, hOld);
			DeleteObject(hBitmap);
			DeleteDC(hMemDC);
			EndPaint(hWnd, &ps);
			return 0;

		case WM_LBUTTONDOWN:
			GetThumbRect(hWnd, &trt);
			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);
			if (PtInRect(&trt, pt)) {
				if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
					offset = trt.left - LOWORD(lParam);
				}
				else {
					offset = trt.top - HIWORD(lParam);
				}
				SetCapture(hWnd);
			}
			else {
				if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
					PosFromPixel(hWnd, LOWORD(lParam) - pInfo->ThumbSize / 2);
				}
				else {
					PosFromPixel(hWnd, HIWORD(lParam) - pInfo->ThumbSize / 2);
				}

				SendMessage(GetParent(hWnd), (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 0), (LPARAM)hWnd);
			}
			return 0;

		case WM_LBUTTONUP:
			if (GetCapture() == hWnd) {
				ReleaseCapture();
				SendMessage(GetParent(hWnd), (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 0), (LPARAM)hWnd);
			}
			return 0;

		case WM_MOUSEMOVE:
			if (GetCapture() == hWnd) {
				GetClientRect(hWnd, &crt);

				if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
					PosFromPixel(hWnd, (int)(short)LOWORD(lParam) + offset);
				}
				else {
					PosFromPixel(hWnd, (int)(short)HIWORD(lParam) + offset);
				}

				SendMessage(GetParent(hWnd), (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_THUMBTRACK, 0), (LPARAM)hWnd);
			}
			return 0;

		case WM_DESTROY:
			if (hPen) {
				for (int i = 0; i < 5; i++) {
					DeleteObject(hPen[i]);
					hPen[0] = NULL;
				}
			}
			if (pInfo) { free(pInfo); }
			return 0;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

void GetThumbRect(HWND hWnd, RECT* trt)
{
	GSCROLLBARINFO* pInfo;
	RECT crt;
	int x, y;

	pInfo = (GSCROLLBARINFO*)GetWindowLongPtr(hWnd, 0);
	GetClientRect(hWnd, &crt);

	if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
		x = MulDiv(pInfo->Position, crt.right - pInfo->ThumbSize, pInfo->Max - pInfo->Min);
		SetRect(trt, x, crt.top + pInfo->ThumbGap, x + pInfo->ThumbSize, crt.bottom - pInfo->ThumbGap);
	}
	else {
		y = MulDiv(pInfo->Position, crt.bottom - pInfo->ThumbSize, pInfo->Max - pInfo->Min);
		SetRect(trt, crt.left + pInfo->ThumbGap, y, crt.right - pInfo->ThumbGap, y + pInfo->ThumbSize);
	}
}

void PosFromPixel(HWND hWnd, int Pixel)
{
	GSCROLLBARINFO* pInfo;
	RECT crt;
	int Width;

	pInfo = (GSCROLLBARINFO*)GetWindowLongPtr(hWnd, 0);
	GetClientRect(hWnd, &crt);

	if (GetWindowLongPtr(hWnd, GWL_STYLE) & CSS_HORZ) {
		Width = crt.right - pInfo->ThumbSize;
	}
	else {
		Width = crt.bottom - pInfo->ThumbSize;
	}

	pInfo->Position = MulDiv(Pixel, pInfo->Max - pInfo->Min, Width);
	pInfo->Position = max(pInfo->Min, min(pInfo->Max, pInfo->Position));
	InvalidateRect(hWnd, NULL, TRUE);
}
