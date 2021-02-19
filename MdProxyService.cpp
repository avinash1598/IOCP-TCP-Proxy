//#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <winsock2.h>
#include <unordered_map>
//#include <vector>
#include <Mstcpip.h>
#include <intsafe.h>
#include <memory>
#pragma comment(lib, "Ws2_32.lib")

#include "MdProxyService.h"
#include "KernelCommunicator.h"
//#include "SocketContext.h"
#include "SocketContextManager.h"

HANDLE g_hShutdownEvent = NULL;
int g_nThreads = 0;
HANDLE* g_phWorkerThreads = NULL;
HANDLE g_hAcceptThread = NULL;
HANDLE g_hJunkCleanUpThread = NULL;
CRITICAL_SECTION g_csConsole; 
HANDLE g_hIOCompletionPort = NULL;
WSAEVENT g_hAcceptEvent;

using namespace std;
using namespace CPlusPlusLogging;


int main(int argc, char* argv[])
{
	//Update user application process id to WFP kernel driver
	if (TRUE != UpdateKernelWithUserAppProcessId())
	{
		LOG_ERROR("\nError updating data to kernel driver.");
		return 1;
	}

	/*
	//Validate the input
	if (argc < 2)
	{
		printf("\nUsage: %s port.", argv[0]);
		return 1;
	}*/
	LOG_INFO("Usage: %s port.", PROXY_SERVER_PORT);
	
	if (false == Initialize())
	{
		return 1;
	}

	SOCKET ListenSocket;

	struct sockaddr_in ServerAddress;

	//Overlapped I/O follows the model established in Windows and can be performed only on 
	//sockets created through the WSASocket function 
	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == ListenSocket)
	{
		LOG_ERROR("Error occurred while opening socket: %d.", WSAGetLastError());
		goto error;
	}
	else
	{
		LOG_INFO("WSASocket() successful.");
	}

	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char*)&ServerAddress, sizeof(ServerAddress));

	//Port number will be supplied as a command line argument
	int nPortNo;
	nPortNo = atoi(PROXY_SERVER_PORT);

	//Fill up the address structure
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = INADDR_ANY; //WinSock will supply address
	ServerAddress.sin_port = htons(nPortNo);    //comes from commandline

	//Assign local address and port number
	if (SOCKET_ERROR == bind(ListenSocket, (struct sockaddr*) & ServerAddress, sizeof(ServerAddress)))
	{
		closesocket(ListenSocket);
		LOG_ERROR("Error occurred while binding.");
		goto error;
	}
	else
	{
		LOG_INFO("bind() successful.");
	}

	//Make the socket a listening socket
	if (SOCKET_ERROR == listen(ListenSocket, SOMAXCONN))
	{
		closesocket(ListenSocket);
		LOG_ERROR("Error occurred while listening.");
		goto error;
	}
	else
	{
		LOG_INFO("listen() successful.");
	}

	g_hAcceptEvent = WSACreateEvent();

	if (WSA_INVALID_EVENT == g_hAcceptEvent)
	{
		LOG_ERROR("Error occurred while WSACreateEvent().");
		goto error;
	}

	if (SOCKET_ERROR == WSAEventSelect(ListenSocket, g_hAcceptEvent, FD_ACCEPT))
	{
		LOG_ERROR("Error occurred while WSAEventSelect().");
		WSACloseEvent(g_hAcceptEvent);
		goto error;
	}

	LOG_INFO("To exit this server, hit a key at any time on this console...");

	DWORD nThreadID;
	g_hAcceptThread = CreateThread(0, 0, AcceptThread, (void*)ListenSocket, 0, &nThreadID);

	//DWORD cleanupThreadID;
	//g_hJunkCleanUpThread = CreateThread(0, 0, JunkCleanUpThread, (void*)0, 0, &cleanupThreadID);

	//Hang in there till a key is hit
	while (!_kbhit())
	{
		Sleep(0);  //switch to some other thread
	}

	LOG_INFO("Server is shutting down...");

	//Start cleanup
	CleanUp();

	//Close open sockets
	closesocket(ListenSocket);

	DeInitialize();

	return 0; //success

