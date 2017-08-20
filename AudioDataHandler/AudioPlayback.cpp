#include "stdafx.h"
#include "AudioPlayback.h"

#ifdef _MMCSS
	#include <avrt.h>
	#pragma comment(lib, "avrt.lib")
#endif

AudioPlayback::AudioPlayback(AudioDataQueue* queue) :
	ptrQueue(queue),

	ptrDevice(nullptr),
	ptrClient(nullptr),
	ptrRenderClient(nullptr),
	numEventHandle(0U),
	handlePlaybackThread(nullptr),
	initedSyncObjectsCount(0U),
	initedComponentsCount(0U)
{

};

AudioPlayback::~AudioPlayback(void)
{
	if (handlePlaybackThread)
	{
		SetEvent(eventAudioStop);
		WaitForSingleObject(handlePlaybackThread, INFINITE);
	};

	if (!mapEventHandle.empty())
	{
		for (EventHandleMap::const_iterator it
			= mapEventHandle.cbegin(); it != mapEventHandle.cend(); ++it)
			CloseHandle(it->second);
		mapEventHandle.clear();
	};
};

// This function raises events represents that default device is changed.
// Call this function to raise event when changing playback device is begun.
int	AudioPlayback::SetEventAudioChanged(void)
{
	return SetEvent(eventAudioChanged);
};

// This function raises events represents that default device has replaced.
// Call this function to raise event when changing playback device is finished.
int	AudioPlayback::SetEventAudioChangeFinished(void)
{
	return SetEvent(eventAudioChangeFinished);
};


// May return stored playback device pointer.
// The device pointer is null when this object is just created.
_Ret_maybenull_ IMMDevice*	AudioPlayback::GetPlaybackDevice(void)
{
	return ptrDevice;
};

// Set inner playback device pointer to argument.
HRESULT		AudioPlayback::SetPlaybackDevice(_In_ IMMDevice * pDev)
{
	// If there is active previous device, release it first.
	if (pDev != ptrDevice) ptrDevice->Release();

	ptrDevice = pDev;
	return (pDev ? S_OK : E_POINTER);
};


// Creates some objects which are used in synchronization.
// For Windows, CRITICAL_SECTION and event HANDLEs.
unsigned long AudioPlayback::InitializeSyncObjects(void)
{
	// Create mutex object
	InitializeCriticalSection(&mutexHandleMap);
	IncreaseInitedSyncObjectsCount();	// case 1

	// Create Event Instances for maintaining capture thread
	eventAudioReady = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (eventAudioReady == nullptr)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Fatal error on AudioPlayback::InitializeSyncObjects_CreateEventEx_eventAudioReady"));
#endif
		return GetLastError();
	};
	IncreaseInitedSyncObjectsCount();	// case 2

	eventAudioChanged = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (eventAudioChanged == nullptr)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Fatal error on AudioPlayback::InitializeSyncObjects_CreateEventEx_eventAudioChanged"));
#endif
		return GetLastError();
	};
	IncreaseInitedSyncObjectsCount();	// case 3

	eventAudioChangeFinished = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (eventAudioChangeFinished == nullptr)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Fatal error on AudioPlayback::InitializeSyncObjects_CreateEventEx_eventAudioChangeFinished"));
#endif
		return GetLastError();
	};
	IncreaseInitedSyncObjectsCount();	// case 4

	eventAudioStop = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (eventAudioStop == nullptr)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Fatal error on AudioPlayback::InitializeSyncObjects_CreateEventEx_eventAudioStop"));
#endif
		return GetLastError();
	};
	IncreaseInitedSyncObjectsCount();	// case 5

	return 0L;
};

