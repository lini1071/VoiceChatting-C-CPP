#pragma once
#include <Audioclient.h>
#include <AudioPolicy.h>	// IAudioSessionEvents

#ifdef __cplusplus
	#ifdef _WIN32
		template class __declspec(dllexport) AUDIO_DATA_CONTAINER;
		template class __declspec(dllexport) EVENT_HANDLE_CONTAINER;
		typedef AUDIO_DATA_CONTAINER	AudioDataQueue;
		typedef EVENT_HANDLE_CONTAINER	EventHandleMap;
	#else
		// GCC doesn't provide any concurrent containers
		// which is sufficient for using in producer-consumer problem,
		// so we must implement it ourselves. Maybe next time...
	#endif

// This class implements IAudioSessionEvents::OnSessionDisconnected method
// due to imminent accessing after session connection is broken.
class AudioPlayback : public IAudioSessionEvents
{
public:
	// This function returns new Windows event HANDLE
	// which is raised when audio data is captured.
	HANDLE	CreateNewEventHandle(void);
	// Remove and close Windows event HANDLE.
	bool	RemoveEventHandle(_In_ HANDLE handle);

	// Returns playback audio datagram structure.
	inline	AudioDatagram GetAudioDatagram(void) { return playbackDatagram; };

	/// constructor & destructor
	
	// Call this constructor after preparing received data queue.
	explicit AudioPlayback(AudioDataQueue* queue);
	// Calling constructor with empty arguments is forbidden.
	AudioPlayback(void) = delete;
	~AudioPlayback(void);

	// Copy assignment is forbidden.
	AudioPlayback& operator=(const AudioPlayback &)	= delete;
	// Move assignment is forbidden.
	AudioPlayback& operator=(const AudioPlayback &&) = delete;
	

	// Call this function to raise event when changing playback device is begun.
	// AudioDataHandler may use this function when default device is changed.
	int	SetEventAudioChanged(void);
	// Call this function to raise event when changing playback device is finished.
	// AudioDataHandler may use this function when default device is changed.
	int	SetEventAudioChangeFinished(void);

	// May return stored playback device pointer.
	_Ret_maybenull_	IMMDevice*	GetPlaybackDevice(void);
	// Set inner playback device pointer to argument.
	HRESULT		SetPlaybackDevice(_In_ IMMDevice* pDev);

	// Initialize synchronization objects.
	unsigned long InitializeSyncObjects(void);
	// Release synchronization objects.
	void ReleaseSyncObjects(void);

	// Initialize playback objects.
	long InitializePlayback(void);
	// Terminate playback objects.
	long TerminatePlayback(void);

	// Start playback(rendering).
	long StartPlayback(void);
	// Stop playback(rendering).
	long StopPlayback(void);

	// IUnknown class method ; Increases reference count
	ULONG	STDMETHODCALLTYPE AddRef(void) override;
	// IUnknown class method ; Decreases reference count
	ULONG	STDMETHODCALLTYPE Release(void) override;
	// IUnknown class method ; QueryInterface
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;

	/// Definitions below are for interitancing IAudioSessionEvents class.
	HRESULT STDMETHODCALLTYPE OnSessionDisconnected(
		_In_ AudioSessionDisconnectReason reason) override;

	HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(
		_In_ LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnIconPathChanged(
		_In_ LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(
		_In_ float /*NewSimpleVolume*/, _In_ BOOL /*NewMute*/, LPCGUID /*EventContext*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(
		_In_ DWORD /*ChannelCount*/, /*_In_reads_(ChannelCount)*/ float /*NewChannelVolumes*/[],
		_In_ DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(
		_In_ LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) override {	return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnStateChanged(
		_In_ AudioSessionState /*NewState*/) override {	return E_NOTIMPL; };

private:
	IMMDevice				*ptrDevice;
	WAVEFORMATEX			*ptrFormat;
	IAudioSessionControl	*ptrSessionControl;
	REFERENCE_TIME			bufCapacityDuration = 100000L;	// 10ms (before Windows 10)
	REFERENCE_TIME			rtDevDefaultPeriod;
	REFERENCE_TIME			rtDevMinimumPeriod;

	IAudioClient			*ptrClient;
	IAudioRenderClient		*ptrRenderClient;

	// inner pointer of audio data queue
	// This must point received audio data queue so we can hear others.
	AudioDataQueue			*ptrQueue;

	HANDLE					eventAudioReady;
	HANDLE					eventAudioStop;
	HANDLE					eventAudioChanged;
	HANDLE					eventAudioChangeFinished;
	UINT32					bufFrameNumbers;



	// number of event handles in event handle list
	// This is for running instruction more 'convinient' by just
	// comparing integer rather than calling data_queue::empty() function.
	unsigned int			numEventHandle;

	AudioDatagram			playbackDatagram;
	
	size_t					playbackBufferSize;
	size_t					playbackCurrentIndex;

	// playback thread handle
	HANDLE					handlePlaybackThread;
	// ThreadProc callback function
	static DWORD WINAPI		PlaybackThread(_In_ LPVOID lpParameter);
	// actual thread function
	DWORD					PerformRendering(void);
	// function handling default device switched event
	long					ReplaceComponents(void);

	// synchronization object in accessing event handle list
	CRITICAL_SECTION		mutexHandleMap;
	// event handle list which are raised on playback
	EventHandleMap			mapEventHandle;

	// IUnknown interface variable ; referenced count
	unsigned long			refCount;

	// Integer variable which counts initialized synchronization objects.
	unsigned int			initedSyncObjectsCount;

	// Integer variable which counts initialized audio component objects.
	unsigned int			initedComponentsCount;

	// Increases initialized synchronization objects count variable 
	inline void				IncreaseInitedSyncObjectsCount(void);
	// Decreases initialized synchronization objects count variable
	inline void				DecreaseInitedSyncObjectsCount(void);

	// Increases initialized audio components count variable
	inline void				IncreaseInitedComponentsCount(void);
	// Decreases initialized audio components count variable
	inline void				DecreaseInitedComponentsCount(void);
};

#else
typedef struct AudioPlayback {
	// This function returns new Windows event HANDLE
	// which is raised when audio data is captured.
	HANDLE	(*CreateNewEventHandle) (AudioPlayback * This);
	// Remove and close Windows event HANDLE.
	bool	(*RemoveEventHandle) (AudioPlayback * This, _In_ HANDLE handle);

	// Returns playback audio datagram structure.
	inline	AudioDatagram GetAudioDatagram(AudioPlayback * This) { return playbackDatagram; };

	/// constructor & destructor

	// Call this constructor after preparing received data queue.
	explicit AudioPlayback(AudioDataQueue* queue);
	// Calling constructor with empty arguments is forbidden.
	(* AudioPlayback) (AudioPlayback * This) = delete;
	(* ~AudioPlayback) (AudioPlayback * This);

	// Copy assignment is forbidden.
	AudioPlayback& operator=(AudioPlayback * This, const AudioPlayback &) = delete;
	// Move assignment is forbidden.
	AudioPlayback& operator=(AudioPlayback * This, const AudioPlayback &&) = delete;


	// Call this function to raise event when changing playback device is begun.
	// AudioDataHandler may use this function when default device is changed.
	int	(*SetEventAudioChanged) (AudioPlayback * This);
	// Call this function to raise event when changing playback device is finished.
	// AudioDataHandler may use this function when default device is changed.
	int	(*SetEventAudioChangeFinished) (AudioPlayback * This);

	// May return stored playback device pointer.
	_Ret_maybenull_	IMMDevice*	(*GetPlaybackDevice) (AudioPlayback * This);
	// Set inner playback device pointer to argument.
	HRESULT		(*SetPlaybackDevice) (AudioPlayback * This, _In_ IMMDevice* pDev);

	// Initialize synchronization objects.
	unsigned long (*InitializeSyncObjects) (AudioPlayback * This);
	// Release synchronization objects.
	void (*ReleaseSyncObjects) (AudioPlayback * This);

	// Initialize playback objects.
	long (*InitializePlayback) (AudioPlayback * This);
	// Terminate playback objects.
	long (*TerminatePlayback) (AudioPlayback * This);

	// Start playback(rendering).
	long (*StartPlayback) (AudioPlayback * This);
	// Stop playback(rendering).
	long (*StopPlayback) (AudioPlayback * This);

	// IUnknown class method ; Increases reference count
	ULONG	(STDMETHODCALLTYPE *AddRef) (AudioPlayback * This) override;
	// IUnknown class method ; Decreases reference count
	ULONG	(STDMETHODCALLTYPE *Release) (AudioPlayback * This) override;
	// IUnknown class method ; QueryInterface
	HRESULT (STDMETHODCALLTYPE *QueryInterface) (
		AudioPlayback * This, REFIID riid, void **ppvObject) override;

	/// Definitions below are for interitancing IAudioSessionEvents class.
	HRESULT (STDMETHODCALLTYPE *OnSessionDisconnected) (
		AudioPlayback * This,
		_In_ AudioSessionDisconnectReason reason
	) override;

	HRESULT (STDMETHODCALLTYPE *OnDisplayNameChanged) (
		AudioPlayback * This,
		_In_ LPCWSTR /*NewDisplayName*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnIconPathChanged) (
		AudioPlayback * This,
		_In_ LPCWSTR /*NewIconPath*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnSimpleVolumeChanged) (
		AudioPlayback * This,
		_In_ float /*NewSimpleVolume*/,
		_In_ BOOL /*NewMute*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnChannelVolumeChanged) (
		AudioPlayback * This,
		_In_ DWORD /*ChannelCount*/, /*_In_reads_(ChannelCount)*/ float /*NewChannelVolumes*/[],
		_In_ DWORD /*ChangedChannel*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnGroupingParamChanged) (
		AudioPlayback * This, 
		_In_ LPCGUID /*NewGroupingParam*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnStateChanged) (
		AudioPlayback * This,
		_In_ AudioSessionState /*NewState*/
	) override { return E_NOTIMPL; };

	IMMDevice				*ptrDevice;
	WAVEFORMATEX			*ptrFormat;
	IAudioSessionControl	*ptrSessionControl;
	REFERENCE_TIME			bufCapacityDuration = 100000L;	// 10ms (before Windows 10)
	REFERENCE_TIME			rtDevDefaultPeriod;
	REFERENCE_TIME			rtDevMinimumPeriod;

	IAudioClient			*ptrClient;
	IAudioRenderClient		*ptrRenderClient;

	// inner pointer of audio data queue
	// This must point received audio data queue so we can hear others.
	AudioDataQueue			*ptrQueue;

	HANDLE					eventAudioReady;
	HANDLE					eventAudioStop;
	HANDLE					eventAudioChanged;
	HANDLE					eventAudioChangeFinished;
	UINT32					bufFrameNumbers;



	// number of event handles in event handle list
	// This is for running instruction more 'convinient' by just
	// comparing integer rather than calling data_queue::empty() function.
	unsigned int			numEventHandle;

	AudioDatagram			playbackDatagram;

	size_t					playbackBufferSize;
	size_t					playbackCurrentIndex;

	// playback thread handle
	HANDLE					handlePlaybackThread;
	// ThreadProc callback function
	static DWORD WINAPI		(*PlaybackThread) (AudioPlayback * This, _In_ LPVOID lpParameter);
	// actual thread function
	DWORD					(*PerformRendering) (AudioPlayback * This);
	// function handling default device switched event
	long					(*ReplaceComponents) (AudioPlayback * This);

	// synchronization object in accessing event handle list
	CRITICAL_SECTION		mutexHandleMap;
	// event handle list which are raised on playback
	EventHandleMap			mapEventHandle;

	// IUnknown interface variable ; referenced count
	unsigned long			refCount;

	// Integer variable which counts initialized synchronization objects.
	unsigned int			initedSyncObjectsCount;

	// Integer variable which counts initialized audio component objects.
	unsigned int			initedComponentsCount;

	// Increases initialized synchronization objects count variable 
	inline void				(*IncreaseInitedSyncObjectsCount) (AudioPlayback * This);
	// Decreases initialized synchronization objects count variable
	inline void				(*DecreaseInitedSyncObjectsCount) (AudioPlayback * This);

	// Increases initialized audio components count variable
	inline void				(*IncreaseInitedComponentsCount) (AudioPlayback * This);
	// Decreases initialized audio components count variable
	inline void				(*DecreaseInitedComponentsCount) (AudioPlayback * This);
} AudioPlayback;
#endif