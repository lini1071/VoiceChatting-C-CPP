// AudioDataServer.cpp : DLL 응용 프로그램을 위해 내보낸 함수를 정의합니다.
//

#include "stdafx.h"
#include "AudioDataServer.h"

// for compatibility with Linux
#ifndef INVALID_SOCKET
#define INVALID_SOCKET ((SOCKET) ~0)
#endif

// Optional entry for Windows dynamic library
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
};

// 내보낸 클래스의 생성자입니다.
// 클래스 정의를 보려면 AudioDataServer.h를 참조하십시오.
AudioDataServer::AudioDataServer(void) :
	isActive(false), multiPort(0)
{
#ifdef _WIN32
	availableRIO = IsWindows8OrGreater();
#endif

    return;
};

AudioDataServer::~AudioDataServer(void)
{
	Terminate();
};

// If no error occurred, it returns 0.
int	AudioDataServer::Initialize(void)
{
	WSADATA wsaData;
	int result;

	// Call WSAStartup
	result = WSAStartup(0x0202UI16, &wsaData);
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		result = WSAVERNOTSUPPORTED;
	}
	return result;
};


// If no error occurred on WSACleanup in Windows, it returns 0.
int	AudioDataServer::Terminate(void)
{
	int result;

	if (isActive)
	{
		isActive = false;

		// actually same as WaitForMultipleObjects of Windows
		// (although in Windows below is less efficient than it)

#ifdef __cplusplus
		for (RecvThreadList::const_iterator it =
			threadList.cbegin() ; it != threadList.cend() ; ++it)
		{
#ifdef _DEBUG
			if (THREAD_JOIN(it->first))
				DEBUG_OUTPUT(TEXT("Error occurred on AudioDataServer::Terminate_THREADJOIN"));
#else
			THREAD_JOIN(it->first);
#endif
		};
#else

#endif

		// Release mutex(lock) object initialized on OpenSocket-blah.
		DESTROY_LOCK(&lockObject);
	};

#ifdef _WIN32
	result = WSACleanup();
	return (result ? SOCKET_ERROR_CODE : 0);
#else
	return SOCKET_ERROR_CODE;
#endif
};

#ifdef _WIN32
// Windows

// Establishing Registered I/O socket connection on Windows
// This uses RIOReceiveEx in receiving, and WSASend in sending.
int	AudioDataServer::OpenSocketRIO(void)
{
	int result;
	DWORD output;
	ERRNO_TYPE getResult;
	unsigned long numOfLogicalProcessors;
	THREAD recvThread;

	socklen_t sa_size = sizeof(struct sockaddr);
	struct sockaddr_in serverAddr = { AF_INET, htons(DEFAULT_PORT),{ INADDR_ANY } };

	SendContextContainer*	ptrSendContext;
	RecvContextContainer*	ptrRecvContext;
	ListenContextContainer*	ptrListenContext;

	GUID guid_rio = WSAID_MULTIPLE_RIO, guid_acceptEx = WSAID_ACCEPTEX;
	LPFN_ACCEPTEX ptrFuncAcceptEx = 0;
	void* ptrContext = 0;

	serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_REGISTERED_IO);
	if (WSAIoctl(serverSocket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
			&guid_rio, sizeof(guid_rio), &rioFuncTable, sizeof(rioFuncTable), &output, 0, 0) &&

		WSAIoctl(serverSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_acceptEx,
			sizeof(guid_acceptEx), &ptrFuncAcceptEx, sizeof(ptrFuncAcceptEx), &output, 0, 0))
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::OpenSocketRIO_WSAIoctl"));
#endif
		return SOCKET_ERROR_CODE;
	};

	// Create I/O completion port
	multiPort = MULTIPORT_CREATE(0);
	if (!multiPort)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::OpenSocketRIO_CreateIoCompletionPort"));
#endif
		return ((int) SYSTEM_ERROR_CODE);
	};

	// Maybe you can initialize with designated initializer list "IF" MSVS supports C99...
	/// compType = { RIO_IOCP_COMPLETION, .Iocp = { handleIOCP, 0, &overlapped } };
	OVERLAPPED overlapped;
	compType.Type = RIO_IOCP_COMPLETION;
	compType.Iocp = { multiPort, 0, &overlapped };

	// Create Registered I/O completion queue first.
	if ((compQueue = rioFuncTable.RIOCreateCompletionQueue(32UL, &compType)) == RIO_INVALID_CQ)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::OpenSocketRIO_RIOCreateCompletionQueue"));