// Release synchronization objects.
// For Windows, call CloseHandle and DeleteCriticalSection.
void AudioPlayback::ReleaseSyncObjects(void)
{
	int result;

	switch (initedSyncObjectsCount)
	{
		case 5:
			result = CloseHandle(eventAudioStop);
#ifdef _DEBUG
			if (!result) DEBUG_OUTPUT(TEXT(
				"Error on AudioPlayback::ReleaseSyncObjects_CloseHandle_eventAudioStop"));
#endif
			eventAudioStop = nullptr;
			DecreaseInitedSyncObjectsCount();

		case 4:
			result = CloseHandle(eventAudioChangeFinished);
#ifdef _DEBUG
			if (!result) DEBUG_OUTPUT(TEXT(
				"Error on AudioPlayback::ReleaseSyncObjects_CloseHandle_eventAudioChangeFinished"));
#endif
			eventAudioChangeFinished = nullptr;
			DecreaseInitedSyncObjectsCount();

		case 3:
			result = CloseHandle(eventAudioChanged);
#ifdef _DEBUG
			if (!result) DEBUG_OUTPUT(TEXT(
				"Error on AudioPlayback::ReleaseSyncObjects_CloseHandle_eventAudioChanged"));
#endif
			eventAudioChanged = nullptr;
			DecreaseInitedSyncObjectsCount();

		case 2:
			result = CloseHandle(eventAudioReady);
#ifdef _DEBUG
			if (!result) DEBUG_OUTPUT(TEXT(
				"Error on AudioPlayback::ReleaseSyncObjects_CloseHandle_eventAudioReady"));
#endif
			eventAudioReady = nullptr;
			DecreaseInitedSyncObjectsCount();

		case 1:
			DeleteCriticalSection(&mutexHandleMap);
			DecreaseInitedSyncObjectsCount();
		default:
			break;
	};
};

HRESULT AudioPlayback::InitializePlayback(void)
{
	HRESULT result;
	if (ptrDevice == nullptr)	return E_POINTER;

	// 1. IMMDevice::Activate, IAudioClient::GetMixFormat
	result = ptrDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, reinterpret_cast<void**>(&ptrClient));
	if (result != S_OK)	return result;
	IncreaseInitedComponentsCount();	// case 1

	result = ptrClient->GetMixFormat(&ptrFormat);
	if (result != S_OK) return result;
	IncreaseInitedComponentsCount();	// case 2

	// MSDN description :
	// For a shared-mode stream, if the client passes IAudioClient::GetDevicePeriod method
	// an (I think that editor misspelled 'and'.) hnsBufferDuration parameter value of 0,
	// the method assumes that the periods for the client and audio engine are guaranteed to be equal,
	// and the method will allocate a buffer small enough to achieve the minimum possible latency.
	// ref.) https://msdn.microsoft.com/en-us/library/windows/desktop/dd370871(v=vs.85).aspx
	//result = ptrClient->GetDevicePeriod(&rtDevDefaultPeriod, &rtDevMinimumPeriod);

	// 2. IAudioClient::Initialize, IAudioClient::GetBufferSize,
	//    IAudioClient::SetEventHandle (in event-driven streaming)
	result = ptrClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, 0, 0, ptrFormat, NULL);
	if (result != S_OK)	return result;

	result = ptrClient->GetBufferSize(&bufFrameNumbers);
	if (result != S_OK)	return result;

	result = ptrClient->SetEventHandle(eventAudioReady);
	if (result != S_OK) return result;

	// 3. IAudioClient::GetService(IID_IAudioRenderClient, ...)
	result = ptrClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&ptrRenderClient));
	if (result != S_OK) return result;
	IncreaseInitedComponentsCount();	// case 3

	// Opt. IAudioClient::GetService(IID_IAudioSessionControl, ...) &
	//		IAudioSessionControl::RegisterAudioSessionNotification(this)
	result = ptrClient->GetService(__uuidof(IAudioSessionControl), reinterpret_cast<void**>(&ptrSessionControl));
	if (result != S_OK) return result;
	IncreaseInitedComponentsCount();	// case 4

	result = ptrSessionControl->RegisterAudioSessionNotification(this);
	if (result == S_OK)
		IncreaseInitedComponentsCount();// case 5
	return result;
};

HRESULT AudioPlayback::TerminatePlayback(void)
{
	long result;

	switch (initedComponentsCount)
	{
		case 5:
			result = ptrSessionControl->UnregisterAudioSessionNotification(this);
			DecreaseInitedComponentsCount();
		case 4:
			ptrSessionControl->Release();
			ptrSessionControl = nullptr;
			DecreaseInitedComponentsCount();
		case 3:
			ptrRenderClient->Release();
			ptrRenderClient = nullptr;
			DecreaseInitedComponentsCount();
		case 2:
			// There is description on MSDN for method GetMixFormat ; 
			// The method allocates the storage for the structure.
			// The caller is responsible for freeing the storage,
			// when it is no longer needed, by calling the CoTaskMemFree function.
			// ref.) https://msdn.microsoft.com/en-us/library/windows/desktop/dd370872(v=vs.85).aspx
			CoTaskMemFree(ptrFormat);
			ptrFormat = nullptr;
			DecreaseInitedComponentsCount();
		case 1:
			ptrClient->Release();
			ptrClient = nullptr;
			DecreaseInitedComponentsCount();
		default:
			break;
	};

	return result;
};

