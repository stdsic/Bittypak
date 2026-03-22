#ifndef __RESOURCE_H_
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
#include <initguid.h>
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

// User-defined message
#define WM_PLAYNEXT					WM_APP + 1
#define WM_NEWPOSITION				WM_APP + 2

#define KEY_PATH_POSITION			L"Software\\Bittypak\\InitInfo\\LastPosition"
#define KEY_PATH_PLAYLIST           L"Software\\Bittypak\\InitInfo\\Playlist"

// ComboBox
#define IDC_CBFIRST					0x500

// ListView
#define IDC_LVFIRST					0x600

// Context Menu
#define IDM_ITEM_DELETE				0x900
#define IDM_CREATE_PLAYLIST         0x901
#define IDM_DELETE_PLAYLIST			0x902
#define IDM_RANDOM_PLAY             0x903
#define IDM_LOOP_PLAY               0x904

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

// 장치 변경 콜백
class DeviceCallback : public IMMNotificationClient {
    private:
        LONG RefCount;
        HANDLE hDeviceChangedEvent;
    public:
        DeviceCallback(HANDLE hEvent) : RefCount(1), hDeviceChangedEvent(hEvent) {;}
        virtual ~DeviceCallback() {;}

        virtual ULONG __stdcall AddRef(){
            return InterlockedIncrement(&RefCount);
        }
        virtual ULONG __stdcall Release(){
            ULONG Count = InterlockedDecrement(&RefCount);
            if(Count == 0){ delete this; }
            return Count;
        }
        virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvInterface){
            if(ppvInterface == NULL){ return E_POINTER; }

            if(riid == IID_IUnknown || riid == IID_IMMNotificationClient){
                *ppvInterface = (IMMNotificationClient*)(this);
                AddRef();
                return S_OK;
            }

            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }

        virtual HRESULT __stdcall OnDefaultDeviceChanged(EDataFlow Flow, ERole Role, LPCWSTR lpwszDeviceID){
            if(Flow == eRender && Role == eConsole){
                SetEvent(hDeviceChangedEvent);
            }
            return S_OK;
        }
        virtual HRESULT __stdcall OnDeviceAdded(LPCWSTR lpwszDeviceID){
            return S_OK;
        }
        virtual HRESULT __stdcall OnDeviceRemoved(LPCWSTR lpwszDeviceID){
            return S_OK;
        }
        virtual HRESULT __stdcall OnDeviceStateChanged(LPCWSTR lpwszDeviceID, DWORD dwNewState){
            return S_OK;
        }
        virtual HRESULT __stdcall OnPropertyValueChanged(LPCWSTR lpwszDeviceID, const PROPERTYKEY Key){
            return S_OK;
        }
};

// 플레이어 콜백 클래스
class PlayerCallback : public IMFPMediaPlayerCallback{
    LONG RefCount = 1;
    HWND hWnd;
    IMFPMediaPlayer* pPlayer = NULL;
    WCHAR Debug[0x100];
    MFP_MEDIAPLAYER_STATE CurrentState = MFP_MEDIAPLAYER_STATE_EMPTY;

    public:
    virtual ULONG __stdcall AddRef() 
    {
        return InterlockedIncrement(&RefCount);
    }

    virtual ULONG __stdcall Release()
    {
        ULONG Count = InterlockedDecrement(&RefCount);
        if(Count == 0)
        {
            delete this;
        }

        return Count;
    }

    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppv)
    {
        if(riid == __uuidof(IUnknown) || riid == __uuidof(IMFPMediaPlayerCallback))
        {
            *ppv = (IMFPMediaPlayerCallback*)this;
            AddRef();
            return S_OK;
        }

        *ppv = NULL;
        return E_NOINTERFACE;
    }

    virtual void __stdcall OnMediaPlayerEvent(MFP_EVENT_HEADER* pEventHeader)
    {
        if(pPlayer == NULL)
        { 
            return;
        }

        switch(pEventHeader->eEventType)
        {
            case MFP_EVENT_TYPE_PLAY:
                pPlayer->GetState(&CurrentState);
                KillTimer(hWnd, 1);
                SetTimer(hWnd, 1, 50, NULL);
                break;

            case MFP_EVENT_TYPE_PAUSE:
                pPlayer->GetState(&CurrentState);
                KillTimer(hWnd, 1);
                break;

            case MFP_EVENT_TYPE_STOP:
                pPlayer->GetState(&CurrentState);
                KillTimer(hWnd, 1);
                break;

            case MFP_EVENT_TYPE_POSITION_SET:
                pPlayer->GetState(&CurrentState);
                PostMessage(hWnd, WM_NEWPOSITION, 0, 0);
                break;

            case MFP_EVENT_TYPE_RATE_SET:
                break;

            case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
                pPlayer->GetState(&CurrentState);
                break;

            case MFP_EVENT_TYPE_MEDIAITEM_CLEARED:
                // EMPTY
                pPlayer->GetState(&CurrentState);
                break;

            case MFP_EVENT_TYPE_MEDIAITEM_SET:
                if(pPlayer){
                    MFP_MEDIAPLAYER_STATE State;
                    HRESULT hr = pPlayer->GetState(&State);

                    if(SUCCEEDED(hr))
                    {
                        switch(State){
                            case MFP_MEDIAPLAYER_STATE_EMPTY:
                                break;

                            case MFP_MEDIAPLAYER_STATE_STOPPED:
                                break;

                            case MFP_MEDIAPLAYER_STATE_PLAYING:
                                break;

                            case MFP_MEDIAPLAYER_STATE_PAUSED:
                                break;

                            default:
                                wprintf(L"플레이어 상태: 알 수 없음\n");
                                break;
                        }
                    }
                    else
                    {
                        wsprintf(Debug, L"GetState 호출 실패: 0x%08X\n", hr);
                        MessageBox(hWnd, Debug, L"Error", MB_OK | MB_ICONERROR);
                    }
                }
                break;

            case MFP_EVENT_TYPE_FRAME_STEP:
                break;

            case MFP_EVENT_TYPE_MF:
                break;

            case MFP_EVENT_TYPE_ERROR:
                pPlayer->GetState(&CurrentState);
                wsprintf(Debug, L"오류 발생: 0x%08X\n", pEventHeader->hrEvent);
                MessageBox(hWnd, Debug, L"Error", MB_OK | MB_ICONERROR);
                break;

            case MFP_EVENT_TYPE_PLAYBACK_ENDED:
                pPlayer->GetState(&CurrentState);
                PostMessage(hWnd, WM_PLAYNEXT, 0, 0);
                break;

            case MFP_EVENT_TYPE_ACQUIRE_USER_CREDENTIAL:
                break;
        }
    }

    void SetPlayer(IMFPMediaPlayer* pNewPlayer) 
    {
        if(pPlayer)
        {
            pPlayer->Release();
            pPlayer = NULL;
        }

        if(pNewPlayer)
        {
            pNewPlayer->AddRef();
            pPlayer = pNewPlayer;
        }
    }

    MFP_MEDIAPLAYER_STATE GetCurrentState()
    {
        return CurrentState;
    }

    public:
    PlayerCallback(HWND _hWnd) : hWnd(_hWnd) {;}
    ~PlayerCallback() 
    {
        if(pPlayer){
            pPlayer->Release();
            pPlayer = NULL;
        }
    }
};
#endif
