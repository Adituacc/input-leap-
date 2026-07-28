/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/DragPayload.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace inputleap {

namespace {

std::string lowercase_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool is_windows_reserved_name(const std::string& filename)
{
    auto stem = filename.substr(0, filename.find('.'));
    stem = lowercase_ascii(stem);

    static const std::set<std::string> reserved{
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
    };
    return reserved.count(stem) != 0;
}

} // namespace

bool is_supported_drag_url(const std::string& value)
{
    if (value.find_first_of("\r\n") != std::string::npos || value.size() > 8192) {
        return false;
    }

    const auto lower = lowercase_ascii(value);
    return lower.rfind("https://", 0) == 0 || lower.rfind("http://", 0) == 0;
}

std::string make_windows_internet_shortcut(const std::string& url)
{
    if (!is_supported_drag_url(url)) {
        return {};
    }
    return "[InternetShortcut]\r\nURL=" + url + "\r\n";
}

std::string sanitize_drag_filename(const std::string& value, const std::string& fallback)
{
    const auto separator = value.find_last_of("/\\");
    std::string filename =
        separator == std::string::npos ? value : value.substr(separator + 1);

    for (char& c : filename) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' || c == ',') {
            c = '_';
        }
    }

    while (!filename.empty() && (filename.back() == ' ' || filename.back() == '.')) {
        filename.pop_back();
    }

    if (filename.empty() || filename == "." || filename == "..") {
        filename = fallback;
    }
    if (is_windows_reserved_name(filename)) {
        filename.insert(filename.begin(), '_');
    }
    return filename;
}

} // namespace inputleap
