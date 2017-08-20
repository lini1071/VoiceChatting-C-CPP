#pragma once

// 다음 ifdef 블록은 DLL에서 내보내기하는 작업을 쉽게 해 주는 매크로를 만드는 
// 표준 방식입니다. 이 DLL에 들어 있는 파일은 모두 명령줄에 정의된 _EXPORTS 기호로
// 컴파일되며, 다른 프로젝트에서는 이 기호를 정의할 수 없습니다.
// 이렇게 하면 소스 파일에 이 파일이 들어 있는 다른 모든 프로젝트에서는 
// AUDIODATASERVER_API 함수를 DLL에서 가져오는 것으로 보고, 이 DLL은
// 이 DLL은 해당 매크로로 정의된 기호가 내보내지는 것으로 봅니다.
#if		defined	(_MSC_VER)
#define	ATTRIBUTE(x)	__declspec(x)
#elif	defined	(__GNUC__)
#define	ATTRIBUTE(x)	__attribute__ ((x))
#endif

#ifdef AUDIODATASERVER_EXPORTS
#define AUDIODATASERVER_API ATTRIBUTE(dllexport)
#else
#define AUDIODATASERVER_API ATTRIBUTE(dllimport)
#endif

#ifdef _WIN32
	// Windows

	#include <VersionHelpers.h>

	#include <WinSock2.h>
	#pragma comment(lib, "ws2_32.lib")

	#include <WS2tcpip.h>

	#include <MSWSock.h>
	#pragma comment(lib, "Mswsock.lib")

#else
	// Linux

	#include <sys/types.h>
	#include <sys/socket.h>

	typedef int SOCKET;

#endif


#ifdef __cplusplus

	#ifdef _WIN32
		template class __declspec(dllexport) AUDIO_DATA_CONTAINER;
		typedef AUDIO_DATA_CONTAINER	AudioDataQueue;

		#include <utility>	// std::pair, std::tuple

		typedef std::pair<SOCKET, struct sockaddr_in> ClientConnection;
		typedef std::pair<ClientConnection, AudioDataQueue *> ConnectionQueuePair;

		#ifdef CONNECTION_CONTAINER_USE_STL
		#include <vector>
		#define CONNECTION_CONTAINER std::vector<std::pair<SOCKET, struct sockaddr_in>>
		#else
				// MSVC++ specific, for thread-safety
				// Visual Studio provides some concurrent containers.
				// e.g.) concurrent_vector, concurrent_queue, concurrent_unordered_map, etc.
				// And this contents can be only used in C++, not C.
		#include <concurrent_unordered_map.h>
		#define CONNECTION_CONTAINER \
			concurrency::concurrent_unordered_map<SOCKET, ClientConnection>
		#endif
		template class __declspec(dllexport) CONNECTION_CONTAINER;
		typedef CONNECTION_CONTAINER		 ConnectionContainer;

	#else
		// GCC doesn't provide any concurrent containers
		// which is sufficient for using in producer-consumer problem,
		// so we must implement it ourselves. Maybe next time...
	#endif

#include <thread>	// thread ; Standard Library

typedef std::pair<bool* RESTRICT, ConnectionContainer* RESTRICT> CondContPair;

class AUDIODATASERVER_API AudioDataServer {
public:
	explicit AudioDataServer(void);
	~AudioDataServer(void);

	int			Initialize(void);
	int			Terminate(void);

	int			CreateSocket(int flags);
	int			BindSocket(void);
	
#ifdef _WIN32
	// Windows
	int			OpenSocketRIO(void);
	int			OpenSocketOIO(void);
#else
	// Linux
	int			OpenSocketEpoll(void);
	int			OpenSocketAsync(void);
#endif

	int			OpenSocket(void);

private:
#ifdef _WIN32
	// boolean value
	bool		availableRIO;
#endif

	bool		isActive;

	ConnectionContainer				connections;
	static unsigned long __stdcall	ListenThread(_In_ void* lpParameter);
	static unsigned long __stdcall	SendThread(_In_ void* lpParameter);
	static unsigned long __stdcall	RecvThread(_In_ void* lpParameter);

	// Store pointers of audio data.
	AudioDataQueue		dataQueue;

