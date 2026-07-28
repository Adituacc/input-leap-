/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#define FILE_CHUNK_META_SIZE 2

namespace inputleap {

class IStream;

class FileChunk {
public:
    static constexpr std::size_t kMaxFileSize = 256u * 1024u * 1024u;

    static FileChunk start(std::size_t size);
    static FileChunk data(std::uint8_t* data, size_t dataSize);
    static FileChunk end();
    static int assemble(inputleap::IStream* stream, std::string& dataCached, size_t& expectedSize);

    std::uint8_t mark_ = 0;
    std::string data_;
};

class FileChunkAssembler {
public:
    int assemble(inputleap::IStream* stream, std::string& data_cached,
                 std::size_t& expected_size);

private:
    void reset();

    std::size_t received_data_size_ = 0;
    double elapsed_time_ = 0.0;
    std::chrono::steady_clock::time_point interval_started_;
    bool active_ = false;
};

} // namespace inputleap
