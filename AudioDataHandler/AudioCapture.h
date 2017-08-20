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

class AudioCapture : public IAudioSessionEvents
{
public:
	// This function returns new Windows event HANDLE
	// which is raised when audio data is captured.
	HANDLE	CreateNewEventHandle(void);
	// Remove and close Windows event HANDLE.
	bool	RemoveEventHandle(_In_ HANDLE handle);

	// Returns playback audio datagram structure.
	inline	AudioDatagram GetAudioDatagram(void) { return captureDatagram; };

	// Call this constructor after preparing sending data queue.
	explicit AudioCapture(AudioDataQueue* queue);

	// Calling constructor with empty arguments is forbidden.
	AudioCapture(void) = delete;
	~AudioCapture(void);

	// Copy assignment is forbidden.
	AudioCapture& operator=(const AudioCapture &) = delete;
	// Move assignment is forbidden.
	AudioCapture& operator=(const AudioCapture &&) = delete;

	/// AudioDataHandler object uses these functions.

	// Call this function to raise event when changing capture device is begun.
	int	SetEventAudioChanged(void);
	// Call this function to raise event when changing capture device is finished.
	int	SetEventAudioChangeFinished(void);

	// May return stored capture device pointer.
	_Ret_maybenull_	IMMDevice*	GetCaptureDevice(void);
	// Set inner capture device pointer to argument.
	HRESULT	SetCaptureDevice(_In_ IMMDevice* pDev);

	// Initialize synchronization objects.
	unsigned long InitializeSyncObjects(void);
	// Release synchronization objects.
	void ReleaseSyncObjects(void);

	// Initialize capture objects.
	HRESULT	InitializeCapture(void);
	// Terminate capture objects.
	HRESULT	TerminateCapture(void);

