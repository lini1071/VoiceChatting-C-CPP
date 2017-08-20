#include "stdafx.h"
#include "AudioDeviceHandler.h"

#define _WIN32_DCOM

AudioDeviceHandler::AudioDeviceHandler
	(AudioPlayback **RESTRICT playback, AudioCapture **RESTRICT capture)
{
	HRESULT hr;

	hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), nullptr,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		(void**)&ptrEnumerator);

	// Register for notification callback
	if (ptrEnumerator->RegisterEndpointNotificationCallback(this))
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Error on AudioDeviceHandler::AudioDeviceHandler_RegisterEndpointNotificationCallback"));
		DEBUG_OUTPUT(TEXT(
			"Be advised : Device changed notification can't be received and auto switch will not be performed."));
#endif
	};

	// Get default audio endpoints and store them to inner pointers
	ptrEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eCommunications, &ptrRenderDevice);
	ptrEnumerator->GetDefaultAudioEndpoint(EDataFlow::eCapture, ERole::eCommunications, &ptrCaptureDevice);
};

AudioDeviceHandler::~AudioDeviceHandler(void)
{
	// Unregister for notification callback
	if (ptrEnumerator->UnregisterEndpointNotificationCallback(this))
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Error on AudioDeviceHandler::~AudioDeviceHandler_UnregisterEndpointNotificationCallback"));
#endif
	};

	// Release COM object pointer and uninitialize COM library.
	ptrEnumerator->Release();
	CoUninitialize();
};

HRESULT AudioDeviceHandler::Initialize(void)
{
	return S_OK;
};

// Getter of default playback device
// 
_Ret_maybenull_ IMMDevice* AudioDeviceHandler::GetDefaultPlaybackDevice(void)
{
	return ptrRenderDevice;
};
// Getter of default capture device
_Ret_maybenull_ IMMDevice* AudioDeviceHandler::GetDefaultCaptureDevice(void)
{
	return ptrCaptureDevice;
};


// IUnknown interface methods
ULONG STDMETHODCALLTYPE AudioDeviceHandler::AddRef(void)
{
	return InterlockedIncrement(&refCount);
};
ULONG STDMETHODCALLTYPE AudioDeviceHandler::Release(void)
{
	ULONG ulRef = InterlockedDecrement(&refCount);
	if (ulRef == 0) { delete this; }
	return ulRef;
};
HRESULT STDMETHODCALLTYPE AudioDeviceHandler::QueryInterface(REFIID riid, void **ppvObject)
{
	if (ppvObject == nullptr) { return E_POINTER; }

	*ppvObject = nullptr;
	if (riid == IID_IUnknown)
	{
		*ppvObject = static_cast<IUnknown *>(static_cast<IMMNotificationClient *>(this));
		AddRef();
	}
	else if (riid == __uuidof(IMMNotificationClient))
	{
		*ppvObject = static_cast<IMMNotificationClient *>(this);
		AddRef();
	}
	else { return E_NOINTERFACE; }

	return S_OK;
};


// Called when the default capture device changed.
HRESULT STDMETHODCALLTYPE AudioDeviceHandler::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR ptrDeviceId)
{
#ifdef _DEBUG
	TMCContainer tmc_device;
#endif

	if (role == ERole::eCommunications) switch (flow)
	{
		// Playback(render) device
		case (EDataFlow::eRender):
#ifdef _DEBUG
			TimeMeasureGetFreq(&tmc_device);
			TimeMeasureStart(&tmc_device);
#endif
			// Release previous interface, and get changed device
			ptrRenderDevice->Release();
			ptrEnumerator->GetDevice(ptrDeviceId, &ptrRenderDevice);

			if (*ppPlayback)
			{
				// Raise audio changed event, and set new device.
				(*ppPlayback)->SetEventAudioChanged();
				(*ppPlayback)->SetPlaybackDevice(ptrRenderDevice);

				// Raise event that audio change is completed.
				(*ppPlayback)->SetEventAudioChangeFinished();
			};

#ifdef _DEBUG
			TimeMeasureEnd(&tmc_device, "AudioDeviceHandler::OnDefaultDeviceChanged_render");
#endif
			break;


		// Capture device
		case (EDataFlow::eCapture):
#ifdef _DEBUG
			TimeMeasureGetFreq(&tmc_device);
			TimeMeasureStart(&tmc_device);
#endif
			// Release previous interface, and get changed device
			ptrCaptureDevice->Release();
			ptrEnumerator->GetDevice(ptrDeviceId, &ptrCaptureDevice);

			if (*ppCapture)
			{
				// Raise audio changed event, and set new device.
				// At this time if eventAudioChanged is set already,
				// we expect there are no side effects for this call.
				(*ppCapture)->SetEventAudioChanged();
				(*ppCapture)->SetCaptureDevice(ptrCaptureDevice);

				// Raise event that audio change is completed.
				(*ppCapture)->SetEventAudioChangeFinished();
			};

#ifdef _DEBUG
			TimeMeasureEnd(&tmc_device, "AudioDeviceHandler::OnDefaultDeviceChanged_capture");
#endif
			break;

		// No cases
		case (EDataFlow::eAll):
		case (EDataFlow::EDataFlow_enum_count):
			break;
	};

	return S_OK;
};
