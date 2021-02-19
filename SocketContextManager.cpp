#include <winsock2.h>
#include <unordered_map>
#include <memory>
#include <Windows.h>

//#include "SocketContext.h"
#include "MdProxyService.h"
//#include "SocketContextManager.h"

using namespace std;
using namespace CPlusPlusLogging;

CRITICAL_SECTION g_numSockCntxt;
CRITICAL_SECTION g_csSockCntxtMap;
CRITICAL_SECTION g_csJunkSockCntxt;

std::unordered_map<int, std::shared_ptr<SocketContext>> SocketContextManager::s_SocketContextMap;
std::unordered_map<int, long> SocketContextManager::s_JunkSocketContextMap;
int SocketContextManager::s_SOCK_COUNT;


void SocketContextManager::Initialize()
{
	//Initialize the Socket Counter critical section
	InitializeCriticalSection(&g_numSockCntxt);

	//Initialize the Socket context map Critical Section
	InitializeCriticalSection(&g_csSockCntxtMap);

	//Initialize the Junk Socket context Critical Section
	InitializeCriticalSection(&g_csJunkSockCntxt);

	//Initialize Socket Counter
	ResetSocketCounter();
}

void SocketContextManager::CleanUp()
{
	//Clean up junk socket context list
	CleanUpJunkSocketContextMap(true);

	//Before deleting critical section clean up map
	CleanMap();

	//Delete the socket counter Critical Section.
	DeleteCriticalSection(&g_numSockCntxt);

	//Delete the Socket Context map Critical Section.
	DeleteCriticalSection(&g_csSockCntxtMap);

	//Delete the Junk Socket Context Critical Section.
	DeleteCriticalSection(&g_csJunkSockCntxt);
}

void SocketContextManager::AddToMap(int SocketId, const std::shared_ptr<SocketContext>& pSocketContext)
{
	EnterCriticalSection(&g_csSockCntxtMap);

	s_SocketContextMap[SocketId] = pSocketContext;

	LeaveCriticalSection(&g_csSockCntxtMap);
}

std::shared_ptr<SocketContext>& SocketContextManager::GetFromMap(int SocketId)
{
	return s_SocketContextMap[SocketId];
}

void SocketContextManager::RemoveFromMapAndCleanUpMemory(int SocketId)
{
	EnterCriticalSection(&g_csSockCntxtMap);

	//i/o will be cancelled and socket will be closed by destructor.
	s_SocketContextMap[SocketId] = nullptr;
	s_SocketContextMap.erase(SocketId);

	LeaveCriticalSection(&g_csSockCntxtMap);
}

void SocketContextManager::CleanMap()
{
	EnterCriticalSection(&g_csSockCntxtMap);
	
	for (auto IterSocketContext = s_SocketContextMap.begin(); IterSocketContext != s_SocketContextMap.end(); IterSocketContext++)
	{
		//i/o will be cancelled and socket will be closed by destructor.
		IterSocketContext->second = nullptr;
	}

	s_SocketContextMap.clear();

	LeaveCriticalSection(&g_csSockCntxtMap);
}

void SocketContextManager::PutIntoJunkSocketContextMap(const int SocketId)
{
	EnterCriticalSection(&g_csJunkSockCntxt);

	s_JunkSocketContextMap[SocketId] = CurrentTimeInMillis();

	LOG_DEBUG("Socket Id: %d moved to junk list", SocketId);

	LeaveCriticalSection(&g_csJunkSockCntxt);
}

void SocketContextManager::CleanUpJunkSocketContextMap(bool EraseAll)
{
	EnterCriticalSection(&g_csJunkSockCntxt);

	if (EraseAll)
	{
		s_JunkSocketContextMap.clear();
	} 
	else 
	{
		long curr_time_s = CurrentTimeInMillis();

		for (auto it = s_JunkSocketContextMap.begin(); it != s_JunkSocketContextMap.end(); )
		{
			//Clean up sockets which are present in junk list 
			//for more than 60s.
			//LOG_DEBUG("%ld - %ld = %ld", curr_time_s, it->second, (curr_time_s - it->second));
			if (THREAD_SLEEP_INTERVAL < (curr_time_s - it->second))
			{
				LOG_DEBUG("Removing socket id: %d from junk socket list.", it->first);
				RemoveFromMapAndCleanUpMemory(it->first);
				s_JunkSocketContextMap.erase(it++);
			}
			else {
				++it;
			}
		}
	}

	LOG_DEBUG("Junk socket context map size after cleanup: %d", 
		s_JunkSocketContextMap.size());

	LeaveCriticalSection(&g_csJunkSockCntxt);
}

int SocketContextManager::GetSocketId()
{
	EnterCriticalSection(&g_numSockCntxt);

	s_SOCK_COUNT++;

	LeaveCriticalSection(&g_numSockCntxt);

	return s_SOCK_COUNT;
}

void SocketContextManager::ResetSocketCounter()
{
	EnterCriticalSection(&g_numSockCntxt);

	s_SOCK_COUNT = 0;

	LeaveCriticalSection(&g_numSockCntxt);
}

long CurrentTimeInMillis()
{
	SYSTEMTIME time;
	GetSystemTime(&time);
	LONG time_s = (time.wSecond * 1000) + time.wMilliseconds;

	return time_s;
}