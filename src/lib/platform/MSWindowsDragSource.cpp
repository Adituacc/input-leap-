/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/MSWindowsDragSource.h"

#include "base/Log.h"
#include "common/win32/encoding_utilities.h"
#include "io/filesystem.h"

#include <Shlobj.h>
#include <Windows.h>

#include <atomic>
#include <thread>
#include <utility>
#include <vector>

namespace inputleap {

namespace {

class NativeDropSource final : public IDropSource {
public:
    explicit NativeDropSource(std::function<bool()> left_button_down) :
        left_button_down_(std::move(left_button_down))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IDropSource) {
            *object = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const auto references = --references_;
        if (references == 0) {
            delete this;
        }
        return references;
    }

    HRESULT STDMETHODCALLTYPE QueryContinueDrag(
        BOOL escape_pressed, DWORD) override
    {
        if (escape_pressed) {
            return DRAGDROP_S_CANCEL;
        }
        if (!left_button_down_()) {
            return DRAGDROP_S_DROP;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override
    {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

private:
    std::atomic<ULONG> references_{1};
    std::function<bool()> left_button_down_;
};

IDataObject* create_file_data_object(const std::vector<std::string>& paths)
{
    if (paths.empty()) {
        return nullptr;
    }

    const auto parent_path = fs::u8path(paths.front()).parent_path();
    const auto wide_parent_path = utf8_to_win_char(parent_path.u8string());
    PIDLIST_ABSOLUTE parent_id = nullptr;
    if (wide_parent_path.empty() ||
        FAILED(SHParseDisplayName(
            wide_parent_path.data(), nullptr, &parent_id, 0, nullptr)) ||
        parent_id == nullptr) {
        LOG_ERR("failed to create shell parent for native drag: %s",
                parent_path.u8string().c_str());
        return nullptr;
    }

    std::vector<PIDLIST_ABSOLUTE> item_ids;
    std::vector<PCUITEMID_CHILD> child_ids;
    item_ids.reserve(paths.size());
    child_ids.reserve(paths.size());

    for (const auto& path : paths) {
        if (fs::u8path(path).parent_path() != parent_path) {
            LOG_ERR("native drag items must share one staging directory");
            continue;
        }
        const auto wide_path = utf8_to_win_char(path);
        if (wide_path.empty()) {
            continue;
        }
        PIDLIST_ABSOLUTE item_id = nullptr;
        const auto result = SHParseDisplayName(
            wide_path.data(), nullptr, &item_id, 0, nullptr);
        if (FAILED(result) || item_id == nullptr) {
            LOG_ERR("failed to create shell item for native drag: %s", path.c_str());
            continue;
        }
        item_ids.push_back(item_id);
        child_ids.push_back(
            reinterpret_cast<PCUITEMID_CHILD>(ILFindLastID(item_id)));
    }

    IDataObject* data_object = nullptr;
    if (!child_ids.empty()) {
        const auto result = SHCreateDataObject(
            parent_id, static_cast<UINT>(child_ids.size()), child_ids.data(),
            nullptr, IID_IDataObject,
            reinterpret_cast<void**>(&data_object));
        if (FAILED(result)) {
            LOG_ERR("failed to create Windows native drag data object: 0x%08x",
                    result);
        }
    }

    for (const auto item_id : item_ids) {
        CoTaskMemFree(item_id);
    }
    CoTaskMemFree(parent_id);
    return data_object;
}

} // namespace

bool MSWindowsDragSource::drag_files(const std::vector<std::string>& paths)
{
    return drag_files(paths, []() {
        return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    });
}

bool MSWindowsDragSource::drag_files(
    const std::vector<std::string>& paths,
    const std::function<bool()>& left_button_down)
{
    if (paths.empty() || !left_button_down) {
        return false;
    }

    const auto ole_result = OleInitialize(nullptr);
    if (FAILED(ole_result)) {
        LOG_ERR("failed to initialize OLE drag source: 0x%08x", ole_result);
        return false;
    }

    IDataObject* data_object = create_file_data_object(paths);
    if (data_object == nullptr) {
        OleUninitialize();
        return false;
    }

    auto* drop_source = new NativeDropSource(left_button_down);
    DWORD effect = DROPEFFECT_NONE;
    LOG_INFO("starting Windows native destination drag with %zi item(s)",
             paths.size());

    // Input Leap observes the physical mouse-up on its event thread, while
    // DoDragDrop owns this dedicated OLE thread. Wake the OLE modal loop when
    // Input Leap's button state changes so it can call QueryContinueDrag and
    // finish the drop even though the original mouse message went elsewhere.
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    const DWORD drag_thread_id = GetCurrentThreadId();
    std::atomic<bool> drag_finished{false};
    std::thread release_watcher(
        [&drag_finished, drag_thread_id, left_button_down]() {
            while (!drag_finished.load()) {
                const bool button_down = left_button_down();
                GUITHREADINFO thread_info{sizeof(thread_info)};
                if (GetGUIThreadInfo(drag_thread_id, &thread_info) &&
                    thread_info.hwndCapture != nullptr) {
                    POINT cursor{};
                    if (GetCursorPos(&cursor)) {
                        ScreenToClient(thread_info.hwndCapture, &cursor);
                        PostMessageW(
                            thread_info.hwndCapture, WM_MOUSEMOVE,
                            button_down ? MK_LBUTTON : 0,
                            MAKELPARAM(cursor.x, cursor.y));
                    }
                    if (!button_down) {
                        PostMessageW(
                            thread_info.hwndCapture, WM_LBUTTONUP, 0, 0);
                    }
                }
                if (!button_down) {
                    PostThreadMessageW(drag_thread_id, WM_LBUTTONUP, 0, 0);
                }
                Sleep(10);
            }
        });

    const auto drag_result = DoDragDrop(
        data_object, drop_source, DROPEFFECT_COPY, &effect);
    drag_finished = true;
    release_watcher.join();
    const bool dropped =
        drag_result == DRAGDROP_S_DROP && (effect & DROPEFFECT_COPY) != 0;
    if (dropped) {
        LOG_INFO("Windows native destination drop completed");
    }
    else {
        LOG_WARN(
            "Windows native destination drag ended without a copy "
            "(result=0x%08x, effect=0x%08x)",
            drag_result, effect);
    }

    drop_source->Release();
    data_object->Release();
    OleUninitialize();
    return dropped;
}

} // namespace inputleap
