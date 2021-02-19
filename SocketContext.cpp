#include <winsock2.h>
#include <unordered_map>
//#include <vector>
#include <memory>

#include "Logger.h"
#include "SocketContext.h"

using namespace std;
using namespace CPlusPlusLogging;

SocketContext::SocketContext()
{
	m_Socket = SOCKET_ERROR;

	ZeroMemory(&m_IoRecv, sizeof(IO_OPERATION_DATA));
	m_IoRecv.dataBuf.buf = (char*)&m_IoRecv.buffer;
	m_IoRecv.dataBuf.len = MAX_BUFFER_LEN;
	m_IoRecv.IoType = OP_READ;

	ZeroMemory(&m_IoSend, sizeof(IO_OPERATION_DATA));
	m_IoSend.dataBuf.buf = (char*)&m_IoSend.buffer;
	m_IoSend.dataBuf.len = MAX_BUFFER_LEN;
	m_IoSend.IoType = OP_WRITE;

	ZeroMemory(m_SockType, sizeof(m_SockType));
	m_ProxySocket = FALSE;
	m_pBuddy.reset();
}

BOOL SocketContext::Recv()
{
	BOOL status = TRUE;
	DWORD dwFlags = 0;
	int nBytesRecv = 0;

	ZeroMemory(&m_IoRecv, sizeof(IO_OPERATION_DATA));

	m_IoRecv.IoType = OP_READ;
	m_IoRecv.dataBuf.buf = (char*)&m_IoRecv.buffer;
	m_IoRecv.dataBuf.len = MAX_BUFFER_LEN;

	dwFlags = 0;

	nBytesRecv = WSARecv(m_Socket, &m_IoRecv.dataBuf, 1, NULL, &dwFlags, &m_IoRecv.overlapped, NULL);

	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		status = FALSE;
	}

	return status;
}

BOOL SocketContext::Send(char* szBuffer, UINT16 length)
{
	LOG_DEBUG("%s %ld: Sending %ld bytes.", m_SockType, m_Id, length);

	BOOL status = TRUE;
	int nBytesSent = 0;
	DWORD dwFlags = 0;

	ZeroMemory(&m_IoSend, sizeof(IO_OPERATION_DATA));

	m_IoSend.IoType = OP_WRITE;
	strcpy(m_IoSend.buffer, szBuffer);
	m_IoSend.len = length;

	m_IoSend.dataBuf.buf = (char*)&m_IoSend.buffer;
	m_IoSend.dataBuf.len = m_IoSend.len;

	nBytesSent = WSASend(m_Socket, &m_IoSend.dataBuf, 1, NULL, dwFlags, &m_IoSend.overlapped, NULL);

	if ((SOCKET_ERROR == nBytesSent) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		status = FALSE;
	}

	return status;
}

/*
	Forward data from recv buffer of client socket to
	send buffer of fwd socket and send it to destination from
	fwd socket.
*/
BOOL SocketContext::Forward(UINT16 length)
{
	LOG_DEBUG("%s %ld: Forwarding %ld bytes to socket id %d.", 
		m_SockType, m_Id, length, m_pBuddy.lock()->GetId());

	BOOL status = TRUE;
	using wt = std::weak_ptr<SocketContext>;

	if (!m_pBuddy.owner_before(wt{}) 
		&& !wt{}.owner_before(m_pBuddy) || m_pBuddy.expired())
	{
		status = FALSE;
		goto Exit;
	}
	
	status = m_pBuddy.lock()->Send(m_IoRecv.buffer, length);

Exit:
	return status;
}

void SocketContext::SetSockType(const char* s) 
{
	strcpy(m_SockType, s);
}

void SocketContext::GetSockType(char* s)
{
	strcpy(s, m_SockType);
}

void SocketContext::SocketAndIOCleanup()
{
	LOG_DEBUG("%s %ld: Closing socket and cancelling all I/O", 
		m_SockType, m_Id);

	//Cancel all Recv IO operations
	CancelIoEx((HANDLE)m_Socket, &m_IoRecv.overlapped);
	SleepEx(0, TRUE); // the completion will be called here

	//Cancel all Send IO operation
	CancelIoEx((HANDLE)m_Socket, &m_IoSend.overlapped);
	SleepEx(0, TRUE); // the completion will be called here

	closesocket(m_Socket);
}

SocketContext::~SocketContext()
{
	LOG_DEBUG("%s %ld: SocketContext::~SocketContext(): "
		"Cleaning up socket context.", m_SockType, m_Id);

	SocketAndIOCleanup();
}

std::shared_ptr<SocketContext> SocketContext::GetBuddySocketContext() const
{
	//return m_pBuddy.Get();
	return m_pBuddy.lock();
}


void SocketContext::SetBuddySocketContext(const std::shared_ptr<SocketContext>& sc)
{
	m_pBuddy = sc;
}

/*
SocketContext::BuddySocketContextPtr::BuddySocketContextPtr()
{
	_strong = nullptr;
	_weak.reset();
}

SocketContext::BuddySocketContextPtr::BuddySocketContextPtr(const std::shared_ptr<SocketContext> sc)
{
	_strong = nullptr;
	_weak.reset();

	Set(sc);
}

SocketContext::BuddySocketContextPtr& SocketContext::BuddySocketContextPtr::operator=(const std::shared_ptr<SocketContext> sc) {
	return Set(sc);
}

std::shared_ptr<SocketContext> SocketContext::BuddySocketContextPtr::Get() const
{
	if (nullptr != _strong)
	{
		return _strong;
	}
	else 
	{
		return _weak.lock();
	}
}

SocketContext::BuddySocketContextPtr& SocketContext::BuddySocketContextPtr::Set(std::shared_ptr<SocketContext> sc)
{
	if (nullptr == sc->GetBuddySocketContext())
	{
		_strong = sc;
		_weak.reset();
	}
	else
	{
		_strong = nullptr;
		_weak = sc;
	}

	return *this;
}*/

char* SocketContext::GetSocketIpAddress()
{
	return inet_ntoa(m_SockAddress.sin_addr);
}