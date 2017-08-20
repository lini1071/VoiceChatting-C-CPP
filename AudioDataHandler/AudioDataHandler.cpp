#include "stdafx.h"
#include "AudioDataHandler.h"
#include <Strsafe.h>

// Optional entry in Windows dynamic library.
#ifdef _WINDLL

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
}

#endif


#ifdef __cplusplus

AudioDataHandler::AudioDataHandler(void) :
	eventSocketConnected(0),
	isConnected(false)
{
	ptrCollector = new AudioDeviceHandler(&ptrPlayback, &ptrCapture);
	ptrPlayback = new AudioPlayback(&recvQueue);
	ptrCapture	= new AudioCapture(&sendQueue);
};

AudioDataHandler::~AudioDataHandler(void)
{
	delete ptrPlayback;
	delete ptrCapture;
	delete ptrCollector;
};

#endif

// If no error occurred, it returns 0.
int	AudioDataHandler::Initialize(void)
{
	WSADATA wsaData;
	unsigned short wordVersionRequest = MAKEWORD(2, 2);
	int result;
	
	// First call WSAStartup
	if (result = WSAStartup(wordVersionRequest, &wsaData)) return result;
	else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		return WSAVERNOTSUPPORTED;
	};

	// And create WSAEVENT for getting connected notice
	eventSocketConnected = WSACreateEvent();
	if (eventSocketConnected == WSA_INVALID_EVENT)
	{
		result = WSAGetLastError();
		WSACleanup();
		return result;
	}
	else return 0;
};

// If no error occurred, it returns 0.
int	AudioDataHandler::Terminate(void)
{
	int result;

	// Close WSAEVENT
	if (eventSocketConnected) WSACloseEvent(&eventSocketConnected);

	result = WSACleanup();
	return (result ? WSAGetLastError() : 0);
};

// Create a socket, and connect to the destination.
int AudioDataHandler::TryConnectW(_In_ PCWSTR string_addr)
{
	unsigned int port = DEFAULT_PORT;
	ADDRINFOW ai_hints = { 0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP }, *ai_result;
	int result;

	// Check port range
	if ((0x0000FFFF < port) || (port == 0x0)) return -1;

	// Create socket object, and register connect event
	priSocket = WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (priSocket == INVALID_SOCKET) return WSAGetLastError();

	result = WSAEventSelect(priSocket, eventSocketConnected, FD_CONNECT);
	if (result == SOCKET_ERROR) return WSAGetLastError();

	// Convert integer value to string
	WCHAR string_port[6];
	StringCbPrintf(string_port, sizeof(string_port), L"%hu", port);

	// Get address info struct and try connecting to server.
	GetAddrInfoW(string_addr, string_port, &ai_hints, &ai_result);
	result = WSAConnect(priSocket, ai_result->ai_addr, (int) ai_result->ai_addrlen, 0, 0, 0, 0);
	FreeAddrInfoW(ai_result);

	return (result == SOCKET_ERROR ? WSAGetLastError() : 0);
};
int AudioDataHandler::TryConnect(_In_ const char* string_addr)
{
	addrinfo ai_hints = { 0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP }, *ai_result = nullptr;
	unsigned int	port = DEFAULT_PORT;
	unsigned long	argp = 1;	// non-zero value for enabling non-blocking mode
	int result;

	// Check port range
	if ((0x0000FFFF < port) || (port == 0x0)) return -1;

	// Create socket object, and register connect event
	priSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (priSocket == INVALID_SOCKET) return WSAGetLastError();

	result = WSAEventSelect(priSocket, eventSocketConnected, FD_CONNECT);
	if (result == SOCKET_ERROR) return WSAGetLastError();

	// Make the socket asynchronous
	result = ioctlsocket(priSocket, FIONBIO, &argp);
	if (result == SOCKET_ERROR) return WSAGetLastError();

	// Convert integer value to string
	char string_port[6];
	sprintf_s(string_port, sizeof(string_port), "%hu", port);

	// Get address info struct, and try connect to the server.
	getaddrinfo(string_addr, string_port, &ai_hints, &ai_result);
	result = connect(priSocket, ai_result->ai_addr, (int) ai_result->ai_addrlen);
	freeaddrinfo(ai_result);

	return (result == SOCKET_ERROR ? WSAGetLastError() : 0);
};
// Shutdown and close socket.
int AudioDataHandler::Disconnect(void)
{
	int result;

	// We don't need performing graceful disconnect
	result = shutdown(priSocket, SD_BOTH);
	if (result) return WSAGetLastError();
	else		return (closesocket(priSocket) ? WSAGetLastError() : 0);
};

