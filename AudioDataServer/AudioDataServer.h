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

#if defined(_WIN32)
	// Windows

	#include <VersionHelpers.h>

	#include <WinSock2.h>
	#pragma comment(lib, "ws2_32.lib")

	#include <WS2tcpip.h>

	#include <MSWSock.h>
	#pragma comment(lib, "Mswsock.lib")

	// Define type of error code be returned
	// DWORD can be replaced with decltype(GetLastError()) in C++.
	///typedef decltype(GetLastError()) ERRNO_TYPE;
	typedef DWORD ERRNO_TYPE;
	// Macro of system error code
	// In Windows, GetLastError() replaces this macro.
	#define SYSTEM_ERROR_CODE	GetLastError()

	// Macro of socket-range error code
	// In Windows, WSAGetLastError() replaces this macro.
	#define SOCKET_ERROR_CODE	WSAGetLastError()

	// Macro of 'would block' socket error code
	// In Windows, WSAEWOULDBLOCK replaces this macro.
	#define ERROR_WOULDBLOCK	WSAEWOULDBLOCK

	// Define type of multiplexing port identifier
	typedef HANDLE RESTRICT MULTIPORT;
	// Macro of function creating multiplexing port identifier
	// In Windows, CreateIoCompletionPort() with INVALID_HANDLE_VALUE replaces this macro.
	#define MULTIPORT_CREATE(numOfConcurrentThreads) \
		CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, numOfConcurrentThreads)
	// Macro of function associating socket with port identifier
	// In Windows, CreateIoCompletionPort() replaces this macro.
	#define MULTIPORT_ASSOCIATE(port, socket) \
		CreateIoCompletionPort((MULTIPORT) socket, port, socket, 0)
	// Macro of function dissociating socket with port identifier
	// In Windows, closesocket() replaces this macro to close socket only
	// (and then the socket is removed from the port 'automatically').
	#define MULTIPORT_DISSOCIATE(port, socket) closesocket(socket)
	// Macro of function releasing multiplexing port identifier
	// In Windows, CloseHandle() replaces this macro.
	// But we must assure that all socket handles are
	// "explicitly" closed before using this macro.
	#define MULTIPORT_CLOSE(port) CloseHandle(port)

	// Define type of thread identifier
	typedef HANDLE THREAD;
	// Define type of thread function(ThreadProc in Windows)
	typedef DWORD TF_TYPE;
	// Macro of calling convention
	// In Windows, thread function should be 'stdcall'
	#define CALL_CONV WINAPI

	// Macro of function creating thread
	// In Windows, CreateThread() replaces this macro.
	#define THREAD(handle,func,param) \
		handle = CreateThread(0, 0, func, param, 0, 0)
	// Macro of function returning identifier of current thread
	// In Windows, GetCurrentThread() replaces this macro.
	#define CURRENT_THREAD_ID	GetCurrentThread()
	// Macro of function waiting exit of thread
	// In Windows, WaitForSingleObject() replaces this macro.
	#define THREAD_JOIN(x)		WaitForSingleObject(x, INFINITE)

	// Define type of process-local mutex object
	typedef CRITICAL_SECTION LockObject;
	// Macro of function initializing mutex object used
	// In Windows, InitializeCriticalSection() replaces this macro.
	#define INIT_LOCK(x)	InitializeCriticalSection(x)
	// Macro of function locking mutex object
	// In Windows, EnterCriticalSection() replaces this macro.
	#define LOCK(x)			EnterCriticalSection(x)
	// Macro of function unlocking mutex object
	// In Windows, LeaveCriticalSection() replaces this macro.
	#define UNLOCK(x)		LeaveCriticalSection(x)
	// Macro of function releasing mutex object used
	// In Windows, DeleteCriticalSection() replaces this macro.
	#define DESTROY_LOCK(x)	DeleteCriticalSection(x)