#endif
		MULTIPORT_CLOSE(multiPort);
		return SOCKET_ERROR_CODE;
	};

	// Create Registered I/O request queue after.
	if ((reqQueue = rioFuncTable.RIOCreateRequestQueue(
		serverSocket, 32UL, 1UL, 16UL, 1UL, compQueue, compQueue, &ptrContext)) == RIO_INVALID_RQ)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::OpenSocketRIO_RIOCreateRequestQueue"));
#endif
		MULTIPORT_CLOSE(multiPort);
		return SOCKET_ERROR_CODE;
	};

	// Register the buffer to make the system locking buffer.
	rioIdData = rioFuncTable.RIORegisterBuffer((char *) &buffer, sizeof(buffer));
	if (rioIdData == RIO_INVALID_BUFFERID)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::OpenSocketRIO_RIORegisterBuffer"));
#endif
		MULTIPORT_CLOSE(multiPort);
		return SOCKET_ERROR_CODE;
	};

	// Bind the socket to all addresses(IPADDR_ANY).
	result = bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR)
	{
		MULTIPORT_CLOSE(multiPort);
		closesocket(serverSocket);
		return SOCKET_ERROR_CODE;
	};

	// Make server side socket listening incoming connection.
	result = listen(serverSocket, SOMAXCONN);
	if (result == SOCKET_ERROR)
	{
		MULTIPORT_CLOSE(multiPort);
		closesocket(serverSocket);
		return SOCKET_ERROR_CODE;
	};

	/// Setting part ; RIO_BUF
	output = 0;
	for (size_t i = 0 ; i < sizeof(rioDataBuffer) / sizeof(*rioDataBuffer) ; i++)
	{
		rioDataBuffer[i].BufferId	= rioIdData;
		rioDataBuffer[i].Offset		= output;

		rioDataBuffer[i].Length = L1_CACHE_SIZE;
		output += L1_CACHE_SIZE;
	};
	for (size_t i = 0 ; i < sizeof(rioAddrBuffer) / sizeof(*rioAddrBuffer) ; i++)
	{
		rioAddrBuffer[i].BufferId	= rioIdData;
		rioAddrBuffer[i].Offset		= output;

		rioAddrBuffer[i].Length = sizeof(SOCKADDR_INET);
		output += sizeof(SOCKADDR_INET);
	};

	THREAD(sendThread, AudioDataServer::SendThreadOIO, ptrSendContext);

	for (size_t i = 0; i < numOfLogicalProcessors; i++)
	{
		// Create a receiving thread and push to the thread list
		THREAD(recvThread, AudioDataServer::RecvThreadRIO, ptrRecvContext);
		threadList.insert({ recvThread, recvThread });
	};

	THREAD(listenThread, AudioDataServer::ListenThreadIOCP, ptrListenContext);

	return 0;
};

// Establishing Overlapped I/O socket connection on Windows
int	AudioDataServer::OpenSocketOIO(void)
{
	int result;
	ERRNO_TYPE getResult;
	unsigned long numOfLogicalProcessors;
	THREAD recvThread;

	socklen_t sa_size = sizeof(struct sockaddr);
	struct sockaddr_in serverAddr = { AF_INET, htons(DEFAULT_PORT), { INADDR_ANY } };

	SendContextContainer*	ptrSendContext;
	RecvContextContainer*	ptrRecvContext;
	ListenContextContainer*	ptrListenContext;

	// Create a socket.
	serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (serverSocket == INVALID_SOCKET) return -1;

	// Bind the socket to all addresses(IPADDR_ANY).
	result = bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR)
	{
		closesocket(serverSocket);
		return WSAGetLastError();
	};

	result = listen(serverSocket, SOMAXCONN);

	// Create I/O completion port
	multiPort = MULTIPORT_CREATE(0) /* CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0) */;
	if (!multiPort)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::OpenSocketRIO_CreateIoCompletionPort"));
#endif
		return ((int) SYSTEM_ERROR_CODE);
	};
	
	// Prepare sending & listening thread.
	isActive = true;

	// Initialize lock(mutex) object
	INIT_LOCK(&lockObject);

