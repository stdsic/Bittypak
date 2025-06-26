#ifndef __REGISTRY_UTILITY_H_
#define __REGISTRY_UTILITY_H_

BOOL WriteRegistryData(HKEY hParentKey, LPCWSTR lpszPath, DWORD dwDesired, LPCWSTR lpszKeyName, DWORD dwType, PVOID Data, size_t Size);
DWORD ReadRegistryData(HKEY hParentKey, LPCWSTR lpszPath, DWORD dwDesired, LPCWSTR lpszKeyName, PVOID Return, DWORD Size, INT_PTR nDefault);

void SavePosition(HWND hWnd, HKEY hKey, LPCWSTR lpszPath);
void LoadPosition(HWND hWnd, HKEY hKey, LPCWSTR lpszPath);
#endif