// If the function succeeds, the return value is 0.
unsigned long AudioDataHandler::StartSocketThread(void)
{
	handleSocketThread = CreateThread(NULL, 0, this->SocketThread, this, 0, NULL);
	return (handleSocketThread ? 0 : GetLastError());
};
unsigned long AudioDataHandler::StopSocketThread(void)
{
	this->isConnected = false;
	return WaitForSingleObject(handleSocketThread, INFINITE);
};

// The functions below are for optical appearance of GUI compnents,
// but at this point I can't assure time integrity of data.
AudioDatagram	AudioDataHandler::GetCapturedAudioDatagram(void)
{
	return ptrCapture->GetAudioDatagram();
};
AudioDatagram	AudioDataHandler::GetPlaybackAudioDatagram(void)
{
	return ptrPlayback->GetAudioDatagram();
};

HANDLE	AudioDataHandler::CreateCaptureEvent(void)
{
	return (ptrCapture ? ptrCapture->CreateNewEventHandle() : nullptr);
};
bool	AudioDataHandler::RemoveCaptureEvent(_In_ HANDLE handle)
{
	return (ptrCapture ? ptrCapture->RemoveEventHandle(handle) : false);
};
HANDLE	AudioDataHandler::CreatePlaybackEvent(void)
{
	return (ptrPlayback ? ptrPlayback->CreateNewEventHandle() : nullptr);
};
bool	AudioDataHandler::RemovePlaybackEvent(_In_ HANDLE handle)
{
	return (ptrPlayback ? ptrPlayback->RemoveEventHandle(handle) : false);
};

// ThreadProc callback function
DWORD WINAPI	AudioDataHandler::SocketThread(LPVOID lpParameter)
{
	// lpParameter must point this instance.
	AudioDataHandler *handler = static_cast<AudioDataHandler *>(lpParameter);
	return handler->PerformSocketOps();
};

// Socket handling method with blocking socket
/*
DWORD	AudioDataHandler::PerformSocketOps(void)
{
	AudioDatagram* ptrBuffer;
	int	result;

	// recv and send may be blocked periodically.
	while (this->isConnected)
	{
		// Receive remote data from a server, or other clients
		ptrBuffer = new AudioDatagram;
		result = recv(priSocket, (char *) ptrBuffer, sizeof(AudioDatagram), 0);
		if (result > 0) recvQueue.push(ptrBuffer);

		// Send queued data from a server, or other clients
		if (!this->sendQueue.empty())
		{
			ptrBuffer = sendQueue.front();
			result = send(priSocket, (const char *) ptrBuffer, ptrBuffer->dataLength +
				(sizeof(AudioDatagram::addr) + sizeof(AudioDatagram::dataLength)), 0);
			sendQueue.pop();
		};
	};

	return 0;
};
*/