#ifdef __cplusplus
	try
	{
		getResult = GetLogicalProcessorsCount(&numOfLogicalProcessors);
		if (getResult) throw std::system_error(getResult, std::system_category());

		// Prepare socket threads
		ptrSendContext =
			new SendContextContainer{ &isActive, &connections, &dataQueue };
		ptrRecvContext =
			new RecvContextContainer{ &isActive, &lockObject, &connections, &threadList, &dataQueue, 0, { }, multiPort };
		ptrListenContext =
			new ListenContextContainer{ &isActive, &lockObject, &connections, &threadList, &dataQueue, serverSocket };

		THREAD(sendThread, AudioDataServer::SendThreadOIO, ptrSendContext);

		for (size_t i = 0 ; i < numOfLogicalProcessors ; i++)
		{
			// Create a receiving thread and push to the thread list
			THREAD(recvThread, AudioDataServer::RecvThreadRIO, ptrRecvContext);
			threadList.insert({ recvThread, recvThread });
		};

		THREAD(listenThread, AudioDataServer::ListenThreadIOCP, ptrListenContext);
	}
	catch (const std::bad_alloc&)
	{
		result = ERROR_OUTOFMEMORY;
	}
	catch (const std::system_error& e)
	{
		result = e.code().value();
	};
#else
	getResult = GetLogicalProcessorsCount(&numOfLogicalProcessors);
	if (getResult) 

	ptrSendContext = (SendContextContainer *)malloc(sizeof(SendContextContainer));
	ptrListenContext = (ListenContextContainer *)malloc(sizeof(ListenContextContainer));

	if (!ptrSendContext || !ptrListenContext) result = ERROR_OUTOFMEMORY;
	else
	{
#if defined(_WIN32)
		sendThread = CreateThread(0, 0, SendThread, ptrSendContext, 0, 0);
		listenThread = CreateThread(0, 0, ListenThread, ptrListenContext, 0, 0);
#elif defined(__linux__)
		result = (pthread_create(&sendThread, 0, SendThread, ptrSendContext) &&
			pthread_create(&listenThread, 0, ListenThread, ptrListenContext));
#endif
	};
#endif

	return result;
};

// Listening thread method on Windows Overlapped I/O socket (IOCP)
TF_TYPE CALL_CONV AudioDataServer::ListenThreadIOCP(_In_ void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE) ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		LockObject*				RESTRICT ptrLockObject;
		ConnectionContainer*	RESTRICT ptrConnections;
		RecvThreadList*			RESTRICT ptrThreadList;
		AudioDataQueue*			RESTRICT ptrAudioQueue;
		SOCKET					socket;
		MULTIPORT				multiPort;

		int sa_size = sizeof(struct sockaddr_in);

		ListenContextContainer* ptrContainer =
#ifdef __cplusplus
			static_cast<ListenContextContainer*>(lpParameter);
#else
			((ListenContextContainer *)lpParameter);
#endif

		ptrCondition	= ptrContainer->pBool;
		ptrLockObject	= ptrContainer->pLockObject;
		ptrConnections	= ptrContainer->pConnections;
		ptrThreadList	= ptrContainer->pThreadList;
		ptrAudioQueue	= ptrContainer->pAudioQueue;
		socket			= ptrContainer->socket;
		multiPort		= ptrContainer->port;

		if (!ptrCondition || !ptrLockObject || !ptrConnections ||
			!ptrThreadList || !ptrAudioQueue || !socket || !multiPort)
			return ((TF_TYPE) ERROR_INVALID_PARAMETER);

		// Prepare accepting part.
		else
		{
			SOCKET clientSocket;
			struct sockaddr_in clientAddr;
			union AudioDatagram::addr unionAddr;
			
			// ERROR_TYPE type temporary variable
			ERRNO_TYPE result = 0;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			free(lpParameter);
#endif

			while (*ptrCondition)
			{
				clientSocket = accept(socket, (struct sockaddr *) &clientAddr, &sa_size);
				if (clientSocket != INVALID_SOCKET)
				{
					LOCK(ptrLockObject);

					// Insert a binded connection struct to the container,
					// and associate the socket to multiplexing port.
					ptrConnections->insert({ clientSocket, { clientSocket, unionAddr } });
					MULTIPORT_ASSOCIATE(multiPort, clientSocket);

					UNLOCK(ptrLockObject);
				}
				else if (SOCKET_ERROR_CODE != ERROR_WOULDBLOCK) break;
			};

			return ((TF_TYPE) (result != 0 ? result : SOCKET_ERROR_CODE));
		};
	};
};

// Sending thread method on Windows Overlapped I/O socket
TF_TYPE CALL_CONV AudioDataServer::SendThreadOIO(_In_ void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE) ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		ConnectionContainer*	RESTRICT ptrConnections;
		AudioDataQueue*			RESTRICT ptrQueue;

		SendContextContainer*	RESTRICT ptrContainer;

		WSABUF*					RESTRICT ptrWsaBuf;
