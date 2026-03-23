#ifndef __CUSTOM_BUTTON_H_
#define __CUSTOM_BUTTON_H_
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#define BTN_CLASS_NAME				L"MyCustomButton"
#define IDC_BTNSTOP             0x400
#define IDC_BTNPLAY             IDC_BTNSTOP + 1
#define IDC_BTNPREV             IDC_BTNSTOP + 2	
#define IDC_BTNNEXT             IDC_BTNSTOP + 3	
#define IDC_BTNOPENFILE         IDC_BTNSTOP + 4	
#define IDC_BTNVIEW             IDC_BTNSTOP + 5	
#define IDC_BTNRECORD           IDC_BTNSTOP + 6	
#define IDC_BTNTIMER            IDC_BTNSTOP + 7	
#define IDC_BTNSPECTRUM         IDC_BTNSTOP + 8	

// 시스템 클래스의 윈도우 스타일 값 중에서 조합에만 반응하는 값을 재활용하기로 한다.
// WS_MINIMIZEBOX, WS_MAXIMIZEBOX, WS_TABSTOP, WS_GROUP, WS_SYSMENU
enum tag_ButtonIndex	{ STOP, START, BACKWARD, FORWARD, NEWFILE, PLAYLIST, CAPTURE, TIMER, SPECTRUM, BTN_COUNT };
enum tag_ButtonStyle	{ PUSH = 0x010000, CHECK = 0x020000 };			// 1, 2, 3, 8, 9, A, B
enum tag_ButtonMessage	{ CBM_SETSTATE = WM_USER + 1, CBM_GETSTATE };
enum tag_ButtonNotify	{ PRESSED, RELEASED };
enum tag_ButtonState	{ NORMAL, HOT, DOWN, UP, DISABLE };

#endif