HRESULT	AudioPlayback::StartPlayback(void)
{
	playbackBufferSize = (ptrFormat->nBlockAlign) * bufFrameNumbers;

	// Create the playback thread.
	// Before creating the thread, check there are events for ready and stop.
	if (!handlePlaybackThread && eventAudioReady && eventAudioStop)
	{
		handlePlaybackThread = CreateThread(NULL, 0, this->PlaybackThread, this, 0, NULL);
		if (handlePlaybackThread == nullptr) return HRESULT_FROM_WIN32(GetLastError());
	}
	/*else return 400	/* ERROR_THREAD_MODE_ALREADY_BACKGROUND */ ;

	return ptrClient->Start();
};

DWORD WINAPI AudioPlayback::PlaybackThread(_In_ LPVOID lpParameter)
{
	// lpParameter must point initialized AudioCapture instance.
	AudioPlayback *player = static_cast<AudioPlayback *>(lpParameter);
	return player->PerformRendering();
};

DWORD AudioPlayback::PerformRendering(void)
{
	HRESULT hr;
	HANDLE eventArray[3] = { eventAudioStop, eventAudioChanged, eventAudioReady };
	HANDLE mmcssHandle = NULL;
	DWORD waitResult;
	DWORD mmcssTaskIndex = 0UL;

	unsigned int failedCount = 0U;
	unsigned int succeedCount = 0U;
	bool	isActive = true; //	condition variable for while loop

#ifdef _DEBUG
	TMCContainer tmc_while, tmc_signal;
#endif

	hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hr))	return GetLastError();

#ifdef _MMCSS
	if (!DisableMMCSS)
	{
		mmcssHandle = AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);
	}
#endif

	// Send empty data to get event from endpoint device.
	// If a packet size is 0, we don't need to call ReleaseBuffer.
	ptrRenderClient->GetBuffer(0U, 0);	// C6387

	while (isActive)
	{
		// Performance measuring part
#ifdef _DEBUG
		TimeMeasureStart(&tmc_while);
#endif

		waitResult = WaitForMultipleObjects(3, eventArray, FALSE, INFINITE);
		switch (waitResult)
		{
			case WAIT_OBJECT_0:     // eventAudioStop
				isActive = false;
				break;

			case WAIT_OBJECT_0 + 1:	// eventAudioChanged
									// Wait changing audio device is finished,
									// and re-gather audio related objects.
				if (ReplaceComponents()) isActive = false;
				break;

			case WAIT_OBJECT_0 + 2:	// eventAudioReady
									// We need to provide the next buffer of samples to the audio renderer.
				unsigned char *dstBuffer;
				unsigned int padding;
				unsigned int framesAvailable;

				//  We need to find out how much of the buffer *isn't* available (is padding).
				hr = ptrClient->GetCurrentPadding(&padding);
				if (SUCCEEDED(hr))
				{
					//  Calculate the number of frames available.  We'll render
					//  that many frames or the number of frames left in the buffer, whichever is smaller.
					framesAvailable = bufFrameNumbers - padding;
					if (framesAvailable >= SAMPLING_RATE)
					{
						AudioDatagram *ptrDatagram;
						// Get audio source, and destination buffer address
						if (ptrQueue->try_pop(ptrDatagram) && 
							SUCCEEDED(hr = ptrRenderClient->GetBuffer(bufFrameNumbers, &dstBuffer)))
						{
							// Copy data to the output buffer and bump our render pointer.
							// We need only audio data, not a information of sender, type, ...
							CopyMemory(dstBuffer, ptrDatagram->dataArray, ptrDatagram->dataLength);
							hr = ptrRenderClient->ReleaseBuffer(bufFrameNumbers, 0);

							// Copy all data to the buffer for additional use
							// (e.g. AudioDataHandler::GetPlaybackAudioDatagram())
							CopyMemory(&playbackDatagram, ptrDatagram, sizeof(AudioDatagram));
							delete ptrDatagram;
						};

						// Error handling in failed HRESULT
						if (FAILED(hr)) failedCount += 1U;

						// Handling(signal) previously registered event objects.
						// 1. Setting an event that is already set has no effect.
						// 2. An event object whose state remains signaled until a single waiting thread is released,
						// at which time the system automatically sets the state to nonsignaled.
						if (numEventHandle) //if (!mapEventHandle.empty())
						{
#ifdef _DEBUG
							TimeMeasureGetFreq(&tmc_signal);
							TimeMeasureStart(&tmc_signal);
#endif

							EnterCriticalSection(&mutexHandleMap);
							for (EventHandleMap::const_iterator it
								= mapEventHandle.cbegin(); it != mapEventHandle.cend(); ++it)
								SetEvent(it->second);
							LeaveCriticalSection(&mutexHandleMap);

#ifdef _DEBUG
							TimeMeasureEnd(&tmc_signal, "AudioPlayback::PerformRendering_signal");
#endif
						};
					};
				}
				break;

		// End of switch-case
		};

#ifdef _DEBUG
		TimeMeasureEnd(&tmc_while, "AudioCapture::PerformRendering_while");
#endif

		// If there are too many errors, break the loop for stability.
		// Allowable aspect ratio is set to 1F:100S.
		if (failedCount >= 10U) isActive = false;
		else if (failedCount > 0U)
		{
			if (succeedCount < 100U) succeedCount += 1U;
			else
			{
				failedCount -= 1U;
				succeedCount = 0U;
			};
		};
	}
	// Loop breaks.

