/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include <wtf/FileLineNumberingPrintStream.h>
#include <wtf/StringPrintStream.h>

namespace WTF {

FileLineNumberingPrintStream::FileLineNumberingPrintStream(FILE* file, AdoptionMode adoptionMode)
    : m_file(file)
    , m_adoptionMode(adoptionMode)
{
}

FileLineNumberingPrintStream::~FileLineNumberingPrintStream()
{
    if (m_adoptionMode == Borrow)
        return;
    fclose(m_file);
}

std::unique_ptr<FileLineNumberingPrintStream> FileLineNumberingPrintStream::open(const char* filename, const char* mode)
{
    FILE* file = fopen(filename, mode);
    if (!file)
        return nullptr;

    return makeUnique<FileLineNumberingPrintStream>(file);
}

void FileLineNumberingPrintStream::vprintf(const char* format, va_list argList)
{
    StringPrintStream s;
    s.vprintf(format, argList);
    CString cs = s.toCString();
    for (auto o : cs.bytes()) {
        if (o == '\n') {
            m_lineNo++;
        }
    }
    fwrite(cs.data(), cs.length(), 1, m_file);
}

void FileLineNumberingPrintStream::flush()
{
    fflush(m_file);
}

} // namespace WTF

