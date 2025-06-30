#ifndef __RESROUCE_H_
#define __RESOURCE_H_

#include "CustomButton.h"
#include "CustomScrollBar.h"
#include "RegistryUtility.h"
#include "Color.h"
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
#include <shlwapi.h>
#include <shcore.h> // GetDpiForMonitor
#include <shellscalingapi.h>
#include <stdio.h>

#include <fftw3.h>
#include <math.h>
#include <random>

#define IDI_ICON1   101
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
	DWORD    AvgByteRate;
	WORD     BlockAlign;
	WORD     BitsPerSample;
	char     Subchunk2ID[4];
	DWORD    Subchunk2Size;
} WAVHEADER;
#pragma pack(pop)
#endif
