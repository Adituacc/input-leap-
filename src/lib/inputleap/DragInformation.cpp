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

#include "inputleap/DragInformation.h"
#include "inputleap/DragPayload.h"
#include "base/Log.h"
#include "io/filesystem.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace inputleap {

DragInformation::DragInformation() :
    m_filename(),
    m_filesize(0)
{
}

void DragInformation::parseDragInfo(DragFileList& dragFileList, std::uint32_t fileNum,
                                    std::string data)
{
    size_t startPos = 0;
    dragFileList.clear();

    std::uint32_t index = 0;
    while (index < fileNum) {
        const auto filename_end = data.find(',', startPos);
        if (filename_end == std::string::npos || filename_end == startPos) {
            break;
        }
        const auto size_end = data.find(',', filename_end + 1);
        if (size_end == std::string::npos) {
            break;
        }

        const auto full_name = data.substr(startPos, filename_end - startPos);
        const auto separator = full_name.find_last_of("/\\");
        const auto filename = separator == std::string::npos
            ? full_name : full_name.substr(separator + 1);
        if (filename.empty()) {
            break;
        }

        auto filesize = data.substr(filename_end + 1, size_end - filename_end - 1);
        DragInformation item;
        item.setFilename(filename);
        item.setFilesize(stringToNum(filesize));
        dragFileList.push_back(std::move(item));

        startPos = size_end + 1;
        ++index;
    }

    LOG_DEBUG("drag info received, total drag file number: %zi",
        dragFileList.size());

    for (size_t i = 0; i < dragFileList.size(); ++i) {
        LOG_DEBUG("dragging file %zi name: %s",
            i + 1,
            dragFileList.at(i).getFilename().c_str());
    }
}

std::string DragInformation::getDragFileExtension(std::string filename)
{
    size_t findResult = std::string::npos;
    findResult = filename.find_last_of(".", filename.size());
    if (findResult != std::string::npos) {
        return filename.substr(findResult + 1, filename.size() - findResult - 1);
    }
    else {
        return "";
    }
}

int
DragInformation::setupDragInfo(DragFileList& fileList, std::string& output)
{
    output.clear();
    int size = static_cast<int>(fileList.size());
    for (int i = 0; i < size; ++i) {
        output.append(sanitize_drag_filename(fileList.at(i).getFilename()));
        output.append(",");
        std::string filesize = getFileSize(fileList.at(i).getFilename());
        output.append(filesize);
        output.append(",");
    }
    return size;
}

bool DragInformation::isFileValid(std::string filename)
{
    try {
        return fs::exists(fs::u8path(filename));
    }
    catch (const fs::filesystem_error&) {
        return false;
    }
}

size_t DragInformation::stringToNum(std::string& str)
{
    std::istringstream iss(str.c_str());
    size_t size = 0;
    iss >> size;
    if (iss.fail()) {
        return 0;
    }
    return size;
}

std::string DragInformation::getFileSize(const std::string& filename)
{
    std::uintmax_t size = 0;
    try {
        const auto path = fs::u8path(filename);
        if (fs::is_regular_file(path)) {
            size = fs::file_size(path);
        }
        else if (!fs::is_directory(path)) {
            throw std::runtime_error("drag source is not a file or directory");
        }
    }
    catch (const fs::filesystem_error& error) {
        throw std::runtime_error(error.what());
    }

    std::stringstream ss;
    ss << size;
    return ss.str();
}

} // namespace inputleap
