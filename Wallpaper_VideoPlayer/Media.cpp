#include "Media.h"

RECT GetDisplayResolution()
{
	UINT32 PathNum, ModeNum;
	GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &PathNum, &ModeNum);
	DISPLAYCONFIG_PATH_INFO *ppath = new DISPLAYCONFIG_PATH_INFO[PathNum];
	DISPLAYCONFIG_MODE_INFO *pmode = new DISPLAYCONFIG_MODE_INFO[ModeNum];
	QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &PathNum, ppath, &ModeNum, pmode, NULL);
	int width = pmode[0].targetMode.targetVideoSignalInfo.activeSize.cx;
	int height = pmode[0].targetMode.targetVideoSignalInfo.activeSize.cy;
	delete[]ppath;
	delete[]pmode;
	return { 0, 0, width, height };
}

Wallpaper wall;

BOOL CALLBACK lpEnumWindow(HWND hwnd, LPARAM lparam)
{
	static HWND last = NULL;
	HWND progman = (HWND)lparam;
	if (hwnd == progman)
	{
		ShowWindow(last, SW_HIDE);
		return false;
	}
	else
	{
		last = hwnd;
		return true;
	}
}

Wallpaper::Wallpaper()
{
	ProgramManager = FindWindow(L"Progman", L"Program Manager");
	SendMessageTimeout(ProgramManager, 0x52c, 0, 0, SMTO_NORMAL, 1000, NULL);
	HWND last = NULL;
	EnumWindows(lpEnumWindow, (LPARAM)ProgramManager);
}

Wallpaper::~Wallpaper()
{
	SendMessageTimeout(ProgramManager, 0x52c, 1, 0, SMTO_NORMAL, 1000, NULL);
}

HWND Wallpaper::GetHWND()
{
	return ProgramManager;
}


template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}


MFCore::MFCore(HWND hVideo) :
	m_pMediaSession(NULL),
	m_pMediaSource(NULL),
	m_pVideoControl(NULL),
	m_hwndVideo(hVideo),
	m_hCloseEvent(NULL),
	m_nRefCount(1)
{
	MFStartup(MF_VERSION);
}


MFCore::~MFCore(void)
{
	if (m_pMediaSession)
		m_pMediaSession->Close();
	if (m_pMediaSource)
		m_pMediaSource->Shutdown();
	if (m_pMediaSession)
		m_pMediaSession->Shutdown();
}

HRESULT MFCore::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(MFCore, IMFAsyncCallback),
		{ 0 }
	};
	return QISearch(this, qit, riid, ppv);
}

ULONG MFCore::AddRef()
{
	return InterlockedIncrement(&m_nRefCount);
}

ULONG MFCore::Release()
{
	ULONG uCount = InterlockedDecrement(&m_nRefCount);
	if (uCount == 0)
		delete this;
	return uCount;
}

