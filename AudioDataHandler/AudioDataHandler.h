#pragma once

// ���� ifdef ����� DLL���� ���������ϴ� �۾��� ���� �� �ִ� ��ũ�θ� ����� 
// ǥ�� ����Դϴ�. �� DLL�� ��� �ִ� ������ ��� ����ٿ� ���ǵ� _EXPORTS ��ȣ��
// �����ϵǸ�, �ٸ� ������Ʈ������ �� ��ȣ�� ������ �� �����ϴ�.
// �̷��� �ϸ� �ҽ� ���Ͽ� �� ������ ��� �ִ� �ٸ� ��� ������Ʈ������ 
// AUDIODATAHANDLER_API �Լ��� DLL���� �������� ������ ����, �� DLL��
// �� DLL�� �ش� ��ũ�η� ���ǵ� ��ȣ�� ���������� ������ ���ϴ�.
#if		defined	(_MSC_VER)
#define	ATTRIBUTE(x)	__declspec(x)
#elif	defined	(__GNUC__)
#define	ATTRIBUTE(x)	__attribute__ ((x))
#endif

#ifdef AUDIODATAHANDLER_EXPORTS
#define AUDIODATAHANDLER_API ATTRIBUTE(dllexport)
#else
#define AUDIODATAHANDLER_API ATTRIBUTE(dllimport)
#endif


#ifdef _WIN32
	// Windows

	// This class uses Winsock2 contents for sending audio data.
	#include <Winsock2.h>
	#pragma comment(lib, "Ws2_32.lib")
	#include <WS2tcpip.h>

#else
	// Linux

	// This class uses Berkeley socket contents for sending audio data.
	#include <sys/types.h>
	#include <sys/socket.h>

	typedef int SOCKET;

#endif

#include "AudioDeviceHandler.h"
#include "AudioCapture.h"
#include "AudioPlayback.h"

#define DEFAULT_TIMEOUT 10000


#ifdef __cplusplus
	#ifdef _WIN32
		template class __declspec(dllexport) AUDIO_DATA_CONTAINER;
		typedef AUDIO_DATA_CONTAINER	AudioDataQueue;
	#else
		// GCC doesn't provide any concurrent containers
		// which is sufficient for using in producer-consumer problem,
		// so we must implement it ourselves. Maybe next time...
	#endif

// Define task context container for overlapped I/O completion routine
typedef struct ContextContainer
{
	WSAOVERLAPPED	overlapObj;
	WSABUF			wsabuf;

	AudioDatagram	*ptrBuffer;
	AudioDataQueue	*ptrQueue;

} ContextContainer;

class AUDIODATAHANDLER_API AudioDataHandler
{
public:
	explicit AudioDataHandler(void);
	~AudioDataHandler(void);

	int				Initialize(void);
	int				Terminate(void);

	int				TryConnectW(_In_ PCWSTR string_addr);
	int				TryConnect(_In_ const char* string_addr);
	int				Disconnect(void);

	// for event-driven
	HANDLE			CreateCaptureEvent(void);
	bool			RemoveCaptureEvent(_In_ HANDLE handle);
	HANDLE			CreatePlaybackEvent(void);
	bool			RemovePlaybackEvent(_In_ HANDLE handle);

	// for optical appearance of GUI compnents
	AudioDatagram	GetCapturedAudioDatagram(void);
	AudioDatagram	GetPlaybackAudioDatagram(void);

private:
	AudioDeviceHandler	*ptrCollector;
	AudioCapture		*ptrCapture;
	AudioPlayback		*ptrPlayback;
	
	HANDLE				handleSocketThread;

	WSAEVENT			eventSocketConnected;
	SOCKET				priSocket;
	bool				isConnected;

	// Store pointers of audio data you shall send as the datagram to server, or other.
	AudioDataQueue		sendQueue;

	// Store pointers of datagrams you received from server, or other.
	AudioDataQueue		recvQueue;

	unsigned long		StartSocketThread(void);
	unsigned long		StopSocketThread(void);

	// ThreadProc callback function
	static unsigned
		long __stdcall	SocketThread(_In_ LPVOID lpParameter);	
	// actual thread function
	unsigned long		PerformSocketOps(void);					
	// inline routine
	inline int			CheckWSALastError(_Out_ int* result);

	// Overlapped I/O completion routine callback functions
	static void __stdcall	SendRoutine(
		_In_ DWORD error,
		_In_ DWORD cbTransferred,
		_In_ LPWSAOVERLAPPED lpOverlapped,
		_In_ DWORD /*flags*/
	);
	static void __stdcall	RecvRoutine(
		_In_ DWORD error,
		_In_ DWORD cbTransferred,
		_In_ LPWSAOVERLAPPED lpOverlapped,
		_In_ DWORD /*flags*/
	);
};

#else
typedef struct AudioDataHandler {

	explicit (* AudioDataHandler) (AudioDataHandler * This);
	(* ~AudioDataHandler) (AudioDataHandler * This);

	int				(*Initialize) (AudioDataHandler * This);
	int				(*Terminate) (AudioDataHandler * This);

	int				(*TryConnectW) (AudioDataHandler * This, _In_ PCWSTR string_addr);
	int				(*TryConnect) (AudioDataHandler * This, _In_ const char* string_addr);
	int				(*Disconnect) (AudioDataHandler * This);

	// for event-driven
	HANDLE			(*CreateCaptureEvent) (AudioDataHandler * This);
	bool			(*RemoveCaptureEvent) (AudioDataHandler * This, _In_ HANDLE handle);
	HANDLE			(*CreatePlaybackEvent) (AudioDataHandler * This);
	bool			(*RemovePlaybackEvent) (AudioDataHandler * This, _In_ HANDLE handle);

	// for optical appearance of GUI compnents
	AudioDatagram	(*GetCapturedAudioDatagram) (AudioDataHandler * This);
	AudioDatagram	(*GetPlaybackAudioDatagram) (AudioDataHandler * This);


	AudioDeviceHandler	*ptrCollector;
	AudioCapture		*ptrCapture;
	AudioPlayback		*ptrPlayback;
	
	HANDLE				handleSocketThread;

	WSAEVENT			eventSocketConnected;
	SOCKET				priSocket;
	bool				isConnected;

	// Store pointers of audio data you shall send as the datagram to server, or other.
	AudioDataQueue		sendQueue;

	// Store pointers of datagrams you received from server, or other.
	AudioDataQueue		recvQueue;

	unsigned long		(*StartSocketThread) (AudioDataHandler * This);
	unsigned long		(*StopSocketThread) (AudioDataHandler * This);

	// ThreadProc callback function
	static unsigned
		long __stdcall	(*SocketThread) (AudioDataHandler * This, _In_ LPVOID lpParameter);
	// actual thread function
	unsigned long		(*PerformSocketOps) (AudioDataHandler * This);
	// inline routine
	inline int			(*CheckWSALastError) (AudioDataHandler * This, _Out_ int* result);

	// Overlapped I/O completion routine callback functions
	static void __stdcall	(*SendRoutine) (
		AudioDataHandler * This,
		_In_ DWORD error,
		_In_ DWORD cbTransferred,
		_In_ LPWSAOVERLAPPED lpOverlapped,
		_In_ DWORD /*flags*/
		);
	static void __stdcall	(*RecvRoutine) (
		AudioDataHandler * This,
		_In_ DWORD error,
		_In_ DWORD cbTransferred,
		_In_ LPWSAOVERLAPPED lpOverlapped,
		_In_ DWORD /*flags*/
		);

} AudioDataHandler;
#endif