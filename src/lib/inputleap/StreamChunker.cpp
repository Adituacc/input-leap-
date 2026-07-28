/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2013-2016 Symless Ltd.
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

#include "inputleap/StreamChunker.h"

#include "inputleap/FileChunk.h"
#include "inputleap/ClipboardChunk.h"
#include "inputleap/protocol_types.h"
#include "base/EventTypes.h"
#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/EventTypes.h"
#include "base/finally.h"
#include "base/Log.h"
#include "base/String.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace inputleap {

static const size_t g_chunkSize = 32 * 1024; //32kb

std::atomic_bool StreamChunker::s_isChunkingFile{false};
std::atomic_bool StreamChunker::s_interruptFile{false};

void StreamChunker::sendFile(const char* filename, IEventQueue* events,
                             const EventTarget* event_target)
{
    s_interruptFile.store(false);
    s_isChunkingFile.store(true);
    auto reset_chunking = finally([]() {
        StreamChunker::s_isChunkingFile.store(false);
        StreamChunker::s_interruptFile.store(false);
    });

    std::fstream file(filename, std::ios::in | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file");
    }

    // check file size
    file.seekg (0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    if (size > FileChunk::kMaxFileSize) {
        throw std::runtime_error("file exceeds maximum drag transfer size");
    }

    // send first message (file size)
    auto size_message = FileChunk::start(size);

    events->add_event(EventType::FILE_CHUNK_SENDING, event_target,
                      create_event_data<FileChunk>(size_message));

    // send chunk messages with a fixed chunk size
    size_t sentLength = 0;
    file.seekg (0, std::ios::beg);

    while (sentLength < size) {
        if (s_interruptFile.load()) {
            LOG_DEBUG("file transmission interrupted");
            break;
        }

        events->add_event(EventType::FILE_KEEPALIVE, event_target);

        const auto chunkSize = (std::min)(g_chunkSize, size - sentLength);
        std::vector<std::uint8_t> chunkData(chunkSize);
        file.read(reinterpret_cast<char*>(chunkData.data()),
                  static_cast<std::streamsize>(chunkSize));
        if (static_cast<std::size_t>(file.gcount()) != chunkSize) {
            throw std::runtime_error("failed while reading file");
        }
        FileChunk fileChunk = FileChunk::data(chunkData.data(), chunkSize);

        events->add_event(EventType::FILE_CHUNK_SENDING, event_target,
                          create_event_data<FileChunk>(fileChunk));

        sentLength += chunkSize;
    }

    // send last message
    FileChunk end = FileChunk::end();

    events->add_event(EventType::FILE_CHUNK_SENDING, event_target,
                      create_event_data<FileChunk>(end));

    file.close();

}

void StreamChunker::sendClipboard(std::string& data, std::size_t size, ClipboardID id,
                                  std::uint32_t sequence, IEventQueue* events,
                                  const EventTarget* event_target)
{
    // send first message (data size)
    ClipboardChunk size_message = ClipboardChunk::start(id, sequence, size);

    events->add_event(EventType::CLIPBOARD_SENDING, event_target,
                      create_event_data<ClipboardChunk>(size_message));

    // send clipboard chunk with a fixed size
    size_t sentLength = 0;
    size_t chunkSize = g_chunkSize;

    while (true) {
        events->add_event(EventType::FILE_KEEPALIVE, event_target);

        // make sure we don't read too much from the mock data.
        if (sentLength + chunkSize > size) {
            chunkSize = size - sentLength;
        }

        std::string chunk(data.substr(sentLength, chunkSize).c_str(), chunkSize);
        ClipboardChunk data_chunk = ClipboardChunk::data(id, sequence, chunk);

        events->add_event(EventType::CLIPBOARD_SENDING, event_target,
                          create_event_data<ClipboardChunk>(data_chunk));

        sentLength += chunkSize;
        if (sentLength == size) {
            break;
        }
    }

    // send last message
    ClipboardChunk end = ClipboardChunk::end(id, sequence);

    events->add_event(EventType::CLIPBOARD_SENDING, event_target,
                      create_event_data<ClipboardChunk>(end));

    LOG_DEBUG("sent clipboard size=%zd", sentLength);
}

void
StreamChunker::interruptFile()
{
    if (s_isChunkingFile.load()) {
        s_interruptFile.store(true);
        LOG_INFO("previous dragged file has become invalid");
    }
}

} // namespace inputleap