// Socket handling method with overlapped I/O
DWORD	AudioDataHandler::PerformSocketOps(void)
{
	AudioDatagram	 *ptrDataRecv, *ptrDataSend;
	ContextContainer *ptrContRecv, *ptrContSend;
	int	error = 0, result;
	
	DWORD waitResult = WSAWaitForMultipleEvents(1, &eventSocketConnected, 0, DEFAULT_TIMEOUT, 0);
	if		(waitResult == WSA_WAIT_FAILED)		return WSAGetLastError();
	else if (waitResult != WSA_WAIT_EVENT_0)	return waitResult;

	isConnected = true;
	while (isConnected)
	{
		// Variables below can't be assured their lifetime.
		/*
		AudioDatagram	*ptrDataRecv = new AudioDatagram;
		ContextContainer *ptrCont = new ContextContainer{ ptrDataRecv, &recvQueue };

		// OVERLAPPED object needs to be zero-initialized
		// If lpCompletionRoutine is not NULL, the hEvent parameter can be used
		// by the application to pass context information to the completion routine.
		WSAOVERLAPPED	*ptrOIOObj = new WSAOVERLAPPED{ 0, 0, { 0 }, ptrCont };
		WSABUF			bufferRecv = { sizeof(AudioDatagram), (char *) ptrDataRecv };
		*/
		
		ptrDataRecv	= new AudioDatagram;
		/*
		ContextContainer *ptrContRecv	= new ContextContainer{ 0 };
		ptrContRecv->overlapObj.hEvent	= ptrContRecv;
		ptrContRecv->wsabuf				= { sizeof(AudioDatagram), (char *) ptrDataRecv };

		ptrContRecv->ptrQueue			= &recvQueue;
		*/
		ptrContRecv = new ContextContainer{
			{ 0, 0, { 0 }, ptrContRecv },
			{ sizeof(AudioDatagram), (char *) ptrDataRecv },
			ptrDataRecv,
			&recvQueue
		};

		DWORD	recvFlags;

		// Receive remote data from a server, or other clients.
		// But we can't assure that WSARecv gets incoming packet successfully.
		// So queue.push is called in successful completion routine calling.
		result = WSARecv(priSocket, &(ptrContRecv->wsabuf), 1,
			0, &recvFlags, &(ptrContRecv->overlapObj), RecvRoutine);
		if (result == SOCKET_ERROR && CheckWSALastError(&error)) break;

		// Send queued data from a server, or other clients.
		// If there was no item to dequeue, concurrent_queue::try_pop() returns false,
		// So we don't need to call additional concurrent_queue::empty().
		if (this->sendQueue.try_pop(ptrDataSend))
		{
			/*
			ContextContainer *ptrContSend	= new ContextContainer{ 0 };
			ptrContSend->overlapObj.hEvent	= ptrContSend;
			ptrContSend->wsabuf				=
			{
				ptrDataSend->dataLength	+
				(sizeof(AudioDatagram::addr) + sizeof(AudioDatagram::dataLength)),
				(char *) ptrDataSend
			};
			ptrContSend->ptrQueue			= &sendQueue;
			*/

			ptrContSend = new ContextContainer{
				{ 0, 0, { 0 }, ptrContSend },
				{
					( ptrDataSend->dataLength + sizeof(AudioDatagram::addr) +
					sizeof(AudioDatagram::dataType) + sizeof(AudioDatagram::dataLength) ),
					(char *) ptrDataSend
				},
				ptrDataSend,
				&sendQueue
			};

			result = WSASend(priSocket, &(ptrContSend->wsabuf), 1,
				0, 0, &(ptrContSend->overlapObj), SendRoutine);
			if (result == SOCKET_ERROR && CheckWSALastError(&error)) break;
		};
	};

	// It returns 0 when there are no recently occurred error.
	return error;
};

inline int AudioDataHandler::CheckWSALastError(_Out_ int* result)
{
	// Check WSA error value.
	// WSAEWOULDBLOCK is non-critical error and
	// it'll be fine if we give socket some time to solve queued tasks.
	int error = WSAGetLastError();
	if (error != WSA_IO_PENDING)
	{
		if (error != WSAEWOULDBLOCK) return (*result = error);
		else Sleep(25);
	};
	return 0;
};

void WINAPI	AudioDataHandler::SendRoutine(_In_ DWORD error,
	_In_ DWORD cbTransferred, _In_ LPWSAOVERLAPPED lpOverlapped, _In_ DWORD /*flags*/)
{
	if (!error)
	{
		// If send is completed, release datagram buffer and OVERLAPPED object.
		delete (((ContextContainer *) lpOverlapped->hEvent)->ptrBuffer), lpOverlapped;
	};
};
void WINAPI	AudioDataHandler::RecvRoutine(_In_ DWORD error,
	_In_ DWORD cbTransferred, _In_ LPWSAOVERLAPPED lpOverlapped, _In_ DWORD /*flags*/)
{
	if (!error)
	{
		// If receive is completed, push address of buffer and release OVERLAPPED object.
		// pushed pointer is deleted in AudioPlayback::PerformRendering thread loop.
		ContextContainer *ptrContainer = ((ContextContainer *) lpOverlapped->hEvent);
		ptrContainer->ptrQueue->push(ptrContainer->ptrBuffer);
		delete lpOverlapped;
	};
};