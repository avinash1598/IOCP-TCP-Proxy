#include <stdio.h>
#include <windows.h>

#include "Logger.h"
#include "KernelCommunicator.h"

using namespace std;
using namespace CPlusPlusLogging;

BOOL UpdateKernelWithUserAppProcessId() 
{
	BOOL status = TRUE;
	DWORD processId;
	PUSERAPP_IOCTL_IN_BUF pUserAppData;

	HANDLE device;

	processId = GetCurrentProcessId();
	LOG_INFO("Current process ID: %ld", processId);

	pUserAppData = (PUSERAPP_IOCTL_IN_BUF)malloc(sizeof(USERAPP_IOCTL_IN_BUF));
	memset(pUserAppData, 0, sizeof(USERAPP_IOCTL_IN_BUF));
	pUserAppData->UserModeAppProcessId = (LONG)processId;
	
	device = CreateFileW(WFP_DEVICE_FILE_NAME,
						 GENERIC_ALL,
						 0,
						 0,
						 OPEN_EXISTING,
						 FILE_ATTRIBUTE_SYSTEM, 
						 0);

	if (device == INVALID_HANDLE_VALUE)
	{
		LOG_ERROR("Could not open device: 0x%x. Invalid file device handle.", GetLastError());
		status = FALSE;
		goto Exit;
	}

	status = DeviceIoControl(device,
							 IOCTL_WFP_SET_DEVICE_CONTEXT_DATA,
							 pUserAppData,
							 sizeof(USERAPP_IOCTL_IN_BUF),
							 NULL,
							 0,
							 NULL,
							 (LPOVERLAPPED)NULL);

	if (FALSE == status)
	{
		LOG_ERROR("Error sending device IO control code: 0x%x", GetLastError());
		status = FALSE;
		goto Exit;
	}

Exit:
	free(pUserAppData);
	
	if (device)
		CloseHandle(device);

	return status;
}