	// Start capture.
	HRESULT	StartCapture(void);
	// Stop capture.
	HRESULT StopCapture(void);

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
		_In_ float /*NewSimpleVolume*/, _In_ BOOL /*NewMute*/, LPCGUID /*EventContext*/) override {	return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(
		_In_ DWORD /*ChannelCount*/, /*_In_reads_(ChannelCount)*/ float /*NewChannelVolumes*/[],
		_In_ DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) override {	return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(
		_In_ LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnStateChanged(
		_In_ AudioSessionState /*NewState*/) override { return E_NOTIMPL; };

private:
	IMMDevice			*ptrDevice;
	IAudioClient		*ptrClient;
	IAudioCaptureClient	*ptrCaptureClient;
	IAudioSessionControl*ptrSessionControl;

	WAVEFORMATEX		*ptrFormat;
	REFERENCE_TIME		bufCapacityDuration = 100000L;	// 10ms (before Windows 10)
	REFERENCE_TIME		rtDevDefaultPeriod;
	REFERENCE_TIME		rtDevMinimumPeriod;
	UINT32				bufFrameNumbers;

	HANDLE				eventAudioStop;
	HANDLE				eventAudioChangeFinished;
	HANDLE				eventAudioChanged;
	HANDLE				eventAudioReady;

	// number of event handles in event handle list
	// This is for running instruction more 'convinient' by just
	// comparing integer rather than calling data_queue::empty() function.
	unsigned int		numEventHandle;

	// Buffer-capturing components
	// In capturing, we should use 'relatively' stable memory space because
	// dynamic memory allocation is non-deterministic (thus relatively unstable)
	size_t				captureBufferSize;
	size_t				captureCurrentIndex;

	// capturing thread handle
	HANDLE				handleCaptureThread;
	// ThreadProc callback function
	static DWORD WINAPI	CaptureThread(LPVOID lpParameter);
	// actual thread function
	DWORD				PerformCapturing(void);
	// function handling default device switched event
	long				ReplaceComponents(void);

															//AudioDatagram		captureDatagram;

	// captured datagram as buffer
	AudioDatagram		captureDatagram;

	// Synchronization object in accessing event handle list
	CRITICAL_SECTION	mutexHandleMap;
	// Event handle list which are raised on playback
	EventHandleMap		mapEventHandle;

	// inner pointer of audio data queue
	// This must point audio data queue to be sent so we can say to others.
	AudioDataQueue		*ptrQueue;

	// IUnknown variable
	unsigned long		refCount;

	// Integer variable which counts initialized synchronization objects.
	unsigned int		initedSyncObjectsCount;

	// Integer variable which counts initialized audio component objects.
	unsigned int		initedComponentsCount;

	// Increases initialized synchronization objects count variable 
	inline void			IncreaseInitedSyncObjectsCount(void);
	// Decreases initialized synchronization objects count variable
	inline void			DecreaseInitedSyncObjectsCount(void);

	// Increases initialized audio components count variable
	inline void			IncreaseInitedComponentsCount(void);
	// Decreases initialized audio components count variable
	inline void			DecreaseInitedComponentsCount(void);
};

#else
typedef struct AudioCapture
{
	// This function returns new Windows event HANDLE
	// which is raised when audio data is captured.
	HANDLE	(*CreateNewEventHandle) (AudioCapture * This);
	// Remove and close Windows event HANDLE.
	bool	(*RemoveEventHandle) (AudioCapture * This, _In_ HANDLE handle);

	// Returns playback audio datagram structure.
	inline	AudioDatagram GetAudioDatagram(AudioCapture * This) { return captureDatagram; };

	// Call this constructor after preparing sending data queue.
	explicit (* AudioCapture) (AudioCapture * This, AudioDataQueue* queue);

	// Calling constructor with empty arguments is forbidden.
	(* AudioCapture) (AudioCapture * This) = delete;
	(* ~AudioCapture) (AudioCapture * This);

	// Copy assignment is forbidden.
	AudioCapture& operator=(AudioCapture * This, const AudioCapture &) = delete;
	// Move assignment is forbidden.
	AudioCapture& operator=(AudioCapture * This, const AudioCapture &&) = delete;

	/// AudioDataHandler object uses these functions.

	// Call this function to raise event when changing capture device is begun.
	int	(*SetEventAudioChanged) (AudioCapture * This);
	// Call this function to raise event when changing capture device is finished.
	int	(*SetEventAudioChangeFinished) (AudioCapture * This);

	// May return stored capture device pointer.
	_Ret_maybenull_	IMMDevice*	(*GetCaptureDevice) (AudioCapture * This);
	// Set inner capture device pointer to argument.
	HRESULT	(*SetCaptureDevice) (AudioCapture * This, _In_ IMMDevice* pDev);

	// Initialize synchronization objects.
	unsigned long (*InitializeSyncObjects) (AudioCapture * This);
	// Release synchronization objects.
	void (*ReleaseSyncObjects) (AudioCapture * This);

	// Initialize capture objects.
	HRESULT	(*InitializeCapture) (AudioCapture * This);
	// Terminate capture objects.
	HRESULT	(*TerminateCapture) (AudioCapture * This);

	// Start capture.
	HRESULT	(*StartCapture) (AudioCapture * This);
	// Stop capture.
	HRESULT (*StopCapture) (AudioCapture * This);

	// IUnknown class method ; Increases reference count
	ULONG	(STDMETHODCALLTYPE *AddRef) (AudioCapture * This) override;
	// IUnknown class method ; Decreases reference count
	ULONG	(STDMETHODCALLTYPE *Release) (AudioCapture * This) override;
	// IUnknown class method ; QueryInterface
	HRESULT (STDMETHODCALLTYPE *QueryInterface) (AudioCapture * This, REFIID riid, void **ppvObject) override;

	/// Definitions below are for interitancing IAudioSessionEvents class.
	HRESULT (STDMETHODCALLTYPE *OnSessionDisconnected) (
		AudioCapture * This,
		_In_ AudioSessionDisconnectReason reason
	) override;

	HRESULT (STDMETHODCALLTYPE *OnDisplayNameChanged) (
		AudioCapture * This,
		_In_ LPCWSTR /*NewDisplayName*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnIconPathChanged) (
		AudioCapture * This,
		_In_ LPCWSTR /*NewIconPath*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnSimpleVolumeChanged) (
		AudioCapture * This,
		_In_ float /*NewSimpleVolume*/,
		_In_ BOOL /*NewMute*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnChannelVolumeChanged) (
		AudioCapture * This,
		_In_ DWORD /*ChannelCount*/,
		/*_In_reads_(ChannelCount)*/ float /*NewChannelVolumes*/[],
		_In_ DWORD /*ChangedChannel*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnGroupingParamChanged) (
		AudioCapture * This,
		_In_ LPCGUID /*NewGroupingParam*/,
		LPCGUID /*EventContext*/
	) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnStateChanged) (
		AudioCapture * This,
		_In_ AudioSessionState /*NewState*/
	) override { return E_NOTIMPL; };

private:
	IMMDevice			*ptrDevice;
	IAudioClient		*ptrClient;
	IAudioCaptureClient	*ptrCaptureClient;
	IAudioSessionControl*ptrSessionControl;

	WAVEFORMATEX		*ptrFormat;
	REFERENCE_TIME		bufCapacityDuration = 100000L;	// 10ms (before Windows 10)
	REFERENCE_TIME		rtDevDefaultPeriod;
	REFERENCE_TIME		rtDevMinimumPeriod;
	UINT32				bufFrameNumbers;

	HANDLE				eventAudioStop;
	HANDLE				eventAudioChangeFinished;
	HANDLE				eventAudioChanged;
	HANDLE				eventAudioReady;

	// number of event handles in event handle list
	// This is for running instruction more 'convinient' by just
	// comparing integer rather than calling data_queue::empty() function.
	unsigned int		numEventHandle;

	// Buffer-capturing components
	// In capturing, we should use 'relatively' stable memory space because
	// dynamic memory allocation is non-deterministic (thus relatively unstable)
	size_t				captureBufferSize;
	size_t				captureCurrentIndex;

	// capturing thread handle
	HANDLE				handleCaptureThread;
	// ThreadProc callback function
	static DWORD WINAPI	(*CaptureThread) (AudioCapture * This, LPVOID lpParameter);
	// actual thread function
	DWORD				(*PerformCapturing) (AudioCapture * This);
	// function handling default device switched event
	long				(*ReplaceComponents) (AudioCapture * This);

	//AudioDatagram		captureDatagram;

	// captured datagram as buffer
	AudioDatagram		captureDatagram;

	// Synchronization object in accessing event handle list
	CRITICAL_SECTION	mutexHandleMap;
	// Event handle list which are raised on playback
	EventHandleMap		mapEventHandle;

	// inner pointer of audio data queue
	// This must point audio data queue to be sent so we can say to others.
	AudioDataQueue		*ptrQueue;

	// IUnknown variable
	unsigned long		refCount;

	// Integer variable which counts initialized synchronization objects.
	unsigned int		initedSyncObjectsCount;

	// Integer variable which counts initialized audio component objects.
	unsigned int		initedComponentsCount;

	// Increases initialized synchronization objects count variable 
	inline void			(*IncreaseInitedSyncObjectsCount) (AudioCapture * This);
	// Decreases initialized synchronization objects count variable
	inline void			(*DecreaseInitedSyncObjectsCount) (AudioCapture * This);

	// Increases initialized audio components count variable
	inline void			(*IncreaseInitedComponentsCount) (AudioCapture * This);
	// Decreases initialized audio components count variable
	inline void			(*DecreaseInitedComponentsCount) (AudioCapture * This);
} AudioCapture;
#endif