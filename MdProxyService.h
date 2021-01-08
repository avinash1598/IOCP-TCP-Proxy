#ifndef _MD_PROXY_SERVICE_H_
#define _MD_PROXY_SERVICE_H_

//Disable deprecation warnings
#pragma warning(disable: 4996)

//Op codes for IOCP
#define OP_READ     0
#define OP_WRITE    1

#define WORKER_THREADS_PER_PROCESSOR 2

//Buffer Length 
#define MAX_BUFFER_LEN 8192

//Time out interval for wait calls
#define WAIT_TIMEOUT_INTERVAL 100

//Proxy service port 
#define PROXY_SERVER_PORT "1598"

//Allocate memory with new and zero out its contents.
#define HLPR_NEW_ARRAY(pPtr, object, count)           \
   do                                                 \
   {                                                  \
      size_t SAFE_SIZE = 0;                           \
      HLPR_DELETE_ARRAY(pPtr);                        \
      if(SizeTMult(sizeof(object),                    \
                   (size_t)count,                     \
                   &SAFE_SIZE) == S_OK &&             \
         SAFE_SIZE >= (sizeof(object) * count))       \
      {                                               \
         pPtr = new object[count];                    \
         if(pPtr)                                     \
            SecureZeroMemory(pPtr,                    \
                             SAFE_SIZE);              \
      }                                               \
      else                                            \
      {                                               \
         break;                                       \
      }                                               \
   }while(pPtr == 0)

//Free memory allocated with new[] and set the pointer to 0
#define HLPR_DELETE_ARRAY(pPtr) \
   if(pPtr)                     \
   {                            \
      delete[] pPtr;            \
      pPtr = 0;                 \
   }

//Graceful shutdown Event
//For this simple implementation,
//We can use global variable as well.
//Wanted to demonstrate use of event
//for shutdown
extern HANDLE g_hShutdownEvent;

//Number of threads to be created.
extern int g_nThreads;

//To store handle of worker threads
extern HANDLE* g_phWorkerThreads;

//Handle for Accept related thread
extern HANDLE g_hAcceptThread;

//Network Event for Accept
extern WSAEVENT	g_hAcceptEvent;

extern CRITICAL_SECTION g_csConsole; //When threads write to console we need mutual exclusion
extern CRITICAL_SECTION g_csClientList; //Need to protect the client list

//Global I/O completion port handle
extern HANDLE g_hIOCompletionPort;

class SocketContext  //To store and manage client related information
{
private:

	OVERLAPPED* m_pol;
	WSABUF* m_pwbuf;

	int               m_nTotalBytes;
	int               m_nSentBytes;

	SOCKET            m_Socket;  //accepted socket
	int               m_nOpCode; //will be used by the worker thread to decide what operation to perform
	char              m_szBuffer[MAX_BUFFER_LEN];

	BOOL              m_ProxySocket;

	SocketContext* pFwd_SocketContext;

public:

	//Get/Set calls
	void SetFwdScoketContext(SocketContext* socketContext)
	{
		pFwd_SocketContext = socketContext;
	}

	SocketContext* GetFwdScoketContext()
	{
		return pFwd_SocketContext;
	}

	void SetProxySocket(BOOL value) 
	{
		m_ProxySocket = value;
	}

	BOOL GetProxySocket()
	{
		return m_ProxySocket;
	}

	void SetOpCode(int n)
	{
		m_nOpCode = n;
	}

	int GetOpCode()
	{
		return m_nOpCode;
	}

	void SetTotalBytes(int n)
	{
		m_nTotalBytes = n;
	}

	int GetTotalBytes()
	{
		return m_nTotalBytes;
	}

	void SetSentBytes(int n)
	{
		m_nSentBytes = n;
	}

	void IncrSentBytes(int n)
	{
		m_nSentBytes += n;
	}

	int GetSentBytes()
	{
		return m_nSentBytes;
	}

	void SetSocket(SOCKET s)
	{
		m_Socket = s;
	}

	SOCKET GetSocket()
	{
		return m_Socket;
	}

	void SetBuffer(char* szBuffer)
	{
		strcpy(m_szBuffer, szBuffer);
	}

	void GetBuffer(char* szBuffer)
	{
		strcpy(szBuffer, m_szBuffer);
	}

	void ZeroBuffer()
	{
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
	}

	void SetWSABUFLength(int nLength)
	{
		m_pwbuf->len = nLength;
	}

	int GetWSABUFLength()
	{
		return m_pwbuf->len;
	}

	WSABUF* GetWSABUFPtr()
	{
		return m_pwbuf;
	}

	OVERLAPPED* GetOVERLAPPEDPtr()
	{
		return m_pol;
	}

	void ResetWSABUF()
	{
		ZeroBuffer();
		m_pwbuf->buf = m_szBuffer;
		m_pwbuf->len = MAX_BUFFER_LEN;
	}

	//Constructor
	SocketContext()
	{
		m_pol = new OVERLAPPED;
		m_pwbuf = new WSABUF;

		ZeroMemory(m_pol, sizeof(OVERLAPPED));

		m_Socket = SOCKET_ERROR;

		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);

		m_pwbuf->buf = m_szBuffer;
		m_pwbuf->len = MAX_BUFFER_LEN;

		m_nOpCode = 0;
		m_nTotalBytes = 0;
		m_nSentBytes = 0;
		m_ProxySocket = FALSE;
	}

	//destructor
	~SocketContext()
	{
		//Cancel all IO operations
		CancelIoEx((HANDLE)m_Socket, m_pol);
		SleepEx(0, TRUE); // the completion will be called here
		closesocket(m_Socket);

		//Cleanup
		delete m_pol;
		delete m_pwbuf;
	}
};

//Vector to store pointers of dynamically allocated ClientContext.
//map class can also be used.
//Link list can also be created.
extern std::vector<SocketContext*> g_ClientContext;

//global functions
bool InitializeIOCP();
bool Initialize();
void CleanUp();
void DeInitialize();
DWORD WINAPI AcceptThread(LPVOID lParam);
void AcceptConnection(SOCKET ListenSocket);
bool AssociateWithIOCP(SocketContext* pClientContext);
DWORD WINAPI WorkerThread(LPVOID lpParam);
void WriteToConsole(const char* szFormat, ...);
void AddToClientList(SocketContext* pClientContext);
void RemoveFromClientListAndFreeMemory(SocketContext* pClientContext, int depth = 1);
void CleanClientList();
int GetNoOfProcessors();
SocketContext* InitRemoteConnection(SocketContext* pClientContext);

#endif