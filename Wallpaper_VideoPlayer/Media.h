#pragma once

#include<Windows.h>

#include <mfapi.h>    // MFStartup, mfplat.lib  
#include <mfidl.h>    // MFCreateMediaSession, mf.lib  
#include <evr.h>  // IMFVideoDisplayControl, strmiids.lib  
#include <shlwapi.h>  // QITABENT, shlwapi.lib  
#include <mferror.h>  // MF_E_ALREADY_INITIALIZED  

RECT GetDisplayResolution();

class Wallpaper
{
public:
	Wallpaper();
	~Wallpaper();
	HWND GetHWND();
private:
	HWND ProgramManager;
};

extern Wallpaper wall;

//Copy from MSDN

class MFCore : public IMFAsyncCallback
{
public:
	MFCore(HWND);
	~MFCore();
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP GetParameters(DWORD*, DWORD*)
	{ return E_NOTIMPL;	}
	STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

	HRESULT OpenFile(PCWSTR sURL);

	HRESULT Play();
	HRESULT Repaint();

	HRESULT OnTopologyStatus(IMFMediaEvent*);

private:
	long                    m_nRefCount;
	IMFMediaSession*        m_pMediaSession;
	IMFMediaSource*         m_pMediaSource;
	IMFVideoDisplayControl* m_pVideoControl;
	HWND                    m_hwndVideo;
	HANDLE                  m_hCloseEvent;
};