#ifdef __cplusplus
		// C++ smart pointer
		AudioDatagramPtr		ptrDatagram;

		ptrContainer = static_cast<SendContextContainer*>(lpParameter);
#else
		// C pointer
		AudioDatagramPtr		RESTRICT ptrDatagram;

		ptrContainer = ((SendContextContainer *)lpParameter);
#endif

		ptrCondition	= ptrContainer->pBool;
		ptrConnections	= ptrContainer->pConnections;
		ptrQueue		= ptrContainer->pAudioQueue;

		if (!ptrCondition || !ptrConnections || !ptrQueue)
			return ((TF_TYPE) ERROR_INVALID_PARAMETER);
		else
		{
			// 'int' type temporary variable
			int result = 0;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			free(lpParameter);
#endif
			do
			{
				// Is there any advantage of calling empty() first
				// to check emptiness rather than calling try_pop() always?
				// If then the code below is meaningful.
				// And if not, it should be modified.
				if (!ptrQueue->empty() && ptrQueue->try_pop(ptrDatagram))
					for (ConnectionContainer::const_iterator it =
						ptrConnections->cbegin(); it != ptrConnections->cend(); ++it)
					{
						#ifdef __cplusplus
						#define PTR_DATAGRAM ptrDatagram.get()
						#else
						#define PTR_DATAGRAM ptrDatagram
						#endif

						#if defined(_M_X64) || defined(__amd64__)
						if (PTR_DATAGRAM->addr.full != it->second.second.full)
						#else
						if (PTR_DATAGRAM->addr.sep.ipv4.longType != it->second.second.sep.ipv4.longType &&
							PTR_DATAGRAM->addr.sep.port.longType != it->second.second.sep.port.longType)
						#endif

							result = WSASend(it->first,
								(const char *) PTR_DATAGRAM,
								sizeof(AudioDatagram::addr) +
								sizeof(AudioDatagram::dataLength) +
								sizeof(AudioDatagram::dataType) +
								(ptrDatagram->dataLength),
								0
							);
						#undef PTR_DATAGRAM
					};
			} while (*ptrCondition && result);

			return ((TF_TYPE) (result == SOCKET_ERROR ? SOCKET_ERROR_CODE : result));
		};
	};
};

// Receiving thread method on Windows Overlapped I/O socket (IOCP)
TF_TYPE CALL_CONV AudioDataServer::RecvThreadRIO(_In_ void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE) ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		LockObject*				RESTRICT ptrLockObject;
		ConnectionContainer*	RESTRICT ptrConnections;
		AudioDataQueue*			RESTRICT ptrAudioQueue;
		MULTIPORT				multiPort;

		int sa_size = sizeof(struct sockaddr_in);

		ListenContextContainer* ptrContainer =
#ifdef __cplusplus
			static_cast<ListenContextContainer*>(lpParameter);
#else
			((ListenContextContainer *)lpParameter);
#endif

		ptrCondition	= ptrContainer->pBool;
		ptrLockObject	= ptrContainer->pLockObject;
		ptrConnections	= ptrContainer->pConnections;
		ptrAudioQueue	= ptrContainer->pAudioQueue;
		multiPort		= ptrContainer->port;

		if (!ptrCondition || !ptrLockObject ||
			!ptrConnections || !ptrAudioQueue || !multiPort)
			return ((TF_TYPE) ERROR_INVALID_PARAMETER);

		// Prepare receiving part.
		// It uses GQCS(GetQueuedCompletionStatus) to dequeue completion packet.
		else
		{
			DWORD		numBytes;
			ULONG_PTR*	RESTRICT ptrCompletionKey;
			OVERLAPPED*	RESTRICT ptrOverlapped;

			// ERROR_TYPE type temporary variable
			ERRNO_TYPE result = 0;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			free(lpParameter);
#endif

			while (*ptrCondition)
			{
				if (GetQueuedCompletionStatus(multiPort, &numBytes, ptrCompletionKey, &ptrOverlapped, INFINITE))
				{
					// Regular transfer
					if (numBytes)
					{

					}

					// Graceful shutdown (recv == 0)
					else
					{
						LOCK(ptrLockObject);

						#ifdef __cplusplus
						#ifdef CONNECTION_CONTAINER_USE_STL
						ptrConnections->erase(*ptrCompletionKey);
						#else
						ptrConnections->unsafe_erase(*ptrCompletionKey);
						#endif
						#else
						ptrConnections->erase(*ptrCompletionKey);
						#endif
						MULTIPORT_DISSOCIATE(multiPort, *ptrCompletionKey);

						UNLOCK(ptrLockObject);
					};
				}
				else
				{
					if (SOCKET_ERROR_CODE != ERROR_WOULDBLOCK) break;
				};
			};

			return ((TF_TYPE) (result != 0 ? result : SOCKET_ERROR_CODE));
		};
	};
};
#else
// Linux

