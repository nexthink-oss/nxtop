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
#include <unistd.h>

#include <mach/mach_host.h>
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
    using namespace std::chrono;

    kern_return_t kr;
    mach_msg_type_number_t count;
    host_cpu_load_info_data_t r_load;

    count = HOST_CPU_LOAD_INFO_COUNT;
    kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (int *)&r_load, &count);
	if (kr != KERN_SUCCESS)
	{
		return kr;
	}

    auto ticks_per_second = sysconf(_SC_CLK_TCK);
    auto constexpr sec_milisec = milliseconds(1s).count();
    sample.totalSystemTime = milliseconds(r_load.cpu_ticks[CPU_STATE_SYSTEM] * sec_milisec / ticks_per_second );
    sample.totalUserTime = milliseconds(r_load.cpu_ticks[CPU_STATE_USER] * sec_milisec / ticks_per_second ) + milliseconds(r_load.cpu_ticks[CPU_STATE_NICE] * sec_milisec / ticks_per_second);
    sample.totalIdleTime = milliseconds(r_load.cpu_ticks[CPU_STATE_IDLE] * sec_milisec / ticks_per_second );

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

    vm_size_t pagesize = vm_page_size;
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
    int mib[2];
    int numCPU = 0;
    std::size_t len = sizeof(numCPU);

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

    /* get the number of CPUs from the system */
    auto err = sysctl(mib, 2, &numCPU, &len, NULL, 0);

    if (numCPU < 1 || err != KERN_SUCCESS)
    {
        mib[1] = HW_NCPU;

        err = sysctl(mib, 2, &numCPU, &len, NULL, 0);

        if (numCPU < 1 || err != KERN_SUCCESS)
        {
            numCPU = 1;
        }
    }

    return numCPU;
}

kern_return_t nxt::top::SampleProcessStatistics(int pid, ProcessStatisticsSample &sample)
{
    using namespace std::chrono;

	kern_return_t kr = KERN_RETURN_MAX;

    struct proc_taskinfo prc;
    if ( sizeof(prc) == proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &prc, sizeof(prc)) )
    {
        sample.cpu.threadCount = prc.pti_threadnum;

        auto total = prc.pti_total_system + prc.pti_total_user;
        sample.cpu.totalTime = nanoseconds(total);

        // Report the kernel_task memory as it is done in XNU kernel (Refer to osfmk/kern/task.c)
        if ( pid == 0 )
        {
            vm_statistics64_data_t vm_stat;
            mach_msg_type_number_t count = sizeof(vm_stat) / sizeof(natural_t);

            kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)&vm_stat, &count);
            if (kr == KERN_SUCCESS )
            {
                sample.memory = prc.pti_resident_size - static_cast<uint64_t>(vm_stat.compressor_page_count) * vm_page_size;
            }
        }
        else
        {
            // Refer to osfmk/kern/task.c in XNU kernel source code for the definition of phys_footprint.
            // The value reported will be equal or higher than the internal memory reported by the Activity monitor.
            // Nevertheless the value reported should have the same magnitude and be close to that of the Memory column displayed in Activity monitor.
            rusage_info_current rui;
            kr = proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, reinterpret_cast<rusage_info_t *>(&rui));
            if ( kr == KERN_SUCCESS )
            {
                sample.memory = rui.ri_phys_footprint;
            }
        }
    }

	return kr;
}