error:
	closesocket(ListenSocket);
	DeInitialize();
	return 1;
}

bool Initialize()
{
	//Find out number of processors and threads
	g_nThreads = WORKER_THREADS_PER_PROCESSOR * GetNoOfProcessors();

	LOG_INFO("Number of processors on host: %d", GetNoOfProcessors());

	LOG_INFO("The following number of worker threads will be created: %d", g_nThreads);

	//Allocate memory to store thread handless
	g_phWorkerThreads = new HANDLE[g_nThreads];

	//Initialize the Console Critical Section
	InitializeCriticalSection(&g_csConsole);

	SocketContextManager::Initialize();

	//Create shutdown event
	g_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// Initialize Winsock
	WSADATA wsaData;

	int nResult;
	nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (NO_ERROR != nResult)
	{
		LOG_ERROR("Error occurred while executing WSAStartup().");
		return false; //error
	}
	else
	{
		LOG_INFO("WSAStartup() successful.");
	}

	if (false == InitializeIOCP())
	{
		LOG_ERROR("Error occurred while initializing IOCP");
		return false;
	}
	else
	{
		LOG_INFO("IOCP initialization successful.");
	}

	return true;
}

//Function to Initialize IOCP
bool InitializeIOCP()
{
	//Create I/O completion port
	g_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (NULL == g_hIOCompletionPort)
	{
		LOG_ERROR("Error occurred while creating IOCP: %d.", WSAGetLastError());
		return false;
	}

	DWORD nThreadID;

	//Create worker threads
	for (int ii = 0; ii < g_nThreads; ii++)
	{
		g_phWorkerThreads[ii] = CreateThread(0, 0, WorkerThread, (void*)(ii + 1), 0, &nThreadID);
	}

	return true;
}

void CleanUp()
{
	//Ask all threads to start shutting down
	SetEvent(g_hShutdownEvent);
	
	//Let Accept thread go down
	WaitForSingleObject(g_hAcceptThread, INFINITE);

	//Let Junk clean up thread go down
	//WaitForSingleObject(g_hJunkCleanUpThread, INFINITE);
	
	for (int i = 0; i < g_nThreads; i++)
	{
		//Help threads get out of blocking - GetQueuedCompletionStatus()
		PostQueuedCompletionStatus(g_hIOCompletionPort, 0, (DWORD)NULL, NULL);
	}
	
	//Let Worker Threads shutdown
	WaitForMultipleObjects(g_nThreads, g_phWorkerThreads, TRUE, INFINITE);
	
	//We are done with this event
	WSACloseEvent(g_hAcceptEvent);
	
	//Cleanup dynamic memory allocations, if there are any.
	SocketContextManager::CleanMap();
}

void DeInitialize()
{
	SocketContextManager::CleanUp();

	//Delete the Console Critical Section.
	DeleteCriticalSection(&g_csConsole);

	//Cleanup IOCP.
	CloseHandle(g_hIOCompletionPort);

	//Clean up the event.
	CloseHandle(g_hShutdownEvent);

	//Clean up memory allocated for the storage of thread handles
	delete[] g_phWorkerThreads;

	//Cleanup Winsock
	WSACleanup();
}

//This thread will look for accept event
DWORD WINAPI AcceptThread(LPVOID lParam)
{
	SOCKET ListenSocket = (SOCKET)lParam;

	WSANETWORKEVENTS WSAEvents;

	//Accept thread will be around to look for accept event, until a Shutdown event is not Signaled.
	while (WAIT_OBJECT_0 != WaitForSingleObject(g_hShutdownEvent, 0))
	{
		if (WSA_WAIT_TIMEOUT != WSAWaitForMultipleEvents(1, &g_hAcceptEvent, FALSE, WAIT_TIMEOUT_INTERVAL, FALSE))
		{
			WSAEnumNetworkEvents(ListenSocket, g_hAcceptEvent, &WSAEvents);
			if ((WSAEvents.lNetworkEvents & FD_ACCEPT) && (0 == WSAEvents.iErrorCode[FD_ACCEPT_BIT]))
			{
				//Process it
				AcceptConnection(ListenSocket);
			}
		}
	}

	return 0;
}

