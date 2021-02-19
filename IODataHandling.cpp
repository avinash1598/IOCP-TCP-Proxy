#include <winsock2.h>
#include <unordered_map>
#include <memory>
#include <thread>

#include "MdProxyService.h"

using namespace std;
using namespace CPlusPlusLogging;


DWORD WINAPI WorkerThread(LPVOID lpParam)
{
	int nThreadNo = (int)lpParam;

	void* lpContext = nullptr;
	OVERLAPPED* pOverlapped = nullptr;
	DWORD dwBytesTransfered = 0;
	char sock_type[14];
	PIO_OPERATION_DATA pIoData;
	std::shared_ptr<SocketContext> pClientContext = nullptr;

	//Worker thread will be around to process requests, 
	//until a Shutdown event is not Signaled.
	while (WAIT_OBJECT_0 != WaitForSingleObject(g_hShutdownEvent, 0))
	{
		BOOL status = TRUE;
		BOOL bReturn = GetQueuedCompletionStatus(
			g_hIOCompletionPort,
			&dwBytesTransfered,
			(LPDWORD)&lpContext,
			&pOverlapped,
			INFINITE);

		if (NULL == lpContext)
		{
			//We are shutting down
			status = FALSE;
			break;
		}
		
		LOG_DEBUG("Thread %d: Execution inside worker thread.", nThreadNo);

		pClientContext = *static_cast<std::shared_ptr<SocketContext>*>(lpContext);
		pClientContext->GetSockType(sock_type);

		//Check if socket connection is closed
		if ((FALSE == bReturn) || ((TRUE == bReturn) && (0 == dwBytesTransfered)))
		{
			LOG_DEBUG("Thread %d %s %d: connection gone.", 
				nThreadNo, sock_type, pClientContext->GetId());
			status = FALSE;
			goto Exit;
		}

		pIoData = (PIO_OPERATION_DATA)pOverlapped;

		try
		{
			switch (pIoData->IoType)
			{
			case OP_READ:

				LOG_DEBUG("Thread %d %s %ld: Received %ld bytes.", nThreadNo, 
					sock_type, pClientContext->GetId(), dwBytesTransfered);

				if (FALSE == pClientContext->Forward(dwBytesTransfered))
				{
					LOG_ERROR("Thread %d %s %ld: Error occured while forwading data.",
						nThreadNo, sock_type, pClientContext->GetId());
					status = FALSE;
					goto Exit;
				}

				if (FALSE == pClientContext->Recv())
				{
					LOG_ERROR("Thread %d %s %d: Error occured while receiving data.", 
						nThreadNo, sock_type, pClientContext->GetId());
					status = FALSE;
					goto Exit;
				}

				break;

			case OP_WRITE:

				LOG_DEBUG("Thread %d %s %ld: Sent %ld bytes.", nThreadNo, 
					sock_type, pClientContext->GetId(), dwBytesTransfered);

				break;

			default:
				break;
			}
		}
		catch (const char* message)
		{
			LOG_ERROR("Thread %d %s: Exception occured: %s.", message);
		}

	Exit:
		if (FALSE == status)
		{
			LOG_DEBUG("Thread %d %s %ld: "
				"Processing of I/O Completion port data failed at some point.", 
				nThreadNo, sock_type, pClientContext->GetId());
			
			//Clean up this socket and its buddy socket
			pClientContext->SocketAndIOCleanup();
			pClientContext->GetBuddySocketContext()->SocketAndIOCleanup();
			SocketContextManager::PutIntoJunkSocketContextMap(
				pClientContext->GetId());
			SocketContextManager::PutIntoJunkSocketContextMap(
				pClientContext->GetBuddySocketContext()->GetId());

			/*
				if (buddysocket not in junk)
					post completion status
			*/
			//PostQueuedCompletionStatus(g_hIOCompletionPort, 0, 
			//	(DWORD)&SocketContextManager::GetFromMap(pClientContext->GetBuddySocketContext()->GetId()), 
			//	NULL);

			//LOG_DEBUG("Thread %d: Reference count for socket context: %ld",
			//	nThreadNo, pClientContext.use_count());
		}
	} // while

	return 0;
}