// Establishing epoll I/O socket connection on Linux
int AudioDataServer::OpenSocketEpoll(void)
{

};

TF_TYPE CALL_CONV AudioDataServer::ListenSocketEpoll(void* lpParameter)
{

};

TF_TYPE CALL_CONV AudioDataServer::RecvSocketEpoll(void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE)ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		LockObject*				RESTRICT ptrLockObject;
		ConnectionContainer*	RESTRICT ptrConnections;
		RecvThreadList*			RESTRICT ptrThreadList;
		AudioDataQueue*			RESTRICT ptrAudioQueue;
		SOCKET					socket;
		MULTIPORT				multiPort;

		int sa_size = sizeof(struct sockaddr_in);

		ListenContextContainer* ptrContainer =
#ifdef __cplusplus
			static_cast<ListenContextContainer*>(lpParameter);
#else
			((ListenContextContainer *)lpParameter);
#endif

		ptrCondition = ptrContainer->pBool;
		ptrLockObject = ptrContainer->pLockObject;
		ptrConnections = ptrContainer->pConnections;
		ptrThreadList = ptrContainer->pThreadList;
		ptrAudioQueue = ptrContainer->pAudioQueue;
		socket = ptrContainer->socket;
		multiPort = ptrContainer->port;

		if (!ptrCondition || !ptrLockObject || !ptrConnections ||
			!ptrThreadList || !ptrAudioQueue || !socket || !multiPort)
			return ((TF_TYPE)ERROR_INVALID_PARAMETER);

		// Prepare receiving part.
		// Traditional one socket - one thread concept.
		else
		{
			DWORD		 numBytes;
			PULONG_PTR	 RESTRICT ptrCompletionKey;
			LPOVERLAPPED RESTRICT ptrOverlapped;

			// ERROR_TYPE type temporary variable
			ERRNO_TYPE result = 0;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			free(lpParameter);
#endif

			while (*ptrCondition)
			{
				if (epoll_wait(multiPort, &numBytes, ptrCompletionKey, &ptrOverlapped, INFINITE))

				else if (SOCKET_ERROR_CODE != ERROR_WOULDBLOCK) break;
			};

			return ((TF_TYPE) (result != 0 ? result : SOCKET_ERROR_CODE));
		};
	};
};

TF_TYPE CALL_CONV AudioDataServer::SendSocketEpoll(void* lpParameter)
{

};

// Establishing asynchronous I/O socket connection on Linux
int	AudioDataServer::OpenSocketAsync(void)
{

};
#endif

// Establishing regular(blocking) socket connection
int AudioDataServer::OpenSocket(void)
{
	int result;
	socklen_t sa_size = sizeof(struct sockaddr);
	struct sockaddr_in serverAddr = { AF_INET, htons(DEFAULT_PORT), { INADDR_ANY } };

	SendContextContainer*	ptrSendContext;
	ListenContextContainer*	ptrListenContext;

	// Create a socket.
	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET) return -1;

	// Bind the socket to all addresses(IPADDR_ANY).
	result = bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR)
	{
		closesocket(serverSocket);
		return WSAGetLastError();
	};

	// Make server side socket listening incoming connection.
	result = listen(serverSocket, SOMAXCONN);
	if (result == SOCKET_ERROR)
	{
		closesocket(serverSocket);
		return WSAGetLastError();
	};

	// Prepare sending & listening thread.
	isActive = true;

	// Initialize lock(mutex) object
	INIT_LOCK(&lockObject);

#ifdef __cplusplus
	try
	{
		ptrSendContext =
			new SendContextContainer{ &isActive, &connections, &dataQueue };
		ptrListenContext =
			new ListenContextContainer{ &isActive, &lockObject, &connections, &threadList, &dataQueue, serverSocket };

		THREAD(sendThread, AudioDataServer::SendThread, ptrSendContext);
		THREAD(listenThread, AudioDataServer::ListenThread, ptrListenContext);
	}
	catch (const std::bad_alloc&)
	{
		result = ERROR_OUTOFMEMORY;


	}
	catch (const std::system_error& e)
	{
		result = e.code().value();
	};