#elif defined(__linux__)
	// Linux

	#include <sys/types.h>
	#include <sys/socket.h>
	#include <pthread.h>

	// Define type of error code be returned
	typedef int ERRNO_TYPE;
	// Macro of system error code
	// In Linux, errno indicates general error value.
	#define SYSTEM_ERROR_CODE		errno

	// Macro of error code indicating there are no enough memory
	#define ERROR_OUTOFMEMORY		ENOMEM
	// Macro of error code indicating inappropriate argument
	#define ERROR_INVALID_PARAMETER	EINVAL

	// Define type of socket as signed integer(file descriptor)
	typedef int SOCKET;
	// Macro of socket error code
	// In Linux, socket error code don't need to be different than others.
	#define SOCKET_ERROR_CODE	errno
	// Macro of 'would block' socket error code
	// In Linux, EWOULDBLOCK replaces this macro.
	#define ERROR_WOULDBLOCK	EWOULDBLOCK

	// Define type of multiplexing port identifier
	typedef int MULTIPORT;
	// Macro of function creating multiplexing port identifier
	// In Linux, epoll_create1() replaces this macro.
	// epoll_create() is not recommended because
	// since Linux 2.6.8 the size argument is ignored
	// (but the argument must be greater than zero).
	#define MULTIPORT_CREATE(flags) epoll_create1(flags)
	// Macro of function associating identifier with socket
	// In Linux, !epoll_ctl() replaces this macro
	// (when epoll_ctl is succeed, it returns 0).
	#define MULTIPORT_ASSOCIATE(port, socket, data) \
		!(epoll_ctl(port, EPOLL_CTL_ADD, socket, data))
	// Macro of function dissociating socket with port identifier
	// In Linux, !epoll_ctl() and close replace this macro
	// (event argument is ignored with EPOLL_CTL_DEL opcode).
	#define MULTIPORT_DISSOCIATE(port, socket) \
		!epoll_ctl(port, EPOLL_CTL_DEL, socket, 0) && close(socket)
	// Macro of function releasing multiplexing port identifier
	// In Linux, this macro replaces no function so it is meaningless.
	#define MULTIPORT_CLOSE(port) 

	// Define type of thread identifier
	typedef pthread_t THREAD;
	// Define type of thread function(ThreadProc in Windows)
	typedef void* TF_TYPE;
	// Macro of calling convention
	// In Linux, thread function don't need to be different than others.
	#define CALL_CONV

	// Macro of function creating thread
	// In Linux, pthread_create() replaces this macro.
	#define THREAD(handle,func,param) \
		pthread_create(&handle, 0, func, param)
	// Macro of function returning identifier of current thread
	// In Linux, pthread_self() replaces this macro.
	#define CURRENT_THREAD_ID	pthread_self()
	// Macro of function waiting exit of thread
	// In Linux, pthread_join() replaces this macro.
	#define THREAD_JOIN(x)		pthread_join(x, 0)

	// Define type of process-local mutex object
	typedef pthread_mutex_t	LockObject;
	// Macro of function initializing mutex object used
	// In Linux, pthread_mutex_init() replaces this macro.
	#define INIT_LOCK(x)	pthread_mutex_init(x, 0)
	// Macro of function locking mutex object
	// In Linux, pthread_mutex_lock() replaces this macro.
	#define LOCK(x)			pthread_mutex_lock(x)
	// Macro of function unlocking mutex object
	// In Linux, pthread_mutex_unlock() replaces this macro.
	#define UNLOCK(x)		pthread_mutex_unlock(x)
	// Macro of function releasing mutex object used
	// In Linux, pthread_mutex_destroy() replaces this macro.
	#define DESTROY_LOCK(x)	pthread_mutex_destroy(x)

#endif


#ifdef __cplusplus

	#ifdef _WIN32
		//template class __declspec(dllexport) AUDIO_DATA_CONTAINER;
		typedef AUDIO_DATA_CONTAINER	AudioDataQueue;

		#include <system_error>	// std::system_error	; thread related
		
		#ifdef RECV_THREAD_CONTAINER_USE_STL
		#include <vector>
		#define RECV_THREAD_CONTAINER std::vector<THREAD>
		#else

		// MSVC++ specific, for thread-safety
		// Visual Studio provides some concurrent containers.
		// e.g.) concurrent_vector, concurrent_queue, concurrent_unordered_map, etc.
		// And this contents can be only used in C++, not C.
		#include <concurrent_unordered_map.h>
		#define RECV_THREAD_CONTAINER concurrency::concurrent_unordered_map<THREAD, THREAD>
		template class __declspec(dllexport) RECV_THREAD_CONTAINER;
		typedef RECV_THREAD_CONTAINER		 RecvThreadList;
		#endif


		#include <utility>	// std::pair

		typedef std::pair<SOCKET, union AudioDatagram::addr> ClientConnection;

		#ifdef CONNECTION_CONTAINER_USE_STL
		#include <vector>
		#define CONNECTION_CONTAINER std::vector<std::pair<SOCKET, union AudioDatagram::addr>>
		#else
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

