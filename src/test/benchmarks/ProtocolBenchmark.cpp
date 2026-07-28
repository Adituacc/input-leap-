/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/ProtocolUtil.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "io/IStream.h"

#if SYSAPI_WIN32
#include "arch/win32/ArchMiscWindows.h"
#endif

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

class SinkStream final : public inputleap::IStream {
public:
    void close() override {}
    std::uint32_t read(void*, std::uint32_t) override { return 0; }

    void write(const void* data, std::uint32_t size) override
    {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        bytes_ += size;
        if (size > 0) {
            checksum_ += bytes[0];
            checksum_ += bytes[size - 1];
        }
    }

    void flush() override {}
    void shutdownInput() override {}
    void shutdownOutput() override {}
    const inputleap::EventTarget* get_event_target() const override { return nullptr; }
    bool isReady() const override { return false; }
    std::uint32_t getSize() const override { return 0; }

    std::uint64_t bytes() const { return bytes_; }
    std::uint64_t checksum() const { return checksum_; }

private:
    std::uint64_t bytes_ = 0;
    std::uint64_t checksum_ = 0;
};

} // namespace

int main(int argc, char** argv)
{
#if SYSAPI_WIN32
    inputleap::ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#endif
    inputleap::Arch arch;
    arch.init();
    inputleap::Log log;
    log.setFilter(kNOTE);

    std::uint64_t iterations = 2'000'000;
    if (argc == 2) {
        iterations = std::strtoull(argv[1], nullptr, 10);
    }
    if (iterations == 0) {
        std::cerr << "iterations must be greater than zero\n";
        return 2;
    }

    SinkStream stream;
    const auto start = std::chrono::steady_clock::now();

    for (std::uint64_t i = 0; i < iterations; ++i) {
        inputleap::ProtocolUtil::writef(&stream, "DMMV%2i%2i",
                                       static_cast<std::uint32_t>(i & 0xffffu),
                                       static_cast<std::uint32_t>((i * 3u) & 0xffffu));
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    const double ns_per_operation =
        static_cast<double>(elapsed_ns) / static_cast<double>(iterations);
    const double operations_per_second =
        1.0e9 / ns_per_operation;

    std::cout << "iterations=" << iterations
              << " elapsed_ns=" << elapsed_ns
              << " ns_per_operation=" << ns_per_operation
              << " operations_per_second=" << operations_per_second
              << " bytes=" << stream.bytes()
              << " checksum=" << stream.checksum()
              << '\n';
    return 0;
}
