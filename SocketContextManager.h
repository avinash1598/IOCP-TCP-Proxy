#ifndef _SOCKET_CONTEXT_MANAGER_H_
#define _SOCKET_CONTEXT_MANAGER_H_

//Disable deprecation warnings
#pragma warning(disable: 4996)

//SocketContextManager is wrapper around SocketContext.
//Hence, adding SocketContext.h
#include "SocketContext.h"

extern CRITICAL_SECTION g_numSockCntxt; //Need to protect the socket context count
extern CRITICAL_SECTION g_csSockCntxtMap; //Critical section for Socket context map
extern CRITICAL_SECTION g_csJunkSockCntxt; //Critical section for Junk Socket context

class  SocketContextManager
{
private:

	static std::unordered_map<int, std::shared_ptr<SocketContext>> s_SocketContextMap;
	static std::unordered_map<int, long> s_JunkSocketContextMap;
	static int s_SOCK_COUNT;

public: 

	static void Initialize();

	static void CleanUp();

	static void AddToMap(int SocketId, const std::shared_ptr<SocketContext>& pSocketContext);

	static std::shared_ptr<SocketContext>& GetFromMap(int SocketId);

	static void RemoveFromMapAndCleanUpMemory(int SocketId);

	static void CleanMap();

	static void PutIntoJunkSocketContextMap(const int SocketId);

	static void CleanUpJunkSocketContextMap(bool EraseAll);

	static int GetSocketId();

	static void ResetSocketCounter();
};

long CurrentTimeInMillis();

#endif