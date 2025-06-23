#ifndef __CUSTOM_BUTTON_H_
#define __CUSTOM_BUTTON_H_
#include <windows.h>
#include <commctrl.h>
#define BTN_CLASS_NAME				L"MyCustomButton"
#define IDC_BTNFIRST				0x400

// 시스템 클래스의 윈도우 스타일 값 중에서 조합에만 반응하는 값을 재활용하기로 한다.
// WS_MINIMIZEBOX, WS_MAXIMIZEBOX, WS_TABSTOP, WS_GROUP, WS_SYSMENU
enum tag_ButtonIndex	{ STOP, START, BACKWARD, FORWARD, NEWFILE, PLAYLIST, BTN_COUNT };
enum tag_ButtonStyle	{ PUSH = 0x010000, CHECK = 0x020000 };			// 1, 2, 3, 8, 9, A, B
enum tag_ButtonMessage	{ CBM_SETSTATE = WM_USER + 1, CBM_GETSTATE };
enum tag_ButtonNotify	{ PRESSED, RELEASED };
enum tag_ButtonState	{ NORMAL, HOT, DOWN, UP, DISABLE };

#endif
