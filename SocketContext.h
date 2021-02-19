#ifndef _SOCKET_CONTEXT_H_
#define _SOCKET_CONTEXT_H_

//Disable deprecation warnings
#pragma warning(disable: 4996)

//Op codes for IOCP
#define OP_READ     0
#define OP_WRITE    1

//Buffer Length 
#define MAX_BUFFER_LEN 512

//Structure to hold all the send and receive events
/*
	!!! DO NOT CHANGE THE ORDER !!!
*/
typedef struct IO_OPERATION_DATA {
	OVERLAPPED	overlapped;

	char		IoType; // IO operation type: such as READ or WRITE.
	UINT16		len;	// actual length of the data transmission.

	WSABUF		dataBuf;
	char		buffer[MAX_BUFFER_LEN];
} IO_OPERATION_DATA, * PIO_OPERATION_DATA;

class SocketContext  //To store and manage client related information
{
private:

	SOCKET                  m_Socket;  //accepted socket
	int                     m_Id;
	BOOL                    m_ProxySocket;
	char                    m_SockType[14];
	sockaddr_in             m_SockAddress;

	IO_OPERATION_DATA       m_IoRecv;
	IO_OPERATION_DATA       m_IoSend;

	/*
	class BuddySocketContextPtr
	{
	private:
		std::shared_ptr<SocketContext> _strong;
		std::weak_ptr<SocketContext> _weak;

	public:
		BuddySocketContextPtr();

		BuddySocketContextPtr(std::shared_ptr<SocketContext> buddy);

		std::shared_ptr<SocketContext> Get() const;

		BuddySocketContextPtr& Set(std::shared_ptr<SocketContext> sc);

		BuddySocketContextPtr& operator=(const std::shared_ptr<SocketContext>);
	};*/

	//BuddySocketContextPtr     m_pBuddy;
	std::weak_ptr<SocketContext>     m_pBuddy;

public:

	//BOOL s_CircularDestructor;

	BOOL Recv();

	BOOL Send(char* szBuffer, UINT16 length);

	BOOL Forward(UINT16 length);

	//void SetBuddySocketContext(const std::shared_ptr<SocketContext> sc);
	void SetBuddySocketContext(const std::shared_ptr<SocketContext>& sc);

	//std::shared_ptr<SocketContext> GetBuddySocketContext() const;
	std::shared_ptr<SocketContext> GetBuddySocketContext() const;

	char* GetSocketIpAddress();

	void SetSockType(const char* s);

	void GetSockType(char* s);

	void SocketAndIOCleanup();

	void SetProxySocket(BOOL value)
	{
		m_ProxySocket = value;
	}

	BOOL GetProxySocket()
	{
		return m_ProxySocket;
	}

	void SetId(int id)
	{
		m_Id = id;
	}

	int GetId()
	{
		return m_Id;
	}

	void SetSocket(SOCKET s)
	{
		m_Socket = s;
	}

	SOCKET GetSocket()
	{
		return m_Socket;
	}

	void SetSocketAddress(sockaddr_in s) 
	{
		m_SockAddress = s;
	}

	sockaddr_in GetSocketAddress()
	{
		return m_SockAddress;
	}

	void SetRevBuffer(char* RevBuffer)
	{
		strcpy(m_IoRecv.buffer, RevBuffer);
	}

	void GetRecvBuffer(char* RevBuffer)
	{
		strcpy(RevBuffer, m_IoRecv.buffer);
	}

	void ZeroRecvBuffer()
	{
		ZeroMemory(m_IoRecv.buffer, MAX_BUFFER_LEN);
	}

	void SetSendBuffer(char* SendBuffer)
	{
		strcpy(m_IoSend.buffer, SendBuffer);
	}

	void GetSendBuffer(char* SendBuffer)
	{
		strcpy(SendBuffer, m_IoSend.buffer);
	}

	void SetSendBufferLength(UINT16 length)
	{
		m_IoSend.len = length;
	}

	UINT16 GetSendBufferLength()
	{
		return m_IoSend.len;
	}

	void ZeroSendBuffer()
	{
		ZeroMemory(m_IoSend.buffer, MAX_BUFFER_LEN);
	}

	void ResetWSARecvIoData()
	{
		ZeroMemory(&m_IoRecv, sizeof(IO_OPERATION_DATA));
		m_IoRecv.dataBuf.buf = (char*)&m_IoRecv.buffer;
		m_IoRecv.dataBuf.len = MAX_BUFFER_LEN;
	}

	void ResetWSASendIoData()
	{
		ZeroMemory(&m_IoSend, sizeof(IO_OPERATION_DATA));
		m_IoSend.dataBuf.buf = (char*)&m_IoSend.buffer;
		m_IoSend.dataBuf.len = MAX_BUFFER_LEN;
	}

	//Constructor
	SocketContext();

	//destructor
	~SocketContext();
};

//Vector to store pointers of dynamically allocated ClientContext.
//map class can also be used.
//Link list can also be created.
//extern std::vector<std::shared_ptr<SocketContext>> g_ClientContext;
//extern std::unordered_map<int, std::shared_ptr<SocketContext>> g_SocketContextMap;

bool AssociateWithIOCP(std::shared_ptr<SocketContext> &pClientContext);
std::shared_ptr<SocketContext> InitRemoteConnection(std::shared_ptr<SocketContext> pClientContext);

#endif