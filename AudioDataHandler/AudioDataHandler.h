#pragma once

// 다음 ifdef 블록은 DLL에서 내보내기하는 작업을 쉽게 해 주는 매크로를 만드는 
// 표준 방식입니다. 이 DLL에 들어 있는 파일은 모두 명령줄에 정의된 _EXPORTS 기호로
// 컴파일되며, 다른 프로젝트에서는 이 기호를 정의할 수 없습니다.
// 이렇게 하면 소스 파일에 이 파일이 들어 있는 다른 모든 프로젝트에서는 
// AUDIODATAHANDLER_API 함수를 DLL에서 가져오는 것으로 보고, 이 DLL은
// 이 DLL은 해당 매크로로 정의된 기호가 내보내지는 것으로 봅니다.
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