#ifdef _MMCSS
	if (!DisableMMCSS)
	{
		AvRevertMmThreadCharacteristics(mmcssHandle);
	}
#endif
	CoUninitialize();

	return 0;
};

//
long AudioPlayback::ReplaceComponents(void)
{
	DWORD waitResult;
	long result;

	// Stop the device first.
	result = ptrClient->Stop();
	if (result) return result;

	waitResult = WaitForSingleObject(eventAudioChangeFinished, 200);
	if (waitResult)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT(
			"Failed at AudioPlayback::PerformRendering_WaitForSingleObject(eventAudioChangedFinished)."));
#endif
		return (waitResult == 0xFFFFFFFF /* WAIT_FAILED */
			? HRESULT_FROM_WIN32(GetLastError()) : HRESULT_FROM_WIN32(waitResult));
	}
	else
	{
		// Before re-gathering audio objects release previous things first.
		// In this moment previous device is released by AudioDeviceHandler already,
		// So we don't need to care about the device.

		/// You should compare (maybe)current format and the previous
		/// so there is no problem with handling prefered format.
		/* but now here is blank... what should i put? */
		TerminatePlayback();

		result = InitializePlayback();
		if (result)
		{
#ifdef _DEBUG
			DEBUG_OUTPUT(TEXT(
				"Failed at AudioPlayback::PerformRendering_InitializePlayback"));
#endif
			return result;
		};
		result = StartPlayback();
		if (result)
		{
#ifdef _DEBUG
			DEBUG_OUTPUT(TEXT(
				"Failed at AudioPlayback::PerformRendering_StartPlayback"));
#endif
			return result;
		};

		return 0L;
	};
};


// Stop playback.
// Raises audio stopped event to end the thread.
HRESULT AudioPlayback::StopPlayback(void)
{
	// Stop playback thread and confirm thread is stopped.
	if (eventAudioStop)
	{
		SetEvent(eventAudioStop);
		WaitForSingleObject(handlePlaybackThread, INFINITE);
	}
	else /// case ; eventAudioStop is null so can't be raised up
	{
		// 0 means ERROR_SUCCESS (in HRESULT same as 0)
		TerminateThread(handlePlaybackThread, 0);	// C6258
	};
	return ptrClient->Stop();
};

HANDLE AudioPlayback::CreateNewEventHandle(void)
{
	// There is no function which verify handle as an event object.
	// So making new event handle in this function looks safer than
	// getting unspecified handle from somewhere out of this library.
	// Just don't call this function too much for acceptable performance.
	// ref.) https://groups.google.com/d/msg/microsoft.public.vc.language/2WMZTKHVsv0/2XSPjEFrojkJ
	HANDLE newEventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

#ifdef EVENT_HANDLE_CONTAINER_USE_STL
	EnterCriticalSection(&mutexHandleMap);
#endif

	if (newEventHandle != nullptr)
	{
		if (mapEventHandle.insert({ newEventHandle, newEventHandle }).second)
			numEventHandle += 1U;
#ifdef _DEBUG
		else
			DEBUG_OUTPUT(TEXT("Error on AudioPlayback::CreateNewEventHandle_insert"));
#endif
	};
	// else check the error code using GetLastError() in Windows.

#ifdef EVENT_HANDLE_CONTAINER_USE_STL
	LeaveCriticalSection(&mutexHandleMap);
#endif

	return newEventHandle;
};

