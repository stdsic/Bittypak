#ifndef __BITTYPAK_H_
#define __BITTYPAK_H_
#include "resource.h"

HRESULT Initialize();
void Cleanup();

POINT GetMonitorCenter(HWND hWnd);
SIZE GetScaledWindowSize(HWND hWnd);
BOOL SetWindowCenter(HWND hParent, HWND hChild);

std::mt19937& GetRandomEngine();
int GetRandomInt(int Min, int Max);

void OpenFiles(HWND hWnd);
void AppendFile(HWND hListView, WCHAR* Path);

void TraverseDirTree(HWND hWnd, WCHAR* Path);
BOOL IsAudioFile(const WCHAR* Path);
BOOL MatchPattern(const WCHAR* Path, const WCHAR* Pattern);

BOOL WriteWavHeader(HANDLE hFile, DWORD DataSize, WORD Channels, DWORD SampleRate, WORD BitsPerSample);

void CreatePlaylist(HWND hWnd, WCHAR* Name, BOOL bDelete = FALSE);
BOOL DestroyPlaylist(HWND hWnd);
void SavePlaylist(HWND hWnd);
void LoadPlaylist(HWND hWnd);
void SavePosition(HWND hWnd, HKEY hKey, LPCWSTR lpszPath);
void LoadPosition(HWND hWnd, HKEY hKey, LPCWSTR lpszPath);

void PlaySelectedItem(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer **pPlayer, WCHAR* Return, BOOL bPaused = FALSE);
void PlayNextOrPrev(HWND hWnd, PlayerCallback* pCallback, IMFPMediaPlayer** pPlayer, BOOL bLoop, BOOL bRandom, BOOL bNext);

BOOL ShowInputPopup(HWND hParent, WCHAR* Out, int MaxLength, int iMode);

#endif
