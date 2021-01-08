#include <winsock2.h>
#include <vector>

#include "MdProxyService.h"

/*
No need of synchronization as long as this function works with local variables
*/
BOOL ForwardData(
	SocketContext* pClientContext,
	int nThreadNo,
	DWORD dwBytesTransfered
)
{
	BOOL status = TRUE;

	WSABUF* p_wbuf;
	OVERLAPPED* p_ol;

	int nBytesSent = 0;
	DWORD dwBytes = 0, dwFlags = 0;
	char szBuffer[MAX_BUFFER_LEN];
	SocketContext* pFwd_SocketContext;

	//Display the message we recevied
	pClientContext->GetBuffer(szBuffer);

	WriteToConsole("\nThread %d: The following message was received: %s", nThreadNo, szBuffer);

	//Forward message to fwd socket
	pFwd_SocketContext = pClientContext->GetFwdScoketContext();

	if (pFwd_SocketContext == NULL)
	{
		WriteToConsole("\nThread %d: No Fwd Socket associated with this socket context.", nThreadNo);

		//Let's not work with this client
		RemoveFromClientListAndFreeMemory(pClientContext);
		status = FALSE;
		goto Exit;
	}

	p_wbuf = pFwd_SocketContext->GetWSABUFPtr();
	p_ol = pFwd_SocketContext->GetOVERLAPPEDPtr();

	pFwd_SocketContext->SetOpCode(OP_WRITE);

	pFwd_SocketContext->SetTotalBytes(dwBytesTransfered);
	pFwd_SocketContext->SetSentBytes(0);

	p_wbuf->len = dwBytesTransfered;
	pFwd_SocketContext->SetBuffer(szBuffer);

	dwFlags = 0;

	nBytesSent = WSASend(pFwd_SocketContext->GetSocket(), p_wbuf, 1,
		&dwBytes, dwFlags, p_ol, NULL);

	if ((SOCKET_ERROR == nBytesSent) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		WriteToConsole("\nThread %d: Error occurred while executing WSASend().", nThreadNo);

		//Let's not work with this client
		RemoveFromClientListAndFreeMemory(pFwd_SocketContext);
		status = FALSE;
		goto Exit;
	}

Exit:
	return status;
}

BOOL PostWSARecv(
	SocketContext* pClientContext
)
{
}

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
	int nThreadNo = (int)lpParam;

	void* lpContext = NULL;
	OVERLAPPED* pOverlapped = NULL;
	SocketContext* pClientContext = NULL;
	DWORD dwBytesTransfered = 0;
	int nBytesRecv = 0;

	//Worker thread will be around to process requests, until a Shutdown event is not Signaled.
	while (WAIT_OBJECT_0 != WaitForSingleObject(g_hShutdownEvent, 0))
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			g_hIOCompletionPort,
			&dwBytesTransfered,
			(LPDWORD)&lpContext,
			&pOverlapped,
			INFINITE);

		if (NULL == lpContext)
		{
			//We are shutting down
			break;
		}

		//Get the client context
		pClientContext = (SocketContext*)lpContext;

		if ((FALSE == bReturn) || ((TRUE == bReturn) && (0 == dwBytesTransfered)))
		{
			//Client connection gone, remove it.
			RemoveFromClientListAndFreeMemory(pClientContext);
			continue;
		}

		if (TRUE == pClientContext->GetProxySocket())
		{
			WriteToConsole("\nPROXY SOCKET: READ or WRITE operation needs to be done.");
		}
		else
		{
			WriteToConsole("\nLOCAL SOCKET: READ or WRITE operation needs to be done.");

			switch (pClientContext->GetOpCode())
			{
			case OP_READ:

				WriteToConsole("\nReceived data from local application. Redirecting it to remote socket...");

				if (FALSE == ForwardData(pClientContext, nThreadNo, dwBytesTransfered))
				{
					//Do nothing. Exit this iteration and move to next.
					continue;
				}


			}

		}


		/*
		switch (pClientContext->GetOpCode())
		{
		case OP_READ:

			p_wbuf = pClientContext->GetWSABUFPtr();
			p_ol = pClientContext->GetOVERLAPPEDPtr();

			pClientContext->IncrSentBytes(dwBytesTransfered);

			//Write operation was finished, see if all the data was sent.
			//Else post another write.
			if (pClientContext->GetSentBytes() < pClientContext->GetTotalBytes())
			{
				pClientContext->SetOpCode(OP_READ);

				p_wbuf->buf += pClientContext->GetSentBytes(); //why this??
				p_wbuf->len = pClientContext->GetTotalBytes() - pClientContext->GetSentBytes();

				dwFlags = 0;

				//Overlapped send
				nBytesSent = WSASend(pClientContext->GetSocket(), p_wbuf, 1,
					&dwBytes, dwFlags, p_ol, NULL);

				if ((SOCKET_ERROR == nBytesSent) && (WSA_IO_PENDING != WSAGetLastError()))
				{
					//Let's not work with this client
					RemoveFromClientListAndFreeMemory(pClientContext);
				}
			}
			else
			{
				if (TRUE == pClientContext->GetProxySocket())
				{
					WriteToConsole("\nPROXY SOCKET: WSARecv.");
				}
				else
				{
					WriteToConsole("\nLOCAL SOCKET: WSARecv.");
				}

				//Once the data is successfully received, we will print it.
				pClientContext->SetOpCode(OP_WRITE);
				pClientContext->ResetWSABUF();

				dwFlags = 0;

				//Get the data.
				nBytesRecv = WSARecv(pClientContext->GetSocket(), p_wbuf, 1,
					&dwBytes, &dwFlags, p_ol, NULL);

				if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
				{
					WriteToConsole("\nThread %d: Error occurred while executing WSARecv().", nThreadNo);

					//Let's not work with this client
					RemoveFromClientListAndFreeMemory(pClientContext);
				}
			}

			break;

		case OP_WRITE:

			

			break;

		default:
			//We should never be reaching here, under normal circumstances.
			break;
		} // switch

		*/

	} // while

	return 0;
}