bool AudioPlayback::RemoveEventHandle(_In_ HANDLE handle)
{
	bool result;

	EnterCriticalSection(&mutexHandleMap);
#ifdef EVENT_HANDLE_CONTAINER_USE_STL
	result = mapEventHandle.erase(handle);
#else
	result = mapEventHandle.unsafe_erase(handle);
#endif
	LeaveCriticalSection(&mutexHandleMap);

	if (result)
	{
		numEventHandle -= 1U;

#ifdef _DEBUG
		// If an error occurred, CloseHandle returns 0.
		if (!CloseHandle(handle)) 
			DEBUG_OUTPUT(TEXT("Error on AudioPlayback::RemoveEventHandle_CloseHandle"));
#else
		CloseHandle(handle);
#endif
	};

	return result;
};


// inline ; Increases variable initedSyncObjectsCount
inline void	AudioPlayback::IncreaseInitedSyncObjectsCount(void)
{
	initedSyncObjectsCount += 1U;
};
// inline ; Decreases variable initedSyncObjectsCount
inline void	AudioPlayback::DecreaseInitedSyncObjectsCount(void)
{
	initedSyncObjectsCount -= 1U;
};
// inline ; Increases variable initedComponentsCount
inline void	AudioPlayback::IncreaseInitedComponentsCount(void)
{
	initedComponentsCount += 1U;
};
// inline ; Decreases variable initedComponentsCount
inline void	AudioPlayback::DecreaseInitedComponentsCount(void)
{
	initedComponentsCount -= 1U;
};



// IUnknown class methods ; Increases reference count
ULONG	STDMETHODCALLTYPE AudioPlayback::AddRef(void)
{
	return InterlockedIncrement(&refCount);
};
// IUnknown class methods ; Decreases reference count
ULONG	STDMETHODCALLTYPE AudioPlayback::Release(void)
{
	ULONG ulRef = InterlockedDecrement(&refCount);
	// In normal zero-referenced COM objects should be deleted.
	// But as we're definitely going to release this class later,
	// we don't need to (and must not) call destructor in this time.
	//// (ulRef == 0) { delete this; }
	return ulRef;
};
// IUnknown class methods ; QueryInterface
// This method basically can writes out address of this class to IAudioSessionEvents pointer.
HRESULT STDMETHODCALLTYPE AudioPlayback::QueryInterface(REFIID riid, void **ppvObject)
{
	if (ppvObject == nullptr) { return E_POINTER; }

	*ppvObject = nullptr;
	if (riid == IID_IUnknown)
	{
		*ppvObject = static_cast<IUnknown *>(static_cast<IAudioSessionEvents *>(this));
		AddRef();
	}

	else if (riid == __uuidof(IAudioSessionEvents))
	{
		*ppvObject = static_cast<IAudioSessionEvents *>(this);
		AddRef();
	}

	else { return E_NOINTERFACE; }

	return S_OK;
};

/// Definition below is for interitancing IAudioSessionEvents class.

// This function is called when playback audio session is disconnected.
HRESULT STDMETHODCALLTYPE AudioPlayback::OnSessionDisconnected(
	_In_ AudioSessionDisconnectReason reason)
{
	HRESULT result = 0;

	switch (reason)
	{
		case (AudioSessionDisconnectReason::DisconnectReasonDeviceRemoval):
			// OnSessionDisconnected signal comes from system
			// faster than OnDefaultDeviceChanged signal.
			// So we should wait, then AudioDeviceHandler may change device.
			result = SetEvent(eventAudioChanged);
			break;
		case (AudioSessionDisconnectReason::DisconnectReasonFormatChanged):
			// In this case we just need to re-take client object.
			// Raise changing event to get WAIT_OBJECT_0 in WaitForSingleObject,
			// and finished event to intend entering re-take audio components routine.
			result = SetEvent(eventAudioChanged);
			result = (SetEvent(eventAudioChangeFinished) && result);	// In MSVC left operand is evaluated always.
			break;
		case (AudioSessionDisconnectReason::DisconnectReasonExclusiveModeOverride):
		case (AudioSessionDisconnectReason::DisconnectReasonServerShutdown):
		case (AudioSessionDisconnectReason::DisconnectReasonSessionDisconnected):
		case (AudioSessionDisconnectReason::DisconnectReasonSessionLogoff):
			break;
	};

	return (result ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
};