//
// libtop.cpp
//
// Portions Copyright (c) 1999-2007 Apple Inc.  All Rights Reserved.  This file
// contains Original Code and/or Modifications of Original Code as defined in and
// that are subject to the Apple Public Source License Version 2.0 (the 'License').
// You may not use this file except in compliance with the License.  Please obtain
// a copy of the License at http://www.opensource.apple.com/apsl/ and read it
// before using this file.
//
// The Original Code and all software distributed under the License are distributed
// on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION,
// ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET
// ENJOYMENT OR NON-INFRINGEMENT.  Please see the License for the specific language
// governing rights and limitations under the License."
//
// Written by Hossein Afshari: hossein.afshari@nexthink.com, Lausanne in
// December 2017

#include "libtop.h"

#include <thread>

#include <mach/mach_host.h>
#include <mach/vm_statistics.h>
#include <mach/host_priv.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/processor_set.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <sys/sysctl.h>

#include <libproc.h>

using namespace nxt::top;

kern_return_t nxt::top::PhysicalMemory(int64_t &physical_memory)
{
    int mib[2];
    size_t length = sizeof(int64_t);
    
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    
    return sysctl(mib, 2, &physical_memory, &length, NULL, 0);
}

kern_return_t nxt::top::SwapStat(xsw_usage& usage)
{
    int mib[2];
    size_t length = sizeof(xsw_usage);
    
    mib[0] = CTL_VM;
    mib[1] = VM_SWAPUSAGE;
    
    return sysctl(mib, 2, &usage, &length, NULL, 0);
}

kern_return_t nxt::top::DeltaSampleCpuLoad(CpuSample &sample, std::chrono::milliseconds msec)
{
	kern_return_t kr;
	CpuSample sample_one, sample_two;

	kr = SampleCpuLoad(sample_one);
	if (kr != KERN_SUCCESS)
    {
	   return kr;
    }

    std::this_thread::sleep_for(msec);

	kr = SampleCpuLoad(sample_two);
	if (kr != KERN_SUCCESS)
    {
	   return kr;
    }

    sample.totalIdleTime = sample_two.totalIdleTime - sample_one.totalIdleTime;
    sample.totalUserTime = sample_two.totalUserTime - sample_one.totalUserTime;
    sample.totalSystemTime = sample_two.totalSystemTime - sample_one.totalSystemTime;

	return kr;
}

kern_return_t nxt::top::SampleCpuLoad(CpuSample &sample)
{
    kern_return_t kr;
    mach_msg_type_number_t count;
    host_cpu_load_info_data_t r_load;

    count = HOST_CPU_LOAD_INFO_COUNT;
    kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (int *)&r_load, &count);
	if (kr != KERN_SUCCESS)
	{
		return kr;
	}

    sample.totalSystemTime = static_cast<uint64_t>(r_load.cpu_ticks[CPU_STATE_SYSTEM]);
    sample.totalUserTime = static_cast<uint64_t>(r_load.cpu_ticks[CPU_STATE_USER]) + r_load.cpu_ticks[CPU_STATE_NICE];
    sample.totalIdleTime = static_cast<uint64_t>(r_load.cpu_ticks[CPU_STATE_IDLE]);

	return kr;
}

kern_return_t nxt::top::SampleMemoryUsage(MemorySample &sample)
{
    kern_return_t kr;
    static int64_t physical_memory = 0;

    if ( physical_memory == 0 )
    {
        kr = PhysicalMemory(physical_memory);
        if (kr != KERN_SUCCESS)
        {
            return kr;
        }
    }

    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = sizeof(vm_stat) / sizeof(natural_t);

    kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)&vm_stat, &count);
	if (kr != KERN_SUCCESS)
	{
		return kr;
	}

    vm_size_t pagesize = vm_kernel_page_size;
    uint64_t total_used_count;

    total_used_count = static_cast<uint64_t>(vm_stat.wire_count) + vm_stat.internal_page_count - vm_stat.purgeable_count + vm_stat.compressor_page_count;
    sample.memoryUsed = total_used_count * pagesize;

    sample.memoryFree = static_cast<uint64_t>(physical_memory) - sample.memoryUsed;

    sample.faultCount = vm_stat.faults;

    xsw_usage swap_status;
    kr  = SwapStat(swap_status);
    if (kr != KERN_SUCCESS)
    {
        return kr;
    }

    sample.memoryPagedout = swap_status.xsu_used;
	sample.memoryCommitted = static_cast<uint64_t>(physical_memory) + swap_status.xsu_used - sample.memoryFree;
	sample.memoryLimit = static_cast<uint64_t>(physical_memory) + swap_status.xsu_total;

	return kr;
}

unsigned nxt::top::GetNumberOfCpu()
{
    return std::thread::hardware_concurrency();
}

kern_return_t nxt::top::SampleProcessStatistics(int pid, ProcessStatisticsSample &sample)
{
    using namespace std::chrono;
    
	kern_return_t kr = KERN_RETURN_MAX;

    struct proc_taskinfo prc;
    constexpr auto sec_ns = duration_cast<nanoseconds>(1s).count();
	constexpr auto usec_ns =  duration_cast<nanoseconds>(1us).count();
    
    if ( sizeof(prc) == proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &prc, sizeof(prc)) )
    {
        sample.cpu.threadCount = prc.pti_threadnum;

        auto total = prc.pti_total_system + prc.pti_total_user;
        sample.cpu.totalTime.tv_sec = total / sec_ns;
        sample.cpu.totalTime.tv_usec = (total % sec_ns) / usec_ns;

        // prc.pti_resident_size is equal to the RealMemory column of Activity Monitor
        sample.memory = 0;

        kr = KERN_SUCCESS;
    }

	return kr;
}
