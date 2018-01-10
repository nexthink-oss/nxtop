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
#include <sys/sysctl.h>

kern_return_t libTop::DeltaSampleCpuLoad(CPU_SAMPLE &sample, std::chrono::milliseconds msec)
{
	kern_return_t kr;
	CPU_SAMPLE sample_one, sample_two;

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

kern_return_t libTop::SampleCpuLoad(CPU_SAMPLE &sample)
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

kern_return_t libTop::SampleMemoryUsage(MEMORY_SAMPLE &sample)
{
	static uint64_t physical_memory = PhysicalMemory();
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = sizeof(vm_stat) / sizeof(natural_t);

    kern_return_t kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)&vm_stat, &count);
	if (kr != KERN_SUCCESS)
	{
		return kr;
	}

    vm_size_t pagesize = vm_kernel_page_size;
    uint64_t total_used_count;

    sample.memoryFree = static_cast<uint64_t>(vm_stat.free_count) * pagesize;

    total_used_count = static_cast<uint64_t>(vm_stat.wire_count + vm_stat.internal_page_count - vm_stat.purgeable_count + vm_stat.compressor_page_count);
    sample.memoryUsed = total_used_count * pagesize;

    sample.memoryPagedout = SwapUsed();

    sample.faultCount = vm_stat.faults;
	sample.memoryLimit = physical_memory + sample.memoryPagedout;

	return kr;
}

uint64_t libTop::PhysicalMemory()
{
	int mib[2];
	int64_t physical_memory;
	size_t length = sizeof(int64_t);

	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;

	sysctl(mib, 2, &physical_memory, &length, NULL, 0);

	return static_cast<uint64_t>(physical_memory);
}

uint64_t libTop::SwapUsed()
{
    int mib[2];
    xsw_usage usage;
    size_t length = sizeof(xsw_usage);

    mib[0] = CTL_VM;
    mib[1] = VM_SWAPUSAGE;

    sysctl(mib, 2, &usage, &length, NULL, 0);

    return usage.xsu_used;
}