#else
	ptrSendContext = (SendContextContainer *) malloc(sizeof(SendContextContainer));
	ptrListenContext = (ListenContextContainer *) malloc(sizeof(ListenContextContainer));

	if (!ptrSendContext || !ptrListenContext) result = ERROR_OUTOFMEMORY;
	else
	{
	#if defined(_WIN32)
		sendThread		= CreateThread(0, 0, SendThread, ptrSendContext, 0, 0);
		listenThread	= CreateThread(0, 0, ListenThread, ptrListenContext, 0, 0);
	#elif defined(__linux__)
		result = (pthread_create(&sendThread, 0, SendThread, ptrSendContext) &&
			pthread_create(&listenThread, 0, ListenThread, ptrListenContext));
	#endif
	};
#endif

	return result;
};

// Regular listening thread method.
TF_TYPE CALL_CONV AudioDataServer::ListenThread(_In_ void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE) ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		LockObject*				RESTRICT ptrLockObject;
		ConnectionContainer*	RESTRICT ptrConnections;
		RecvThreadList*			RESTRICT ptrThreadList;
		AudioDataQueue*			RESTRICT ptrAudioQueue;
		SOCKET					socket;

		int sa_size = sizeof(struct sockaddr_in);

		ListenContextContainer* ptrContainer =
		#ifdef __cplusplus
			static_cast<ListenContextContainer*>(lpParameter);
		#else
			((ListenContextContainer *) lpParameter);
		#endif
		ptrCondition	= ptrContainer->pBool;
		ptrLockObject	= ptrContainer->pLockObject;
		ptrConnections	= ptrContainer->pConnections;
		ptrThreadList	= ptrContainer->pThreadList;
		ptrAudioQueue	= ptrContainer->pAudioQueue;

		socket			= ptrContainer->socket;

		if (!ptrCondition || !ptrLockObject || !ptrConnections ||
			!ptrThreadList || !ptrAudioQueue || !socket)
			return ((TF_TYPE) ERROR_INVALID_PARAMETER);

		// Prepare receiving part.
		// Traditional one socket - one thread concept.
		else
		{
			// temporary variables
			ERRNO_TYPE result = 0;
			struct sockaddr_in clientAddr;
			union AudioDatagram::addr unionAddr;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			free(lpParameter);
#endif

			while (*ptrCondition)
			{
				SOCKET clientSocket = accept(socket, (struct sockaddr *) &clientAddr, &sa_size);
				if (clientSocket != INVALID_SOCKET)
				{
					#if defined(_WIN32)
					THREAD newRecvThread = CreateThread(0, 0, AudioDataServer::RecvThread,
						new RecvContextContainer{
						ptrCondition,
						ptrLockObject,
						ptrConnections,
						ptrThreadList,
						ptrAudioQueue,
						clientSocket,
						clientAddr
					}, 0, 0);

					if (!newRecvThread)
					{
						result = SYSTEM_ERROR_CODE /* maybe ERROR_OUTOFMEMORY */;
						break;
					}
					#elif defined(__linux__)
					if (result =
						pthread_create(&newRecvThread, 0, RecvThread, ptrRecvContext)) break;
					#endif

					// Register new remote connection data to the container,
					// and new receiving thread handle to the thread list.
					else
					{
						ConvSockAddrToInt(clientAddr, &unionAddr);

						LOCK(ptrLockObject);

						ptrConnections->insert({ clientSocket, { clientSocket, unionAddr } });
						ptrThreadList->insert({ newRecvThread, newRecvThread });

						UNLOCK(ptrLockObject);
					};
				}
				else break;
			};

			return ((TF_TYPE) (result != 0 ? result : SOCKET_ERROR_CODE));
		};
	};
};

// Send method (broadcasting - unique)
TF_TYPE CALL_CONV AudioDataServer::SendThread(_In_ void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE) ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		ConnectionContainer*	RESTRICT ptrConnections;
		AudioDataQueue*			RESTRICT ptrQueue;

		SendContextContainer*	RESTRICT ptrContainer;

#ifdef __cplusplus
		// C++ smart pointer
		AudioDatagramPtr		ptrDatagram;

		ptrContainer = static_cast<SendContextContainer*>(lpParameter);
#else
		// C pointer
		AudioDatagramPtr		RESTRICT ptrDatagram;

		ptrContainer = ((SendContextContainer *) lpParameter);
