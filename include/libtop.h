//
// libtop.h
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

#pragma once

#include <chrono>

#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <sys/sysctl.h>

class libTop
{
public:
    typedef struct
    {
        uint64_t totalSystemTime;
        uint64_t totalUserTime;
        uint64_t totalIdleTime;
    } CPU_SAMPLE;

    typedef struct
    {
        uint64_t memoryFree;
        uint64_t memoryUsed;
        uint64_t memoryPagedout;
        uint64_t faultCount;
        uint64_t memoryLimit;
        uint64_t memoryCommitted;
    } MEMORY_SAMPLE;

    typedef struct
    {
        struct timeval totalTime;
        unsigned int threadCount;
    } PROCESS_CPU_SAMPLE;

    kern_return_t DeltaSampleCpuLoad(CPU_SAMPLE &sample, std::chrono::milliseconds msec);
    kern_return_t SampleCpuLoad(CPU_SAMPLE &sample);
    kern_return_t SampleMemoryUsage(MEMORY_SAMPLE &sample);
	kern_return_t PhysicalMemory(int64_t &);
    kern_return_t SwapStat(xsw_usage &);
    kern_return_t SampleProcessCpuLoad(int pid, PROCESS_CPU_SAMPLE &sample);
    static unsigned GetNumberOfCpu();
	kern_return_t TaskForPid(int pid, task_t &task);
};

