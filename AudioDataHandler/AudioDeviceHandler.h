#pragma once

// for auto-recovery linking when default device connection is broken
#include "AudioPlayback.h"
#include "AudioCapture.h"	

#ifdef __cplusplus

// This class implements IMMNotificationClient::OnDefaultDeviceChanged method
// due to imminent accessing after default audio device is changed.
class AudioDeviceHandler : public IMMNotificationClient
{
public:
	explicit AudioDeviceHandler
		(AudioPlayback **RESTRICT playback, AudioCapture **RESTRICT capture);
	~AudioDeviceHandler(void);
	HRESULT Initialize(void);

	// Getter of default playback device
	_Ret_maybenull_ IMMDevice* GetDefaultPlaybackDevice(void);
	// Getter of default capture device
	_Ret_maybenull_ IMMDevice* GetDefaultCaptureDevice(void);

	// IUnknown class method -- increase reference count
	ULONG	STDMETHODCALLTYPE AddRef(void) override;
	// IUnknown class method -- decrease reference count
	ULONG	STDMETHODCALLTYPE Release(void) override;
	// IUnknown class method -- QueryInterface
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;

private:
	AudioPlayback		**ppPlayback;
	AudioCapture		**ppCapture;

	IMMDeviceEnumerator *ptrEnumerator;
	IMMDevice			*ptrRenderDevice;
	IMMDevice			*ptrCaptureDevice;

	// IUnknown variable
	ULONG				refCount;


	/// Definitions below are for interitancing IMMNotificationClient class.

	// Called when the default capture device changed.
	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
		_In_ EDataFlow flow, _In_ ERole role, _In_ LPCWSTR ptrDeviceId) override;

	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
		_In_ LPCWSTR /*pwstrDeviceId*/, _In_ DWORD /*dwNewState*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnDeviceAdded(_In_ LPCWSTR /*pwstrDeviceId*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(_In_ LPCWSTR /*pwstrDeviceId*/) override { return E_NOTIMPL; };
	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
		_In_ LPCWSTR /*pwstrDeviceId*/, _In_ const PROPERTYKEY /*key*/)	override { return E_NOTIMPL; };
};
#else
typedef struct AudioDeviceHandler {

	explicit (* AudioDeviceHandler)	(AudioDeviceHandler * This,
		AudioPlayback **RESTRICT playback, AudioCapture **RESTRICT capture);
	(* ~AudioDeviceHandler) (AudioDeviceHandler * This);
	HRESULT (*Initialize) (AudioDeviceHandler * This);

	// Getter of default playback device
	_Ret_maybenull_ IMMDevice* (*GetDefaultPlaybackDevice) (AudioDeviceHandler * This);
	// Getter of default capture device
	_Ret_maybenull_ IMMDevice* (*GetDefaultCaptureDevice) (AudioDeviceHandler * This);

	// IUnknown class method -- increase reference count 
	ULONG	(STDMETHODCALLTYPE *AddRef) (AudioDeviceHandler * This) override;
	// IUnknown class method -- decrease reference count
	ULONG	(STDMETHODCALLTYPE *Release) (AudioDeviceHandler * This) override;
	// IUnknown class method -- QueryInterface
	HRESULT (STDMETHODCALLTYPE *QueryInterface) (
		AudioDeviceHandler * This, REFIID riid, void **ppvObject) override;

	AudioPlayback		**ppPlayback;
	AudioCapture		**ppCapture;

	IMMDeviceEnumerator *ptrEnumerator;
	IMMDevice			*ptrRenderDevice;
	IMMDevice			*ptrCaptureDevice;

	// IUnknown variable
	ULONG				refCount;


	/// Definitions below are for interitancing IMMNotificationClient class.

	// Called when the default capture device changed.
	HRESULT (STDMETHODCALLTYPE *OnDefaultDeviceChanged) (AudioDeviceHandler * This,
		_In_ EDataFlow flow, _In_ ERole role, _In_ LPCWSTR ptrDeviceId) override;

	HRESULT (STDMETHODCALLTYPE *OnDeviceStateChanged) (AudioDeviceHandler * This,
		_In_ LPCWSTR /*pwstrDeviceId*/, _In_ DWORD /*dwNewState*/) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnDeviceAdded) (
		AudioDeviceHandler * This, _In_ LPCWSTR /*pwstrDeviceId*/) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnDeviceRemoved) (
		AudioDeviceHandler * This, _In_ LPCWSTR /*pwstrDeviceId*/) override { return E_NOTIMPL; };
	HRESULT (STDMETHODCALLTYPE *OnPropertyValueChanged) (AudioDeviceHandler * This,
		_In_ LPCWSTR /*pwstrDeviceId*/, _In_ const PROPERTYKEY /*key*/)	override { return E_NOTIMPL; };

} AudioDeviceHandler;
#endif