HRESULT MFCore::OpenFile(PCWSTR sURL)
{

	HRESULT hr;
	if (m_pMediaSession == NULL)
	{
		hr = MFCreateMediaSession(NULL, &m_pMediaSession);
		if (FAILED(hr))  return hr;
		hr = m_pMediaSession->BeginGetEvent((IMFAsyncCallback*)this, NULL);
		if (FAILED(hr))  return hr;
	}

	IMFSourceResolver* pSourceResolver = NULL;
	IUnknown* pUnknown = NULL;
	IMFTopology* pTopology = NULL;
	IMFPresentationDescriptor* pPD = NULL;

	MF_OBJECT_TYPE objType = MF_OBJECT_INVALID;
	SafeRelease(&m_pMediaSource);

	hr = MFCreateSourceResolver(&pSourceResolver);
	if (FAILED(hr))  goto over;

	hr = pSourceResolver->CreateObjectFromURL(
		sURL,
		MF_RESOLUTION_MEDIASOURCE,
		NULL,
		&objType,
		&pUnknown);
	if (FAILED(hr))  goto over;

	hr = pUnknown->QueryInterface(IID_PPV_ARGS(&m_pMediaSource));
	if (FAILED(hr))  goto over;

	DWORD cStreams = 0;

	hr = MFCreateTopology(&pTopology);
	if (FAILED(hr))  goto over;

	hr = m_pMediaSource->CreatePresentationDescriptor(&pPD);
	if (FAILED(hr))  goto over;

	hr = pPD->GetStreamDescriptorCount(&cStreams);
	if (FAILED(hr))  goto over;

	for (DWORD i = 0; i<cStreams; i++)
	{
		IMFStreamDescriptor* pSD = NULL;
		IMFTopologyNode* pSourceNode = NULL;
		IMFTopologyNode* pOutputNode = NULL;
		BOOL bSelected = FALSE;

		hr = pPD->GetStreamDescriptorByIndex(i, &bSelected, &pSD);
		if (FAILED(hr)) goto over2;
		if (bSelected)
		{
			hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode);
			if (FAILED(hr))  goto over2;
			hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, m_pMediaSource);
			if (FAILED(hr))  goto over2;
			hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
			if (FAILED(hr))  goto over2;
			hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
			if (FAILED(hr))  goto over2;

			IMFMediaTypeHandler* pHandler = NULL;
			IMFActivate* pRendererActivate = NULL;
			GUID guidMajorType = GUID_NULL;
			DWORD id = 0;

			pSD->GetStreamIdentifier(&id);
			hr = pSD->GetMediaTypeHandler(&pHandler);
			if (FAILED(hr))  goto over3;
			hr = pHandler->GetMajorType(&guidMajorType);
			if (FAILED(hr))  goto over3;
			hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);
			if (FAILED(hr))  goto over3;
			if (MFMediaType_Audio == guidMajorType)
				hr = MFCreateAudioRendererActivate(&pRendererActivate);
			else if (MFMediaType_Video == guidMajorType)
				hr = MFCreateVideoRendererActivate(m_hwndVideo, &pRendererActivate);
			else
				hr = E_FAIL;
			if (FAILED(hr))  goto over3;
			hr = pOutputNode->SetObject(pRendererActivate);
			if (FAILED(hr))  goto over3;

			hr = pTopology->AddNode(pSourceNode);
			if (FAILED(hr))  goto over3;
			hr = pTopology->AddNode(pOutputNode);
			if (FAILED(hr))  goto over3;
			hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
		over3:
			SafeRelease(&pRendererActivate);
			SafeRelease(&pHandler);
			goto over2;
		}

	over2:
		SafeRelease(&pSD);
		SafeRelease(&pSourceNode);
		SafeRelease(&pOutputNode);
	}

	hr = m_pMediaSession->SetTopology(0, pTopology);
	if (FAILED(hr))  goto over;

over:
	SafeRelease(&pPD);
	SafeRelease(&pTopology);
	SafeRelease(&pSourceResolver);
	SafeRelease(&pUnknown);
	return hr;
}

HRESULT MFCore::Invoke(IMFAsyncResult* pResult)
{
	MediaEventType meType = MEUnknown;
	IMFMediaEvent* pEvent = NULL;

	HRESULT hr = m_pMediaSession->EndGetEvent(pResult, &pEvent);
	if (FAILED(hr))  goto over;
	
	hr = pEvent->GetType(&meType);
	if (FAILED(hr))  goto over;

	if (meType != MESessionClosed)
	{
		hr = m_pMediaSession->BeginGetEvent(this, NULL);
		if (FAILED(hr))  goto over;
	}

	HRESULT hrStatus = S_OK;

	hr = pEvent->GetStatus(&hrStatus);
	if (FAILED(hr))  goto over;
	if (FAILED(hrStatus))
	{
		hr = hrStatus;
		goto over;
	}

	switch (meType)
	{
	case MESessionTopologyStatus:
		hr = OnTopologyStatus(pEvent);
		break;
	case MEEndOfPresentation:
		hr = Play();
		break;
	default:
		break;
	}

over:
	SafeRelease(&pEvent);
	return S_OK;
}

HRESULT MFCore::OnTopologyStatus(IMFMediaEvent* pEvent)
{
	MF_TOPOSTATUS status;
	HRESULT hr = pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, (UINT32*)&status);
	if (SUCCEEDED(hr) && (status == MF_TOPOSTATUS_READY))
	{
		SafeRelease(&m_pVideoControl);
		
		(void)MFGetService(m_pMediaSession, MR_VIDEO_RENDER_SERVICE,
			IID_PPV_ARGS(&m_pVideoControl));

		hr = Play();
	}
	return hr;
}

HRESULT MFCore::Play()
{
	PROPVARIANT varStart;
	PropVariantInit(&varStart);
	varStart.vt = VT_I8;
	varStart.lVal = 0;

	HRESULT hr = m_pMediaSession->Start(&GUID_NULL, &varStart);
	PropVariantClear(&varStart);

	return hr;
}

HRESULT MFCore::Repaint()
{
	if (m_pVideoControl)
		return m_pVideoControl->RepaintVideo();
	else
		return S_OK;
}