#endif

		ptrCondition	= ptrContainer->pBool;
		ptrConnections	= ptrContainer->pConnections;
		ptrQueue		= ptrContainer->pAudioQueue;

		if (!ptrCondition || !ptrConnections || !ptrQueue)
			return ((TF_TYPE) ERROR_INVALID_PARAMETER);
		else
		{
			// 'int' type temporary variable
			int result = 0;

			// temporary variable to save address of a client
			union AudioDatagram::addr addr;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			free(lpParameter);
#endif
			do
			{
				// Is there any advantage of calling empty() first
				// to check emptiness rather than calling try_pop() always?
				// If then the code below is meaningful.
				// And if not, it should be modified.
				if (!ptrQueue->empty() && ptrQueue->try_pop(ptrDatagram))
					for (ConnectionContainer::const_iterator it =
						ptrConnections->cbegin(); it != ptrConnections->cend(); ++it)
					{
						#ifdef __cplusplus
						#define PTR_DATAGRAM ptrDatagram.get()
						#else
						#define PTR_DATAGRAM ptrDatagram
						#endif

						#if defined(_M_X64) || defined(__amd64__)
						if (PTR_DATAGRAM->addr.full != it->second.second.full)
						#else
						if (PTR_DATAGRAM->addr.sep.ipv4.longType != it->second.second.sep.ipv4.longType &&
							PTR_DATAGRAM->addr.sep.port.longType != it->second.second.sep.port.longType)
						#endif
							result = send(
								it->first,
								(const char *)PTR_DATAGRAM,
								sizeof(AudioDatagram::addr) +
								sizeof(AudioDatagram::dataLength) +
								sizeof(AudioDatagram::dataType) +
								(ptrDatagram->dataLength),
								0
							);

						#undef PTR_DATAGRAM
					};
			} while (*ptrCondition && result);

			return ((TF_TYPE) (result == SOCKET_ERROR ? SOCKET_ERROR_CODE : result));
		};
	};
};

// Recv method (individual)
TF_TYPE CALL_CONV AudioDataServer::RecvThread(_In_ void* lpParameter)
{
	// Check parameter values, and get address contents of container.
	if (!lpParameter) return ((TF_TYPE) ERROR_INVALID_PARAMETER);
	else
	{
		bool*					RESTRICT ptrCondition;
		LockObject*				RESTRICT ptrLockObject;
		ConnectionContainer*	RESTRICT ptrConnections;
		RecvThreadList*			RESTRICT ptrThreadList;
		AudioDataQueue*			RESTRICT ptrQueue;

		SOCKET						clientSocket;
		struct	sockaddr_in			clientAddr;
		union	AudioDatagram::addr	srcAddr;

		AudioDatagram			dataBuffer;
		AudioDatagramPtr		ptrNewData;

		RecvContextContainer*	RESTRICT ptrContainer;

#ifdef __cplusplus
		ptrContainer = static_cast<RecvContextContainer *>(lpParameter);
#else
		ptrContainer = ((RecvContextContainer *)lpParameter);
#endif

		ptrCondition	= ptrContainer->pBool;
		ptrLockObject	= ptrContainer->pLockObject;
		ptrConnections	= ptrContainer->pConnections;
		ptrThreadList	= ptrContainer->pThreadList;
		ptrQueue		= ptrContainer->pAudioQueue;

		clientSocket	= ptrContainer->socket;
		clientAddr		= ptrContainer->addr;

		#if defined(_WIN32)
		#define IP_ADDR_INT_VAL(x) x.S_un.S_addr
		#elif defined(__linux__)
		#define IP_ADDR_INT_VAL(x) x.s_addr
		#endif

		if (!ptrCondition || !ptrLockObject || !ptrConnections ||
			!ptrThreadList || !ptrQueue || !clientSocket ||
			!IP_ADDR_INT_VAL(clientAddr.sin_addr) || !clientAddr.sin_port)
			return ((TF_TYPE) ERROR_INVALID_PARAMETER);

		#undef IP_ADDR_INT_VAL

		else
		{
			// ERROR_TYPE type temporary variable
			ERRNO_TYPE result = 0;

			// 'int' type temporary variable
			int sResult;

			// release context container object
#ifdef __cplusplus
			delete lpParameter;
#else
			FREE(lpParameter);
#endif

			// Assign obtained client address.
			ConvSockAddrToInt(clientAddr, &(dataBuffer.addr));

			while (sResult = recv(clientSocket, (char *) &dataBuffer, sizeof(AudioDatagram), 0))
			{
				#ifdef __cplusplus
				try
				{
					// (Implicit) copy constructor of AudioDatagram may be called.
					ptrNewData = AudioDatagramPtr(new AudioDatagram(dataBuffer));
				}
				catch (const std::bad_alloc&)
				{
					// Not a socket error
					result = SYSTEM_ERROR_CODE /* maybe ERROR_OUTOFMEMORY */;
					break;
				};
				#else
				ptrNewData = (AudioDatagramPtr) malloc(sizeof(AudioDatagram));
				if (!ptrNewData) { result = SYSTEM_ERROR_CODE;  break; };

				memcpy(ptrNewData, &dataBuffer, sizeof(AudioDatagram));
				#endif

				// Assign previously obtained client address.
				//ptrNewData->addr = srcAddr;

				ptrQueue->push(ptrNewData);
			};

			// Recv cannot be executed(graceful shutdown, or error occurring),
			// so unregister this thread from the recv thread list and
			// remove socket which is associated with this thread from the container.
			if (*ptrCondition)
			{
				LOCK(ptrLockObject);

				#ifdef __cplusplus
					#ifdef RECV_THREAD_CONTAINER_USE_STL
				ptrThreadList->erase(CURRENT_THREAD_ID);
					#else
				ptrThreadList->unsafe_erase(CURRENT_THREAD_ID);
					#endif
				#else
				ptrThreadList->erase(CURRENT_THREAD_ID);
				#endif

				#ifdef __cplusplus
					#ifdef CONNECTION_CONTAINER_USE_STL
				ptrConnections->erase(CURRENT_THREAD_ID);
					#else
				ptrConnections->unsafe_erase(clientSocket);
					#endif
				#else
				ptrConnections->erase(CURRENT_THREAD_ID);
				#endif

				UNLOCK(ptrLockObject);
			};

			return ((TF_TYPE) (sResult == SOCKET_ERROR ? SOCKET_ERROR_CODE : result));
		};
	};
};

