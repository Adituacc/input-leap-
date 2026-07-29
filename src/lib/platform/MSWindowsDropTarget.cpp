/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/MSWindowsDropTarget.h"

#include "base/Log.h"
#include "inputleap/DragPayload.h"

#include <Shlobj.h>
#include <Shellapi.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace inputleap {

namespace {

constexpr std::size_t kMaxMaterializedPayloadSize = 64u * 1024u * 1024u;
constexpr UINT kMaxDraggedItems = 256;

std::string wide_to_utf8(const wchar_t* value)
{
    if (value == nullptr || *value == L'\0') {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            &result[0], size, nullptr, nullptr) == 0) {
        return {};
    }
    result.pop_back();
    return result;
}

std::string hglobal_bytes(IDataObject* data_object, CLIPFORMAT format,
                          LONG index = -1)
{
    FORMATETC request{format, nullptr, DVASPECT_CONTENT, index, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(data_object->GetData(&request, &medium))) {
        return {};
    }

    std::string result;
    const auto size = static_cast<std::size_t>(GlobalSize(medium.hGlobal));
    const void* data = GlobalLock(medium.hGlobal);
    if (data != nullptr && size > 0 && size <= kMaxMaterializedPayloadSize) {
        result.assign(static_cast<const char*>(data), size);
    }
    if (data != nullptr) {
        GlobalUnlock(medium.hGlobal);
    }
    ReleaseStgMedium(&medium);
    return result;
}

std::vector<std::string> dropped_files(IDataObject* data_object)
{
    FORMATETC request{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(data_object->GetData(&request, &medium))) {
        return {};
    }

    std::vector<std::string> paths;
    const auto drop = static_cast<HDROP>(medium.hGlobal);
    const UINT count = (std::min)(DragQueryFileW(drop, 0xffffffffu, nullptr, 0),
                                  kMaxDraggedItems);
    for (UINT index = 0; index < count; ++index) {
        const UINT length = DragQueryFileW(drop, index, nullptr, 0);
        std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1u, L'\0');
        if (DragQueryFileW(drop, index, buffer.data(),
                           static_cast<UINT>(buffer.size())) != 0) {
            auto path = wide_to_utf8(buffer.data());
            if (!path.empty()) {
                paths.push_back(std::move(path));
            }
        }
    }
    ReleaseStgMedium(&medium);
    return paths;
}

std::string stream_bytes(IStream* stream)
{
    std::string result;
    char buffer[64u * 1024u];
    while (result.size() <= kMaxMaterializedPayloadSize) {
        ULONG read = 0;
        const auto status = stream->Read(buffer, sizeof(buffer), &read);
        if (FAILED(status)) {
            return {};
        }
        result.append(buffer, read);
        if (status == S_FALSE || read == 0) {
            break;
        }
    }
    return result.size() <= kMaxMaterializedPayloadSize ? result : std::string{};
}

std::string file_content(IDataObject* data_object, LONG index)
{
    const auto format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatA(CFSTR_FILECONTENTS));
    FORMATETC request{
        format, nullptr, DVASPECT_CONTENT, index, TYMED_ISTREAM | TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(data_object->GetData(&request, &medium))) {
        return {};
    }

    std::string result;
    if (medium.tymed == TYMED_ISTREAM && medium.pstm != nullptr) {
        result = stream_bytes(medium.pstm);
    }
    else if (medium.tymed == TYMED_HGLOBAL && medium.hGlobal != nullptr) {
        const auto size = static_cast<std::size_t>(GlobalSize(medium.hGlobal));
        const void* data = GlobalLock(medium.hGlobal);
        if (data != nullptr && size > 0 && size <= kMaxMaterializedPayloadSize) {
            result.assign(static_cast<const char*>(data), size);
        }
        if (data != nullptr) {
            GlobalUnlock(medium.hGlobal);
        }
    }
    ReleaseStgMedium(&medium);
    return result;
}

