#ifndef __VTUNE_SCOPED_TASK_H__
#define __VTUNE_SCOPED_TASK_H__

// Simple wrapper to get VTune task api markers
//#define ENABLE_VTUNE_PROFILING // uncomment to enable marker code

#include "ittnotify.h"

class VTuneScopedTask
{
public:
	VTuneScopedTask(__itt_domain* pDomain, const char* szTaskName)
		: m_pDomain(pDomain)
	{
		__itt_string_handle* pTaskName = __itt_string_handle_createA(szTaskName);
		__itt_task_begin(m_pDomain, __itt_null, __itt_null, pTaskName);
	}
	~VTuneScopedTask(void)
	{
		__itt_task_end(m_pDomain);
	}

private:
	__itt_domain* m_pDomain;
};

#ifdef ENABLE_VTUNE_PROFILING
#define VTUNE_TASK( Domain, TaskName )			VTuneScopedTask _vtune_scoped_task_(Domain, TaskName)
#else
#define VTUNE_TASK( Domain, TaskName )
#endif

//#define VTUNE_TASK_BEGIN( Domain, TaskName )	__itt_task_begin(Domain, __itt_null, __itt_null, TaskName)
//#define VTUNE_TASK_END( Domain )				__itt_task_end( Domain ) 

#endif // __VTUNE_SCOPED_TASK_H__