class AUDIODATASERVER_API AudioDataServer {
public:
	explicit AudioDataServer(void);
	~AudioDataServer(void);

	int			Initialize(void);
	int			Terminate(void);

#ifdef _WIN32
	// Windows
	int			OpenSocketRIO(void);
	int			OpenSocketOIO(void);

	static TF_TYPE CALL_CONV ListenThreadIOCP(_In_ void* lpParameter);
	static TF_TYPE CALL_CONV RecvThreadRIO(_In_ void* lpParameter);
	static TF_TYPE CALL_CONV RecvThreadOIO(_In_ void* lpParameter);
	static TF_TYPE CALL_CONV SendThreadOIO(_In_ void* lpParameter);
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

	ConnectionContainer			connections;
	static TF_TYPE CALL_CONV	ListenThread(_In_ void* lpParameter);
	static TF_TYPE CALL_CONV	SendThread(_In_ void* lpParameter);
	static TF_TYPE CALL_CONV	RecvThread(_In_ void* lpParameter);

	// sending thread handle
	THREAD				sendThread;

	// listening thread handle
	THREAD				listenThread;

	// Input(output is also supported) multiplexing port
	MULTIPORT			multiPort;

	// Receive thread list
	RecvThreadList		threadList;

	// Lock object used in erasing terminated thread
	LockObject			lockObject;
	
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

	struct buffer {
		// actual data buffer
		char			dataBuffer[BUFFER_SIZE];
		// actual remote address buffer
		SOCKADDR_INET	addrBuffer[BUFFER_SIZE / L1_CACHE_SIZE];
	} buffer;
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

	bool		isActive;

	ConnectionContainer			connections;
	static TF_TYPE CALL_CONV	(*ListenThread) (AudioDataServer * This, _In_ void* lpParameter);
	static TF_TYPE CALL_CONV	(*SendThread) (AudioDataServer * This, _In_ void* lpParameter);
	static TF_TYPE CALL_CONV	(*RecvThread) (AudioDataServer * This, _In_ void* lpParameter);

	// sending thread handle
	THREAD			sendThread;

	// listening thread handle
	THREAD			listenThread;

	// Receive thread list
	RecvThreadList	threadList;

	// Lock object used in erasing terminated thread
	LockObject		listLock;

	// Store pointers of audio data.
	AudioDataQueue	dataQueue;

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

// This structure is used in listen thread method.
typedef struct ListenContextContainer {
	bool*					RESTRICT pBool;
	LockObject*				RESTRICT pLockObject;
	ConnectionContainer*	RESTRICT pConnections;
	RecvThreadList*			RESTRICT pThreadList;
	AudioDataQueue*			RESTRICT pAudioQueue;

	SOCKET					socket;
	MULTIPORT				port;
} ListenContextContainer;

// This structure is used in recv thread method.
typedef struct RecvContextContainer {
	bool*					RESTRICT pBool;
	LockObject*				RESTRICT pLockObject;
	ConnectionContainer*	RESTRICT pConnections;
	RecvThreadList*			RESTRICT pThreadList;
	AudioDataQueue*			RESTRICT pAudioQueue;

	SOCKET					socket;
	struct sockaddr_in		addr;
	MULTIPORT				port; // this doesn't mean TCP/UDP port.
} RecvContextContainer;

// This structure is used in send thread method.
typedef struct SendContextContainer {
	bool*					RESTRICT pBool;
	ConnectionContainer*	RESTRICT pConnections;
	AudioDataQueue*			RESTRICT pAudioQueue;
} SendContextContainer;

// Define task context container for overlapped I/O completion routine
typedef struct SendOIOContextContainer
{
	WSAOVERLAPPED	overlapObj;
	WSABUF			wsabuf;

	AudioDatagram	*ptrBuffer;
	AudioDataQueue	*ptrQueue;

} SendOIOContextContainer;