std::vector<std::string> virtual_files(IDataObject* data_object)
{
    const auto descriptor_format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatA(CFSTR_FILEDESCRIPTORW));
    FORMATETC request{
        descriptor_format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(data_object->GetData(&request, &medium))) {
        return {};
    }

    std::vector<std::pair<std::string, LONG>> descriptors;
    const auto* group = static_cast<const FILEGROUPDESCRIPTORW*>(
        GlobalLock(medium.hGlobal));
    if (group != nullptr) {
        const UINT count = (std::min)(group->cItems, kMaxDraggedItems);
        for (UINT index = 0; index < count; ++index) {
            auto name = wide_to_utf8(group->fgd[index].cFileName);
            if (!name.empty() &&
                (group->fgd[index].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                descriptors.emplace_back(std::move(name), static_cast<LONG>(index));
            }
        }
        GlobalUnlock(medium.hGlobal);
    }
    ReleaseStgMedium(&medium);

    std::vector<std::string> paths;
    for (const auto& descriptor : descriptors) {
        auto bytes = file_content(data_object, descriptor.second);
        auto path = materialize_drag_payload(bytes, descriptor.first);
        if (!path.empty()) {
            paths.push_back(std::move(path));
        }
    }
    return paths;
}

std::string bitmap_file(IDataObject* data_object)
{
    CLIPFORMAT format = CF_DIBV5;
    FORMATETC query{format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    if (FAILED(data_object->QueryGetData(&query))) {
        format = CF_DIB;
        query.cfFormat = format;
        if (FAILED(data_object->QueryGetData(&query))) {
            return {};
        }
    }

    const auto dib = hglobal_bytes(data_object, format);
    if (dib.size() < sizeof(BITMAPINFOHEADER)) {
        return {};
    }
    const auto* info = reinterpret_cast<const BITMAPINFOHEADER*>(dib.data());
    if (info->biSize < sizeof(BITMAPINFOHEADER) || info->biSize > dib.size()) {
        return {};
    }

    std::size_t color_bytes = 0;
    if (info->biBitCount <= 8) {
        const std::size_t colors =
            info->biClrUsed != 0 ? info->biClrUsed : (std::size_t{1} << info->biBitCount);
        color_bytes = colors * sizeof(RGBQUAD);
    }
    else if (info->biSize == sizeof(BITMAPINFOHEADER) &&
             info->biCompression == BI_BITFIELDS) {
        color_bytes = 3u * sizeof(DWORD);
    }

    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = static_cast<DWORD>(
        sizeof(BITMAPFILEHEADER) + info->biSize + color_bytes);
    header.bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + dib.size());
    if (header.bfOffBits > header.bfSize) {
        return {};
    }

    std::string bmp(reinterpret_cast<const char*>(&header), sizeof(header));
    bmp += dib;
    return materialize_drag_payload(bmp, "Dragged Image.bmp");
}

std::string png_file(IDataObject* data_object)
{
    const auto format = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"PNG"));
    return materialize_drag_payload(
        hglobal_bytes(data_object, format), "Dragged Image.png");
}

std::string unicode_text(const std::string& bytes)
{
    if (bytes.size() < sizeof(wchar_t)) {
        return {};
    }
    const std::size_t count = bytes.size() / sizeof(wchar_t);
    std::vector<wchar_t> value(count);
    std::memcpy(value.data(), bytes.data(), count * sizeof(wchar_t));
    if (std::find(value.begin(), value.end(), L'\0') == value.end()) {
        return {};
    }
    return wide_to_utf8(value.data());
}

std::string url_file(IDataObject* data_object)
{
    const auto wide_format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatW(L"UniformResourceLocatorW"));
    auto value = unicode_text(hglobal_bytes(data_object, wide_format));
    if (value.empty()) {
        const auto ansi_format = static_cast<CLIPFORMAT>(
            RegisterClipboardFormatW(L"UniformResourceLocator"));
        auto bytes = hglobal_bytes(data_object, ansi_format);
        const auto end = std::find(bytes.begin(), bytes.end(), '\0');
        value.assign(bytes.begin(), end);
    }
    if (!is_supported_drag_url(value)) {
        return {};
    }
    return materialize_drag_payload(
        make_portable_link_page(value), "Dragged Link.html");
}

std::string text_file(IDataObject* data_object)
{
    const auto value = unicode_text(hglobal_bytes(data_object, CF_UNICODETEXT));
    return materialize_drag_payload(value, "Dragged Text.txt");
}

