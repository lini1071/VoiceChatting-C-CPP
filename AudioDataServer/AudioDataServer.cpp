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
	isActive(true)
{
#ifdef _WIN32
	availableRIO = IsWindows8OrGreater();
#endif

    return;
};

AudioDataServer::~AudioDataServer(void)
{
	return;
};

// If no error occurred, it returns 0.
int	AudioDataServer::Initialize(void)
{
	WSADATA wsaData;
	int result;

	// First call WSAStartup
	if (result = WSAStartup(0x0202UI16, &wsaData)) return result;
	else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		return WSAVERNOTSUPPORTED;
	};
};


// If no error occurred, it returns 0.
int	AudioDataServer::Terminate(void)
{
	int result;

	result = WSACleanup();
	return (result ? WSAGetLastError() : 0);
};

// If no error occurred, it returns 0.
int AudioDataServer::CreateSocket(int flags)
{
#ifdef _WIN32	// Windows
	serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, flags);
#else			// Linux
	serverSocket = socket(AF_INET, flags | SOCK_STREAM, IPPROTO_TCP);
#endif
	
	if (serverSocket == INVALID_SOCKET) return -1;
	else return 0;
};

#ifdef _WIN32
int	AudioDataServer::OpenSocketRIO(void)
{

	return 0;
};
int	AudioDataServer::OpenSocketOIO(void)
{

};
#else
// Linux
int AudioDataServer::OpenSocketEpoll(void)
{

};
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

	// Create a socket.
	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET) return -1;
	
	// Bind the socket to all addresses(IPADDR_ANY).
	result = bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof (serverAddr));
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

	// Prepare sending thread.
#ifdef __cplusplus
	std::thread sendThread(AudioDataServer::SendThread, &(CondContPair(&isActive, &connections)));
	std::thread listenThread([&](SOCKET serverSocket, AudioDataQueue& dataQueue) {
#else

#endif

	// Prepare receiving part.
	// Traditional one socket - one thread concept.
	while (isActive)
	{
		struct sockaddr_in clientAddr;
		SOCKET clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &sa_size);
		if (clientSocket != INVALID_SOCKET)
		{
#ifdef __cplusplus
			std::thread newRecvThread(AudioDataServer::RecvThread,
				&(ConnectionQueuePair(ClientConnection(clientSocket, clientAddr), &dataQueue)));

#else
#ifdef _WIN32
			// Windows

#else
			// Linux

#endif
#endif
		};
	};

#ifdef __cplusplus
	});
#else

#endif

	return result;
};

unsigned long __stdcall	AudioDataServer::ListenThread(_In_ void* lpParameter)
{
	return 0;
};

// Send method (broadcasting - unique)
unsigned long __stdcall AudioDataServer::SendThread(_In_ void* lpParameter)
{
	int result = 1;

	CondContQueueTuple*		RESTRICT ptrTuple;
	bool*					RESTRICT ptrCondition;
	ConnectionContainer*	RESTRICT ptrContainer;
	AudioDataQueue*			RESTRICT ptrQueue;

#ifndef __cplusplus
	// C pointer
	AudioDatagramPtr		RESTRICT ptrDatagram;

	ptrTuple = ((CondContPair *)lpParameter);

#else
	// C++ smart pointer
	AudioDatagramPtr		ptrDatagram;

	ptrTuple = static_cast<CondContQueueTuple*>(lpParameter);
#endif

	ptrCondition	= ptrTuple->pBool;
	ptrContainer	= ptrTuple->pCont;
	ptrQueue		= ptrTuple->pQueue;

	do
	{
		// Is there any advantage of calling empty() first
		// to check emptiness rather than calling try_pop() always?
		// If then the code below is meaningful.
		// And if not, it should be modified.
		if (!ptrQueue->empty() && ptrQueue->try_pop(ptrDatagram))
			for (ConnectionContainer::const_iterator it =
				ptrContainer->cbegin() ; it != ptrContainer->cend() ; ++it)
		{
#ifdef __cplusplus
	#define PTR_DATAGRAM ptrDatagram.get()
#else
	#define PTR_DATAGRAM ptrDatagram
#endif
			result = send(it->first,
				(const char *) PTR_DATAGRAM,
				sizeof(AudioDatagram::addr) +
				sizeof(AudioDatagram::dataLength) +
				sizeof(AudioDatagram::dataType) +
				(ptrDatagram->dataLength),
				0
			);
		};
	} while (*ptrCondition && result);

	return (result < 0 ? 

#ifdef _WIN32
		WSAGetLastError()
#else
		errno
#endif
		: 0);
};

