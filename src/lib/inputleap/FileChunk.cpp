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

#include "inputleap/FileChunk.h"

#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "io/IStream.h"
#include "base/Stopwatch.h"
#include "base/String.h"
#include "base/Log.h"

#include <algorithm>
#include <chrono>

namespace inputleap {

static const std::uint16_t kIntervalThreshold = 1;

FileChunk FileChunk::start(std::size_t size)
{
    FileChunk chunk;
    chunk.mark_ = kDataStart;
    chunk.data_ = std::to_string(size);
    return chunk;
}

FileChunk FileChunk::data(std::uint8_t* data, size_t dataSize)
{
    FileChunk chunk;
    chunk.mark_ = kDataChunk;
    chunk.data_ = std::string(reinterpret_cast<char*>(data), dataSize);
    return chunk;
}

FileChunk FileChunk::end()
{
    FileChunk chunk;
    chunk.mark_ = kDataEnd;
    return chunk;
}

int FileChunk::assemble(inputleap::IStream* stream, std::string& dataReceived, size_t& expectedSize)
{
    static thread_local FileChunkAssembler assembler;
    return assembler.assemble(stream, dataReceived, expectedSize);
}

void FileChunkAssembler::reset()
{
    received_data_size_ = 0;
    elapsed_time_ = 0.0;
    interval_started_ = {};
    active_ = false;
}

int FileChunkAssembler::assemble(inputleap::IStream* stream,
                                 std::string& dataReceived,
                                 size_t& expectedSize)
{
    // parse
    std::uint8_t mark = 0;
    std::string content;

    if (!ProtocolUtil::readf(stream, kMsgDFileTransfer + 4, &mark, &content)) {
        return kError;
    }

    switch (mark) {
    case kDataStart:
        if (content.empty() ||
            !std::all_of(content.begin(), content.end(), [](unsigned char c) {
                return c >= '0' && c <= '9';
            })) {
            LOG_ERR("refusing malformed file transfer size");
            reset();
            return kError;
        }
        dataReceived.clear();
        expectedSize = inputleap::string::stringToSizeType(content);
        if (expectedSize > FileChunk::kMaxFileSize) {
            LOG_ERR("refusing file transfer larger than %zu bytes",
                    FileChunk::kMaxFileSize);
            expectedSize = 0;
            reset();
            return kError;
        }
        received_data_size_ = 0;
        elapsed_time_ = 0;
        interval_started_ = std::chrono::steady_clock::now();
        active_ = true;

        if (CLOG->getFilter() >= kDEBUG2) {
            LOG_DEBUG2("recv file size=%s", content.c_str());
        }
        return kStart;

    case kDataChunk:
        if (!active_) {
            LOG_ERR("received file data before transfer start");
            return kError;
        }
        if (content.size() > expectedSize ||
            dataReceived.size() > expectedSize - content.size()) {
            LOG_ERR("file transfer exceeded announced size");
            dataReceived.clear();
            expectedSize = 0;
            reset();
            return kError;
        }
        dataReceived.append(content);
        if (CLOG->getFilter() >= kDEBUG2) {
                LOG_DEBUG2("recv file chunk size=%zi", content.size());
                const auto now = std::chrono::steady_clock::now();
                const double interval =
                    std::chrono::duration<double>(now - interval_started_).count();
                received_data_size_ += content.size();
                LOG_DEBUG2("recv file interval=%f s", interval);
                if (interval >= kIntervalThreshold) {
                    double averageSpeed = received_data_size_ / interval / 1000;
                    LOG_DEBUG2("recv file average speed=%f kb/s", averageSpeed);

                    received_data_size_ = 0;
                    elapsed_time_ += interval;
                    interval_started_ = now;
                }
            }
        return kNotFinish;

    case kDataEnd:
        if (!active_) {
            LOG_ERR("received file end before transfer start");
            return kError;
        }
        if (expectedSize != dataReceived.size()) {
            LOG_ERR("corrupted clipboard data, expected size=%zd actual size=%zd", expectedSize, dataReceived.size());
            reset();
            return kError;
        }

        if (CLOG->getFilter() >= kDEBUG2) {
            LOG_DEBUG2("file transfer finished");
            elapsed_time_ += std::chrono::duration<double>(
                std::chrono::steady_clock::now() - interval_started_).count();
            double averageSpeed =
                elapsed_time_ > 0.0 ? expectedSize / elapsed_time_ / 1000 : 0.0;
            LOG_DEBUG2("file transfer finished: total time consumed=%f s", elapsed_time_);
            LOG_DEBUG2("file transfer finished: total data received=%zi kb", expectedSize / 1000);
            LOG_DEBUG2("file transfer finished: total average speed=%f kb/s", averageSpeed);
        }
        reset();
        return kFinish;
        default:
            break;
    }

    return kError;
}

} // namespace inputleap
