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

struct xsw_usage;

namespace nxt
{
namespace top
{

    struct CpuSample
    {
        uint64_t totalSystemTime;
        uint64_t totalUserTime;
        uint64_t totalIdleTime;
    };

    struct MemorySample
    {
        uint64_t memoryFree;
        uint64_t memoryUsed;
        uint64_t memoryPagedout;
        uint64_t faultCount;
        uint64_t memoryLimit;
        uint64_t memoryCommitted;
    };

    struct ProcessCpuSample
    {
        std::chrono::nanoseconds totalTime;
        uint32_t threadCount;
    };

    struct ProcessStatisticsSample
    {
        ProcessCpuSample cpu;
        uint64_t memory;
    };

    int DeltaSampleCpuLoad(CpuSample &sample, std::chrono::milliseconds msec);
    int SampleCpuLoad(CpuSample &sample);
    int SampleMemoryUsage(MemorySample &sample);
    int PhysicalMemory(int64_t &);
    int SwapStat(xsw_usage &);
    int SampleProcessStatistics(int pid, ProcessStatisticsSample &sample);
    unsigned GetNumberOfCpu();
}
}




