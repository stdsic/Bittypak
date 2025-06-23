#ifndef __RESROUCE_H_
#define __RESOURCE_H_

#include "CustomButton.h"
#include "CustomScrollBar.h"
// #include <windows.h>
// #include <commctrl.h>

// #include <strsafe.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
// #include <mfobjects.h>
// #include <mfmediaengine.h>

// 시스템 사운드(스피커 오디오) 캡처
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

// #include <atlbase.h>
#include <wmcodecdsp.h>
#include <wmsdkidl.h>
#include <shellapi.h>
#include <shcore.h> // GetDpiForMonitor
#include <shellscalingapi.h>
#include <stdio.h>

template <class T> void SAFE_RELEASE(T** ppT){
	if (*ppT){
		(*ppT)->Release();
		*ppT = NULL;
	}
}

template <class T> inline void SAFE_RELEASE(T*& pT){
	if (pT != NULL){
		pT->Release();
		pT = NULL;
	}
}

#pragma pack(push, 1)
typedef struct {
	char     ChunkID[4];
	DWORD    ChunkSize;
	char     Format[4];
	char     Subchunk1ID[4];
	DWORD    Subchunk1Size;
	WORD     AudioFormat;
	WORD     NumChannels;
	DWORD    SampleRate;
	DWORD    ByteRate;
	WORD     BlockAlign;
	WORD     BitsPerSample;
	char     Subchunk2ID[4];
	DWORD    Subchunk2Size;
} WAVHEADER;
#pragma pack(pop)
#endif
