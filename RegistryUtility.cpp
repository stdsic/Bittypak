#include <windows.h>

BOOL WriteRegistryData(HKEY hParentKey, LPCWSTR lpszPath, DWORD dwDesired, LPCWSTR lpszKeyName, DWORD dwType, PVOID Data, size_t Size){
	LONG	ret;
	HKEY	hSubKey;
	DWORD	dwDisposition;

	ret = RegCreateKeyEx(
			hParentKey,
			lpszPath,
			0,						// Reserved
			NULL,					// Class
			0,						// REG_OPTION_NON_VOLAILE
			dwDesired,
			NULL,					// LPSECURITY_ATTRIBUTES(Inherit)
			&hSubKey,
			&dwDisposition			// REG_CREATE_NEW_KEY, REG_OPENED_EXISTING_KEY
	);
	
	// lpszKeyName이 NULL이면 키 이름(=Value)은 "(기본값)"으로 설정된다.
	if(hSubKey){
		ret = RegSetValueEx(
				hSubKey,
				lpszKeyName,
				0,					// Reserved
				dwType,
				(CONST BYTE*)Data,
				Size				// REG_SZ인 경우 NULL종료 문자 포함한 길이 전달
		);

		RegCloseKey(hSubKey);
	}

	return (BOOL)(ret == ERROR_SUCCESS);
}

//	REG_SZ:								null-terminated string (문자열)
//	REG_MULTI_SZ:						multiple null-terminated strings (여러 null-terminated 문자열)
//	REG_EXPAND_SZ:						null-terminated string that can include environment variables (환경 변수를 포함할 수 있는 null-terminated 문자열)
//	REG_DWORD:							32-bit number (32비트 숫자)
//	REG_QWORD:							64-bit number (64비트 숫자)
//	REG_BINARY:							binary data (이진 데이터)
//	REG_NONE:							no defined value type (정의되지 않은 값 타입)
//	REG_LINK:							symbolic link (시스템 링크)
//	REG_RESOURCE_LIST:					resource list (자원 목록)
//	REG_FULL_RESOURCE_DESCRIPTOR:		full resource descriptor (자원 설명자)
//	REG_RESOURCE_REQUIREMENTS_LIST:		resource requirements list (자원 요구 사항 목록)

DWORD ReadRegistryData(HKEY hParentKey, LPCWSTR lpszPath, DWORD dwDesired, LPCWSTR lpszKeyName, PVOID Return, DWORD Size, INT_PTR nDefault){
	LONG ret;
	DWORD dwType, dwcbData = Size;

	HKEY	hSubKey;
	DWORD	dwDisposition;

	ret = RegOpenKeyEx(
			hParentKey,
			lpszPath,
			0,				 // ulOptions
			dwDesired,
			&hSubKey
	);

	// ret = RegCreateKeyEx(
	//		ParentKey,
	//		lpszPath,
	//		0,						// Reserved
	//		NULL,					// Class
	//		0,						// REG_OPTION_NON_VOLAILE
	//		dwDesired,
	//		NULL,					// LPSECURITY_ATTRIBUTES(Inherit)
	//		&hSubKey,
	//		&dwDisposition			// REG_CREATE_NEW_KEY, REG_OPENED_EXISTING_KEY
	// );

	// Get Type & cb
	// 버퍼를 지정했는데 크기가 충분하지 않으면 ERROR_MORE_DATA를 반환하고 필요한 버퍼 크기를 dwcbData에 저장한다.
	// 버퍼를 지정하지 않고 마지막 인수인 dwcbData를 지정한 경우 ERROR_SUCCESS를 반환하고 데이터 크기를 dwcbData에 바이트 단위로 저장한다.
	// lpszKeyName이 레지스트리에 없으면 ERROR_FILE_NOT_FOUND를 반환하고 버퍼에 아무런 값도 저장하지 않는다.
	if(hSubKey != NULL){
		// dwcbData는 입출력용 인수이므로 호출시 전달되는 버퍼의 크기를 입력해야 한다.
		ret = RegQueryValueEx(hSubKey, lpszKeyName, 0, &dwType, (LPBYTE)Return, &dwcbData);
		RegCloseKey(hSubKey);
	}

	if(ret != ERROR_SUCCESS){
		*((INT_PTR*)Return) = nDefault;
	}

	return (((ret) == ERROR_SUCCESS) ? (dwType) : REG_NONE);
}
