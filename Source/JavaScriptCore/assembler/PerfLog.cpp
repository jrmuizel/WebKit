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

#include "config.h"
#include "PerfLog.h"

#if ENABLE(ASSEMBLER) && OS(LINUX)

#include <array>
#include <elf.h>
#include <fcntl.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/DataLog.h>
#include <wtf/PageBlock.h>
#include <wtf/ProcessID.h>

namespace JSC {

namespace PerfLogInternal {
static constexpr bool verbose = false;
} // namespace PerfLogInternal

PerfLog& PerfLog::singleton()
{
    static PerfLog* logger;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        logger = new PerfLog;
    });
    return *logger;
}

PerfLog::PerfLog()
{
    {
        std::array<char, 1024> filename;
        snprintf(filename.data(), filename.size() - 1, "jit-%d.dump", getCurrentProcessID());
        filename[filename.size() - 1] = '\0';
        m_fd = open(filename.data(), O_CREAT | O_TRUNC | O_RDWR, 0666);
        RELEASE_ASSERT(m_fd != -1);

        // Linux perf command records this mmap operation in perf.data as a metadata to the JIT perf annotations.
        // We do not use this mmap-ed memory region actually.
        m_marker = mmap(nullptr, pageSize(), PROT_READ | PROT_EXEC, MAP_PRIVATE, m_fd, 0);
        RELEASE_ASSERT(m_marker != MAP_FAILED);

        m_file = fdopen(m_fd, "wb");
        RELEASE_ASSERT(m_file);
    }

    JITDump::FileHeader header;
    header.timestamp = generateTimestamp();
    header.pid = getCurrentProcessID();

    Locker locker { m_lock };
    write(&header, sizeof(JITDump::FileHeader));
    flush();
}

void PerfLog::write(const void* data, size_t size)
{
    size_t result = fwrite(data, 1, size, m_file);
    RELEASE_ASSERT(result == size);
}

void PerfLog::flush()
{
    fflush(m_file);
}

void PerfLog::log(CString&& name, const uint8_t* executableAddress, size_t size)
{
    if (!size) {
        dataLogLnIf(PerfLogInternal::verbose, "0 size record ", name, " ", RawPointer(executableAddress));
        return;
    }

    PerfLog& logger = singleton();
    Locker locker { logger.m_lock };

    JITDump::CodeLoadRecord record;
    record.header.timestamp = generateTimestamp();
    record.header.totalSize = sizeof(JITDump::CodeLoadRecord) + (name.length() + 1) + size;
    record.pid = getCurrentProcessID();
    record.tid = getCurrentThreadID();
    record.vma = bitwise_cast<uintptr_t>(executableAddress);
    record.codeAddress = bitwise_cast<uintptr_t>(executableAddress);
    record.codeSize = size;
    record.codeIndex = logger.m_codeIndex++;

    logger.write(&record, sizeof(JITDump::CodeLoadRecord));
    logger.write(name.data(), name.length() + 1);
    logger.write(executableAddress, size);
    logger.flush();

    dataLogLnIf(PerfLogInternal::verbose, name, " [", record.codeIndex, "] ", RawPointer(executableAddress), "-", RawPointer(executableAddress + size), " ", size);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && OS(LINUX)