bool has_format(IDataObject* data_object, CLIPFORMAT format,
                DWORD tymed = TYMED_HGLOBAL)
{
    FORMATETC request{format, nullptr, DVASPECT_CONTENT, -1, tymed};
    return SUCCEEDED(data_object->QueryGetData(&request));
}

std::vector<std::string> get_drop_data(IDataObject* data_object)
{
    auto paths = dropped_files(data_object);
    if (!paths.empty()) {
        return paths;
    }

    paths = virtual_files(data_object);
    if (!paths.empty()) {
        return paths;
    }

    auto path = png_file(data_object);
    if (path.empty()) {
        path = bitmap_file(data_object);
    }
    if (path.empty()) {
        path = url_file(data_object);
    }
    if (path.empty()) {
        path = text_file(data_object);
    }
    return path.empty() ? std::vector<std::string>{}
                        : std::vector<std::string>{std::move(path)};
}

} // namespace

MSWindowsDropTarget* MSWindowsDropTarget::s_instance = nullptr;

MSWindowsDropTarget::MSWindowsDropTarget() :
    m_refCount(1),
    m_allowDrop(false)
{
    s_instance = this;
}

MSWindowsDropTarget::~MSWindowsDropTarget() = default;

MSWindowsDropTarget& MSWindowsDropTarget::instance()
{
    assert(s_instance != nullptr);
    return *s_instance;
}

HRESULT MSWindowsDropTarget::DragEnter(
    IDataObject* dataObject, DWORD, POINTL, DWORD* effect)
{
    m_allowDrop = queryDataObject(dataObject);
    if (m_allowDrop) {
        auto paths = get_drop_data(dataObject);
        LOG_INFO("Windows native drag capture received %zi item(s)", paths.size());
        setDraggingPaths(std::move(paths));
    }
    *effect = m_allowDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}

HRESULT MSWindowsDropTarget::DragOver(DWORD, POINTL, DWORD* effect)
{
    *effect = m_allowDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}

HRESULT MSWindowsDropTarget::DragLeave()
{
    return S_OK;
}

HRESULT MSWindowsDropTarget::Drop(IDataObject*, DWORD, POINTL, DWORD* effect)
{
    *effect = m_allowDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}

bool MSWindowsDropTarget::queryDataObject(IDataObject* dataObject)
{
    const auto virtual_files_format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatA(CFSTR_FILEDESCRIPTORW));
    const auto png_format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatW(L"PNG"));
    const auto url_w_format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatW(L"UniformResourceLocatorW"));
    const auto url_format = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatW(L"UniformResourceLocator"));
    return has_format(dataObject, CF_HDROP) ||
           has_format(dataObject, virtual_files_format) ||
           has_format(dataObject, png_format) ||
           has_format(dataObject, CF_DIBV5) ||
           has_format(dataObject, CF_DIB) ||
           has_format(dataObject, url_w_format) ||
           has_format(dataObject, url_format) ||
           has_format(dataObject, CF_UNICODETEXT);
}

void MSWindowsDropTarget::setDraggingPaths(std::vector<std::string> paths)
{
    std::lock_guard<std::mutex> lock(m_dragMutex);
    m_dragPaths = std::move(paths);
}

std::string MSWindowsDropTarget::getDraggingFilename()
{
    std::lock_guard<std::mutex> lock(m_dragMutex);
    return m_dragPaths.empty() ? std::string{} : m_dragPaths.front();
}

std::vector<std::string> MSWindowsDropTarget::getDraggingPaths()
{
    std::lock_guard<std::mutex> lock(m_dragMutex);
    return m_dragPaths;
}

void MSWindowsDropTarget::clearDraggingFilename()
{
    std::lock_guard<std::mutex> lock(m_dragMutex);
    m_dragPaths.clear();
}

HRESULT __stdcall MSWindowsDropTarget::QueryInterface(REFIID iid, void** object)
{
    if (iid == IID_IDropTarget || iid == IID_IUnknown) {
        AddRef();
        *object = this;
        return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
}

ULONG __stdcall MSWindowsDropTarget::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

ULONG __stdcall MSWindowsDropTarget::Release()
{
    const LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return count;
}

} // namespace inputleap