// Recv method (individual)
unsigned long __stdcall AudioDataServer::RecvThread(_In_ void* lpParameter)
{
	int result;

	ConnectionQueuePair* RESTRICT ptrPair;
	SOCKET clientSocket;
	struct sockaddr_in clientAddr;
	AudioDataQueue*	RESTRICT ptrQueue;

	AudioDatagram		dataBuffer;
	AudioDatagramPtr	ptrNewData;
#ifdef __cplusplus
	ptrPair = static_cast<ConnectionQueuePair *>(lpParameter);
#else
	ptrPair = ((ConnectionQueuePair *) lpParameter);
#endif

	clientSocket	= ptrPair->first.first;
	clientAddr		= ptrPair->first.second;
	ptrQueue		= ptrPair->second;


	while (result = recv(clientSocket, (char *) &dataBuffer, sizeof(AudioDatagram), 0))
	{
#ifdef __cplusplus
		try
		{
			ptrNewData = AudioDatagramPtr(new AudioDatagram(dataBuffer));
		}
		catch (const std::bad_alloc&)
		{
			break;
		};
#else
		ptrNewData = (AudioDatagramPtr) malloc(sizeof(AudioDatagram));
		if (!ptrNewData) break;
		
		memcpy(ptrNewData, &dataBuffer, sizeof(AudioDatagram));
#endif

		ptrQueue->push(ptrNewData);
	};

	return (result == SOCKET_ERROR ?

#ifdef _WIN32
		WSAGetLastError()
#else
		errno
#endif

		: 0);
};

int AudioDataServer::BindSocket(void)
{
	DWORD output;
	GUID guid_rio = WSAID_MULTIPLE_RIO;

	//serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_REGISTERED_IO);
	if (WSAIoctl(serverSocket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &guid_rio,
		sizeof(GUID), &rioFuncTable, sizeof(RIO_EXTENSION_FUNCTION_TABLE), &output, 0, 0))
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::BindSocket_WSAIoctl"));
#endif
		return WSAGetLastError();
	};

	// Create I/O completion port
	HANDLE handleIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	if (!handleIOCP)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::BindSocket_CreateIoCompletionPort"));
#endif
		return ((int) GetLastError());
	};

	// Maybe you can initialize with designated initializer list "IF" VS supports C99...
	/// compType = { RIO_IOCP_COMPLETION, .Iocp = { handleIOCP, 0, &overlapped } };
	OVERLAPPED overlapped;
	compType = { RIO_IOCP_COMPLETION };
	compType.Iocp = { handleIOCP, 0, &overlapped };

	// Create Registered I/O completion queue first.
	if ((compQueue = rioFuncTable.RIOCreateCompletionQueue(10000UL, &compType)) == RIO_INVALID_CQ)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::BindSocket_RIOCreateCompletionQueue"));
#endif
		return WSAGetLastError();
	};

	// Create Registered I/O request queue after.											
	if ((reqQueue = rioFuncTable.RIOCreateRequestQueue(
		serverSocket, 5000UL, 1UL, 5000UL, 1UL, compQueue, compQueue, 0)) == RIO_INVALID_RQ)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::BindSocket_RIOCreateRequestQueue"));
#endif
		return WSAGetLastError();
	};

	// Register the buffer to make the system locking buffer.
	rioIdData = rioFuncTable.RIORegisterBuffer(dataBuffer, sizeof(dataBuffer));
	if (rioIdData == RIO_INVALID_BUFFERID)
	{
#ifdef _DEBUG
		DEBUG_OUTPUT(TEXT("Fatal error on AudioDataServer::BindSocket_RIORegisterBuffer"));
#endif
		return WSAGetLastError();
	};

};