// inline function ; We assume that this function is called 'always' correctly, so it doesn't return any value.
inline void ConvSockAddrToInt(struct sockaddr_in sockaddr, union AudioDatagram::addr *target)
{
	#if defined(_WIN32)
	#define IP_ADDR_INT_VAL(x) x.S_un.S_addr
	#elif defined(__linux__)
	#define IP_ADDR_INT_VAL(x) x.s_addr
	#endif

	#if defined(_M_X64) || defined(__amd64__)
		// 64-bit
		target->full =
			htonl((ntohl(((uint64_t)IP_ADDR_INT_VAL(sockaddr.sin_addr)) << 32)) + ntohs(sockaddr.sin_port));
	#else
		target->sep.ipv4.longType = IP_ADDR_INT_VAL(sockaddr.sin_addr);
		target->sep.port.longType = sockaddr.sin_port;
	#endif

	#undef IP_ADDR_INT_VAL
};

// Function collecting total number of logical processors on Windows system
// If there is no error, it returns 0.
ERRNO_TYPE GetLogicalProcessorsCount(unsigned long *pNum)
{
	ERRNO_TYPE code;

	DWORD size = 0L, jumped = 0L, counts = 0L;
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *ptrInfo, *ptrCurInfo;

	// Get total processor count
	GetLogicalProcessorInformationEx(RelationGroup, 0, &size);
	code = SYSTEM_ERROR_CODE;
	if (code != ERROR_INSUFFICIENT_BUFFER) return code;

	// Must it be LocalAlloc?
#ifdef __cplusplus
	ptrInfo = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(LocalAlloc(LMEM_FIXED, size));
#else
	ptrInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) (LocalAlloc(LMEM_FIXED, size));
#endif
	if (!ptrInfo) return SYSTEM_ERROR_CODE;
	if (!GetLogicalProcessorInformationEx(RelationGroup, ptrInfo, &size))
	{
		LocalFree(ptrInfo);
		return SYSTEM_ERROR_CODE;
	};

	// Counts active processors only
	for (ptrCurInfo = ptrInfo ; jumped < size ;
		jumped += (ptrInfo->Size), ptrCurInfo += (ptrInfo->Size))
	{
		for (WORD i = 0U; i < ptrCurInfo->Group.ActiveGroupCount; i++)
			counts += (ptrCurInfo->Group.GroupInfo[i].ActiveProcessorCount);
	};
	LocalFree(ptrInfo);

	*pNum = counts;
	return ((ERRNO_TYPE) 0);
};
