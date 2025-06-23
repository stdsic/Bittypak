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
#include <mmdeviceapi.h>
#include <audioclient.h>
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

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf(" Error: %.2X.\n", hr); goto done; }
#define CHECKHR_GOTO(x, y) if(FAILED(x)) goto y

HRESULT ListAudioOutputDevices();
HRESULT GetAudioOutputDevice(UINT deviceIndex, IMFMediaSink** ppAudioSink);
// std::string GetMediaTypeDescription(IMFMediaType* pMediaType);

#endif
