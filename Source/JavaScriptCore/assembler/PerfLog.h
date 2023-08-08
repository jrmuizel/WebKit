/*
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ASSEMBLER) && OS(LINUX)

#include <stdio.h>
#include <wtf/Lock.h>
#include <wtf/text/CString.h>
#include <wtf/MonotonicTime.h>
#include <elf.h>

namespace JSC {
static inline uint64_t generateTimestamp()
{
    return MonotonicTime::now().secondsSinceEpoch().nanosecondsAs<uint64_t>();
}

static inline pid_t getCurrentThreadID()
{
    return static_cast<pid_t>(syscall(__NR_gettid));
}


class PerfLog {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PerfLog);
public:
    static void log(CString&&, const uint8_t* executableAddress, size_t);

    void write(const void*, size_t) WTF_REQUIRES_LOCK(m_lock);
    void flush() WTF_REQUIRES_LOCK(m_lock);
    Lock m_lock;
    static PerfLog& singleton();
private:
    PerfLog();


    FILE* m_file { nullptr };
    void* m_marker { nullptr };
    uint64_t m_codeIndex { 0 };
    int m_fd { -1 };
};
namespace JITDump {
namespace Constants {

// Perf jit-dump formats are specified here.
// https://raw.githubusercontent.com/torvalds/linux/master/tools/perf/Documentation/jitdump-specification.txt

// The latest version 2, but it is too new at that time.
static constexpr uint32_t version = 1;

#if CPU(LITTLE_ENDIAN)
static constexpr uint32_t magic = 0x4a695444;
#else
static constexpr uint32_t magic = 0x4454694a;
#endif

#if CPU(X86)
static constexpr uint32_t elfMachine = EM_386;
#elif CPU(X86_64)
static constexpr uint32_t elfMachine = EM_X86_64;
#elif CPU(ARM64)
static constexpr uint32_t elfMachine = EM_AARCH64;
#elif CPU(ARM)
static constexpr uint32_t elfMachine = EM_ARM;
#elif CPU(MIPS)
#if CPU(LITTLE_ENDIAN)
static constexpr uint32_t elfMachine = EM_MIPS_RS3_LE;
#else
static constexpr uint32_t elfMachine = EM_MIPS;
#endif
#elif CPU(RISCV64)
static constexpr uint32_t elfMachine = EM_RISCV;
#endif

} // namespace Constants

struct FileHeader {
    uint32_t magic { Constants::magic };
    uint32_t version { Constants::version };
    uint32_t totalSize { sizeof(FileHeader) };
    uint32_t elfMachine { Constants::elfMachine };
    uint32_t padding1 { 0 };
    uint32_t pid { 0 };
    uint64_t timestamp { 0 };
    uint64_t flags { 0 };
};

enum class RecordType : uint32_t {
    JITCodeLoad = 0,
    JITCodeMove = 1,
    JITCodeDebugInfo = 2,
    JITCodeClose = 3,
    JITCodeUnwindingInfo = 4,
};

struct RecordHeader {
    RecordType type { RecordType::JITCodeLoad };
    uint32_t totalSize { 0 };
    uint64_t timestamp { 0 };
};

struct CodeLoadRecord {
    RecordHeader header {
        RecordType::JITCodeLoad,
        0,
        0,
    };
    uint32_t pid { 0 };
    uint32_t tid { 0 };
    uint64_t vma { 0 };
    uint64_t codeAddress { 0 };
    uint64_t codeSize { 0 };
    uint64_t codeIndex { 0 };
};

struct CodeDebugInfoRecord {
    RecordHeader header {
        RecordType::JITCodeDebugInfo,
        0,
        0,
    };
    uint64_t codeAddress { 0 };
    uint64_t nrEntry { 0 };
};

struct DebugEntry {
    uint64_t codeAddress { 0 };
    uint32_t line { 0 };
    uint32_t discrim { 0 };
};


} // namespace JITDump

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && OS(LINUX)
