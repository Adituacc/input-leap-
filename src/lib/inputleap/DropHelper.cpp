/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2014-2016 Symless Ltd.
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

#include "inputleap/DropHelper.h"

#include "inputleap/DragPayload.h"
#include "base/Log.h"
#include "io/filesystem.h"

#include <fstream>

namespace inputleap {

namespace {

fs::path available_drop_path(const fs::path& directory, const std::string& filename)
{
    const auto safe_filename = sanitize_drag_filename(filename);
    fs::path candidate = directory / fs::u8path(safe_filename);
    if (!fs::exists(candidate)) {
        return candidate;
    }

    const auto stem = candidate.stem().u8string();
    const auto extension = candidate.extension().u8string();
    for (unsigned suffix = 1; suffix < 10000; ++suffix) {
        candidate = directory / fs::u8path(
            stem + " (" + std::to_string(suffix) + ")" + extension);
        if (!fs::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

} // namespace

void
DropHelper::writeToDir(const std::string& destination, DragFileList& fileList, std::string& data)
{
    LOG_DEBUG("dropping file, files=%zi target=%s", fileList.size(), destination.c_str());

    if (!destination.empty() && fileList.size() > 0) {
        std::fstream file;
        const auto dropTarget = available_drop_path(
            fs::u8path(destination), fileList.at(0).getFilename());
        if (dropTarget.empty()) {
            LOG_ERR("drop file failed: could not choose a unique destination");
            return;
        }

        open_utf8_path(file, dropTarget, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            LOG_ERR("drop file failed: can not open %s", dropTarget.u8string().c_str());
            return;
        }

        file.write(data.data(), data.size());
        file.close();

        LOG_INFO("dropped file \"%s\"", dropTarget.u8string().c_str());

        fileList.clear();
    }
    else {
        LOG_ERR("drop file failed: drop target is empty");
    }
}

} // namespace inputleap
