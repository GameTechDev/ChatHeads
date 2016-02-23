/**************************************************************************************************
Collection of functions to get Windows system and process specific metrics

Based on: 
http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

References:
MEMORYSTATUSEX https://msdn.microsoft.com/en-us/library/windows/desktop/aa366770(v=vs.85).aspx
PROCESS_MEMORY_COUNTERS_EX https://msdn.microsoft.com/en-us/library/windows/desktop/ms684874(v=vs.85).aspx
PdhOpenQuery
***************************************************************************************************/

#include "windows.h"
#include "psapi.h" // PROCESS_MEMORY_COUNTERS_EX 
#include "pdh.h"// PDH_HQUERY, PDH_HCOUNTER 

#pragma comment(lib, "pdh.lib")
//--------------------------------- SYSTEM SPECIFICS ------------------------------------

static DWORDLONG TotalPhysicalMemory()
{
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);

	DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
	return totalPhysMem;
}


static DWORDLONG PhysicalMemoryUsed()
{
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);

	DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
	return physMemUsed;
}

// Virtual memory size (in bytes) = Size of swap file + RAM. 
static DWORDLONG TotalVirtualMemory()
{
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);

	// ullTotalPageFile = The current committed memory limit for the system or the current process, whichever is smaller, in bytes.
	DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile; 
	return totalVirtualMem;
}

// Virtual memory used in bytes
static DWORDLONG VirtualMemoryUsed()
{
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);
	DWORDLONG virtualMemoryUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;

	return virtualMemoryUsed;
}


static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuTotal;


void InitCPUQuery(){
	PdhOpenQuery(NULL, NULL, &cpuQuery);
	PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
	PdhCollectQueryData(cpuQuery);
}

// Percentage CPU used by system
// Call InitCPUQuery (just the once) prior to using this.
double CPUUsed(){
	PDH_FMT_COUNTERVALUE counterVal;


	PdhCollectQueryData(cpuQuery);
	PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
	return counterVal.doubleValue;
}

//--------------------------------- PROCESS SPECIFICS ------------------------------------	

static SIZE_T PhysicalMemoryUsedByProcess()
{
	PROCESS_MEMORY_COUNTERS_EX pmc;

	// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
	// and compile with -DPSAPI_VERSION=1
	GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS> (&pmc), sizeof(pmc));
	SIZE_T physMemUsedByMe = pmc.WorkingSetSize;

	return physMemUsedByMe;
}


static SIZE_T VirtualMemoryUsedByProcess()
{
	PROCESS_MEMORY_COUNTERS_EX pmc;

	// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
	// and compile with -DPSAPI_VERSION=1
	GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS> (&pmc), sizeof(pmc));
	SIZE_T virtualMemUsedByMe = pmc.PrivateUsage;

	return virtualMemUsedByMe;
}


static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;
static HANDLE self;


void InitCPUInfo(){
	SYSTEM_INFO sysInfo;
	FILETIME ftime, fsys, fuser;


	GetSystemInfo(&sysInfo);
	numProcessors = sysInfo.dwNumberOfProcessors;


	GetSystemTimeAsFileTime(&ftime);
	memcpy(&lastCPU, &ftime, sizeof(FILETIME));


	self = GetCurrentProcess();
	GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
	memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
	memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
}

// Percentage CPU used by process
// Call InitCPUInfo (just the once) prior to using this.
double CPUUsedByProcess(){
	FILETIME ftime, fsys, fuser;
	ULARGE_INTEGER now, sys, user;
	double percent;


	GetSystemTimeAsFileTime(&ftime);
	memcpy(&now, &ftime, sizeof(FILETIME));


	GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
	memcpy(&sys, &fsys, sizeof(FILETIME));
	memcpy(&user, &fuser, sizeof(FILETIME));
	percent = (sys.QuadPart - lastSysCPU.QuadPart) +
		(user.QuadPart - lastUserCPU.QuadPart);
	percent /= (now.QuadPart - lastCPU.QuadPart);
	percent /= numProcessors;
	lastCPU = now;
	lastUserCPU = user;
	lastSysCPU = sys;


	return percent * 100;
}


int NumProcessors()
{
	return numProcessors;
}