	// function table pointer for Registered I/O
	RIO_EXTENSION_FUNCTION_TABLE	rioFuncTable;

	// Registered I/O notifying specification object
	RIO_NOTIFICATION_COMPLETION		compType;

	// Registered I/O request queue
	RIO_RQ			reqQueue;
	// Registered I/O completion queue
	RIO_CQ			compQueue;

	SOCKET			serverSocket;
	HANDLE			handleSocketThread;

	// ID of 'actual' data buffer
	RIO_BUFFERID	rioIdActl;
	// ID of Registered I/O 'data' buffer
	RIO_BUFFERID	rioIdData;
	// ID of Registered I/O remote 'address' buffer
	RIO_BUFFERID	rioIdAddr;
	// ID of Registered I/O 'control' information buffer
	RIO_BUFFERID	rioIdCntl;

	// buffer of Registered I/O data
	RIO_BUF			rioDataBuffer[BUFFER_SIZE / L1_CACHE_SIZE];
	// buffer of Registered I/O remote address
	RIO_BUF			rioAddrBuffer[BUFFER_SIZE / L1_CACHE_SIZE];
	// buffer of Registered I/O control information
	RIO_BUF			rioCntlBuffer[BUFFER_SIZE / L1_CACHE_SIZE];

	// actual data buffer
	char			dataBuffer[BUFFER_SIZE];
};

#else

typedef struct {
	SOCKET				first;
	struct sockaddr_in	second;
} ClientConnection;

typedef struct {

} ConnectionContainer;

struct AUDIODATASERVER_API AudioDataServer {
	explicit (* AudioDataServer) (AudioDataServer * This);
	(* ~AudioDataServer) (AudioDataServer * This);

	int			(*Initialize) (AudioDataServer * This);
	int			(*Terminate) (AudioDataServer * This);

	int			(*CreateSocket) (AudioDataServer * This, int flags);
	int			(*BindSocket) (AudioDataServer * This);

#ifdef _WIN32	// Windows

	int			(*OpenSocketRIO) (AudioDataServer * This);
	int			(*OpenSocketOIO) (AudioDataServer * This);
	int			(*OpenSocket) (AudioDataServer * This);

#else			// Linux

	int			(*OpenSocketEpoll) (AudioDataServer * This);
	int			(*OpenSocketAsync) (AudioDataServer * This);
	int			(*OpenSocket) (AudioDataServer * This);

#endif

#ifdef _WIN32
	// boolean value
	BOOL		availableRIO;
#endif

	// function table pointer for Registered I/O
	RIO_EXTENSION_FUNCTION_TABLE	rioFuncTable;

	// Registered I/O notifying specification object
	RIO_NOTIFICATION_COMPLETION		compType;

	// Registered I/O request queue
	RIO_RQ			reqQueue;
	// Registered I/O completion queue
	RIO_CQ			compQueue;

	SOCKET			serverSocket;
	HANDLE			handleSocketThread;
	WSAEVENT		eventSocketConnected;

	// ID of 'actual' data buffer
	RIO_BUFFERID	rioIdActl;
	// ID of Registered I/O 'data' buffer
	RIO_BUFFERID	rioIdData;
	// ID of Registered I/O remote 'address' buffer
	RIO_BUFFERID	rioIdAddr;
	// ID of Registered I/O 'control' information buffer
	RIO_BUFFERID	rioIdCntl;

	// buffer of Registered I/O data
	RIO_BUF			rioDataBuffer[BUFFER_SIZE / L1_CACHE_SIZE];
	// buffer of Registered I/O remote address
	RIO_BUF			rioAddrBuffer[BUFFER_SIZE / L1_CACHE_SIZE];
	// buffer of Registered I/O control information
	RIO_BUF			rioCntlBuffer[BUFFER_SIZE / L1_CACHE_SIZE];

	// actual data buffer
	char			dataBuffer[BUFFER_SIZE];
};

#endif

// This structure is used in send thread method.
typedef struct {
	bool*					RESTRICT pBool;
	ConnectionContainer*	RESTRICT pCont;
	AudioDataQueue*			RESTRICT pQueue;
} CondContQueueTuple;