//This function will process the accept event
void AcceptConnection(SOCKET ListenSocket)
{
	BOOL status = TRUE;
	std::shared_ptr<SocketContext> pClientContext = nullptr;
	std::shared_ptr<SocketContext> pRemoteSocketContext = nullptr;
	SOCKET Socket = NULL;

	sockaddr_in ClientAddress;
	int nClientLength = sizeof(ClientAddress);
	
	//Accept remote connection attempt from the client
	Socket = WSAAccept(ListenSocket, (sockaddr*)&ClientAddress, &nClientLength, NULL, 0);

	if (INVALID_SOCKET == Socket)
	{
		LOG_ERROR("Error occurred while accepting socket: %ld.", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}

	//Display Client's IP
	LOG_DEBUG("New connection accepted from: %s", inet_ntoa(ClientAddress.sin_addr));

	//Create a new ClientContext for this newly accepted client
	pClientContext = std::make_shared<SocketContext>();
	pClientContext->SetSocket(Socket);
	pClientContext->SetId(SocketContextManager::GetSocketId());
	pClientContext->SetSocketAddress(ClientAddress);
	pClientContext->SetSockType("LOCAL SOCKET");

	LOG_DEBUG("Client socket id: %ld", pClientContext->GetId());

	//Create remote connection
	pRemoteSocketContext = InitRemoteConnection(pClientContext);

	if (!pRemoteSocketContext) 
	{
		LOG_ERROR("Could not establish connection to remote socket.");
		status = FALSE;
		goto Exit;
	}

	//This is the right place to link two sockets
	pClientContext->SetBuddySocketContext(pRemoteSocketContext);
	pRemoteSocketContext->SetBuddySocketContext(pClientContext);

	//This is the right place to store socket contexts
	SocketContextManager::AddToMap(pClientContext->GetId(), pClientContext);
	SocketContextManager::AddToMap(pRemoteSocketContext->GetId(), pRemoteSocketContext);

	//Associate both local and remote socket with IOCP.
	if (false == AssociateWithIOCP(pClientContext))
	{
		LOG_ERROR("Error associating local socket with IOCP.");
		status = FALSE;
		goto Exit;
	}

	if (false == AssociateWithIOCP(pRemoteSocketContext))
	{
		LOG_ERROR("Error associating remote proxy socket with IOCP.");
		status = FALSE;
		goto Exit;
	}
	
	LOG_DEBUG("Associated client and remote socket with I/O Completion port.");
	//Post initial Recv
	//This is a right place to post a initial Recv
	//Posting a initial Recv in WorkerThread will create scalability issues.
	if (FALSE == pClientContext->Recv())
	{
		LOG_ERROR("Error in Initial Post %d.", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}

	if (FALSE == pRemoteSocketContext->Recv())
	{
		LOG_ERROR("Error in Initial Post %d.", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}
	
	LOG_INFO("Proxied connection established from %s to %s.", 
		pClientContext->GetSocketIpAddress(), pRemoteSocketContext->GetSocketIpAddress());
	LOG_DEBUG("Client socket referene count: %ld", pClientContext.use_count());
	LOG_DEBUG("Remote socket reference count: %ld", pRemoteSocketContext.use_count());

Exit:
	if (FALSE == status) 
	{
		if (Socket)
		{
			LOG_DEBUG("Closing client socket.");
			closesocket(Socket);
			Socket = NULL;
		}
		if (pClientContext || pRemoteSocketContext)
		{
			LOG_DEBUG("Cleaning up client socket context data.");
			SocketContextManager::RemoveFromMapAndCleanUpMemory(pClientContext->GetId());
			pClientContext = nullptr;
		}
		if (pRemoteSocketContext)
		{
			LOG_DEBUG("Cleaning up remote socket context data.");
			SocketContextManager::RemoveFromMapAndCleanUpMemory(pRemoteSocketContext->GetId());
			pRemoteSocketContext = nullptr;
		}
	}
}

//Create connection to remote server socket
//TODO: get destination server address here
std::shared_ptr<SocketContext> InitRemoteConnection(std::shared_ptr<SocketContext> pClientContext)
{
	LOG_DEBUG("Initialiting remote connection...");

	BOOL status = TRUE;

	const SIZE_T REDIRECT_RECORDS_SIZE = 2048;
	const SIZE_T REDIRECT_CONTEXT_SIZE = sizeof(SOCKADDR_STORAGE) * 2;

	UINT32 wsaStatus;
	SOCKET remoteProxySocket = NULL;
	std::shared_ptr<SocketContext> pRemoteSockContext = nullptr;

	BYTE** pRedirectRecords = 0;
	BYTE** pRedirectContext = 0;
	SIZE_T redirectRecordsSize = 0;
	SIZE_T redirectContextSize = 0;
	SIZE_T redirectRecordsSet = 0;
	
	SOCKADDR_STORAGE* pRemoteSockAddrStorage = 0;

	//Allocate memory in heap else winsock will throw 10014 error
	//Initialization on top causes memory to be allocated in stack
	HLPR_NEW_ARRAY(pRedirectRecords, BYTE*, REDIRECT_RECORDS_SIZE);
	HLPR_NEW_ARRAY(pRedirectContext, BYTE*, REDIRECT_CONTEXT_SIZE);
	HLPR_NEW_ARRAY(pRemoteSockAddrStorage, SOCKADDR_STORAGE, 1);
	
	//Opaque data to be set on proxy connection
	wsaStatus = WSAIoctl(pClientContext->GetSocket(),
					  SIO_QUERY_WFP_CONNECTION_REDIRECT_RECORDS,
					  0, 
					  0,
					  (BYTE*)pRedirectRecords,
					  REDIRECT_RECORDS_SIZE,
					  (LPDWORD)&redirectRecordsSize,
					  0, 
					  0);

	if (NO_ERROR != wsaStatus)
	{
		LOG_ERROR("Unable to get redirect records from socket: %ld", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}
	
	//Callout allocated data, contains original destination information
	wsaStatus = WSAIoctl(pClientContext->GetSocket(),
					  SIO_QUERY_WFP_CONNECTION_REDIRECT_CONTEXT,
					  0,
				      0,
					  (BYTE*)pRedirectContext,
					  REDIRECT_CONTEXT_SIZE,
					  (LPDWORD)&redirectContextSize,
					  0,
					  0);

	if (NO_ERROR != wsaStatus)
	{
		LOG_ERROR("Unable to get redirect context from socket: %ld", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}
	
	// copy remote address
	RtlCopyMemory(pRemoteSockAddrStorage, 
				  &(((SOCKADDR_STORAGE*)pRedirectContext)[0]), 
				  sizeof(SOCKADDR_STORAGE));
	
	remoteProxySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	if (INVALID_SOCKET == remoteProxySocket)
	{
		LOG_ERROR("Error occured while opening socket: %ld", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}
	
	wsaStatus = WSAIoctl(remoteProxySocket,
					  SIO_SET_WFP_CONNECTION_REDIRECT_RECORDS,
					  (BYTE*)pRedirectRecords,
					  (DWORD)redirectRecordsSize,
					  0,
					  0,
					  (LPDWORD)&redirectRecordsSet,
					  0,
					  0);

	if (NO_ERROR != wsaStatus)
	{
		LOG_ERROR("Unable to set redirect records on socket: %ld", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}

	/*
		Check no longer valid:
		Refer this: https://social.msdn.microsoft.com/Forums/en-US/be79cc7f-e2ff-47ce-bc83-f79307680042/seting-redirect-records-on-the-proxy-connection-socket-correctly?forum=wfp
	*/
	if (redirectRecordsSize != redirectRecordsSet)
	{
		LOG_ALARM("Redirect record size mismatch. %ld and %ld", 
					   redirectRecordsSize, redirectRecordsSet);
	}

	wsaStatus = WSAConnect(remoteProxySocket, (SOCKADDR*)pRemoteSockAddrStorage, sizeof(SOCKADDR_STORAGE), 0, 0, 0, 0);

	if (SOCKET_ERROR == wsaStatus)
	{
		LOG_ERROR("Error occured while connecting to remote server: %ld", WSAGetLastError());
		status = FALSE;
		goto Exit;
	}
	
	LOG_DEBUG("Remote socket connected to %s.", 
		inet_ntoa(((SOCKADDR_IN*)pRemoteSockAddrStorage)->sin_addr));

	pRemoteSockContext = std::make_shared<SocketContext>();
	
	pRemoteSockContext->SetProxySocket(TRUE);
	pRemoteSockContext->SetSocket(remoteProxySocket);
	pRemoteSockContext->SetId(SocketContextManager::GetSocketId());
	pRemoteSockContext->SetSocketAddress(*((SOCKADDR_IN*)pRemoteSockAddrStorage));
	pRemoteSockContext->SetSockType("REMOTE SOCKET");

	LOG_DEBUG("Remote socket Id: %ld", pRemoteSockContext->GetId());

Exit:
	if (FALSE == status)
	{
		if (remoteProxySocket)
		{
			LOG_DEBUG("Closing remote socket.");
			closesocket(remoteProxySocket);
			remoteProxySocket = NULL;
		}
		if (pRemoteSockContext)
		{
			LOG_DEBUG("Cleaning up remote socket context data.");
			pRemoteSockContext = nullptr;
		}
	}
	
	HLPR_DELETE_ARRAY(pRedirectContext);
	HLPR_DELETE_ARRAY(pRedirectRecords);
	HLPR_DELETE_ARRAY(pRemoteSockAddrStorage);

	return pRemoteSockContext;
}

bool AssociateWithIOCP(std::shared_ptr<SocketContext> &pClientContext)
{
	//Associate the socket with IOCP
	//here we are only passing reference to our client context
	//Get socket context from list. 
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pClientContext->GetSocket(), 
										  g_hIOCompletionPort, 
										  (DWORD)&SocketContextManager::GetFromMap(pClientContext->GetId()), 
										  0);

	if (NULL == hTemp)
	{
		LOG_ERROR("Error occurred while executing CreateIoCompletionPort().");

		//Let's not work with this client
		SocketContextManager::RemoveFromMapAndCleanUpMemory(pClientContext->GetId());

		return false;
	}

	return true;
}

//This thread will keep cleaning the junk socket map
DWORD WINAPI JunkCleanUpThread(LPVOID lParam)
{
	//Junk cleanup thread will periodically cleanup junk list, until a Shutdown event is not Signaled.
	while (WAIT_OBJECT_0 != WaitForSingleObject(g_hShutdownEvent, 0))
	{
		LOG_DEBUG("Cleaning Junk socket context list");
		SocketContextManager::CleanUpJunkSocketContextMap(false);
		Sleep(THREAD_SLEEP_INTERVAL);
	}

	return 0;
}

/*
//Function to synchronize console output
//Threads need to be synchronized while they write to console.
//WriteConsole() API can be used, it is thread-safe, I think.
//I have created my own function.
void WriteToConsole(const char* szFormat, ...)
{
	EnterCriticalSection(&g_csConsole);

	va_list args;
	va_start(args, szFormat);

	vprintf(szFormat, args);

	va_end(args);

	LeaveCriticalSection(&g_csConsole);
}
*/

//The use of static variable will ensure that 
//we will make a call to GetSystemInfo() 
//to find out number of processors, 
//only if we don't have the information already.
//Repeated use of this function will be efficient.
int GetNoOfProcessors()
{
	static int nProcessors = 0;

	if (0 == nProcessors)
	{
		SYSTEM_INFO si;

		GetSystemInfo(&si);

		nProcessors = si.dwNumberOfProcessors;
	}

	return nProcessors;
}