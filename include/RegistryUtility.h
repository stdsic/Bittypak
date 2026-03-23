#ifndef __REGISTRY_UTILITY_H_
#define __REGISTRY_UTILITY_H_
#include <windows.h>

BOOL DeleteRegistryValues(HKEY hParentKey, LPCWSTR hSubKey);
BOOL WriteRegistryData(HKEY hParentKey, LPCWSTR hSubKey, DWORD dwDesired, LPCWSTR lpszKeyName, DWORD dwType, PVOID Data, size_t Size);
DWORD ReadRegistryData(HKEY hParentKey, LPCWSTR hSubKey, DWORD dwDesired, LPCWSTR lpszKeyName, PVOID Return, DWORD Size, INT_PTR nDefault);

#endif
