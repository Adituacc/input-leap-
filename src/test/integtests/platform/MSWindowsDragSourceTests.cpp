/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/MSWindowsDragSource.h"

#include "common/win32/encoding_utilities.h"
#include "io/filesystem.h"

#include <gtest/gtest.h>

#include <Shlobj.h>
#include <Shellapi.h>
#include <Windows.h>

#include <atomic>
#include <fstream>
#include <thread>

namespace inputleap {

namespace {

class TestDropTarget final : public IDropTarget {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IDropTarget) {
            *object = static_cast<IDropTarget*>(this);
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

    HRESULT STDMETHODCALLTYPE DragEnter(
        IDataObject*, DWORD, POINTL, DWORD* effect) override
    {
        if (effect != nullptr) {
            *effect &= DROPEFFECT_COPY;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) override
    {
        if (effect != nullptr) {
            *effect &= DROPEFFECT_COPY;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(
        IDataObject* data_object, DWORD, POINTL, DWORD* effect) override
    {
        const auto url_format =
            RegisterClipboardFormatW(L"UniformResourceLocatorW");
        FORMATETC url_format_descriptor{
            static_cast<CLIPFORMAT>(url_format), nullptr,
            DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        received_url_format_ =
            SUCCEEDED(data_object->QueryGetData(&url_format_descriptor));

        FORMATETC format{
            CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (FAILED(data_object->GetData(&format, &medium))) {
            if (effect != nullptr) {
                *effect = DROPEFFECT_NONE;
            }
            return DV_E_FORMATETC;
        }

        const auto drop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
        if (drop != nullptr && DragQueryFileW(drop, 0, nullptr, 0) > 0) {
            const auto length = DragQueryFileW(drop, 0, nullptr, 0);
            std::wstring value(length + 1, L'\0');
            DragQueryFileW(drop, 0, value.data(), length + 1);
            value.resize(length);
            received_path_ = win_wchar_to_utf8(value.c_str());
            received_ = true;
        }
        if (drop != nullptr) {
            GlobalUnlock(medium.hGlobal);
        }
        ReleaseStgMedium(&medium);

        if (effect != nullptr) {
            *effect = received_ ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        return received_ ? S_OK : E_FAIL;
    }

    bool received() const
    {
        return received_;
    }

    const std::string& received_path() const
    {
        return received_path_;
    }

    bool received_url_format() const
    {
        return received_url_format_;
    }

private:
    std::atomic<ULONG> references_{1};
    bool received_ = false;
    bool received_url_format_ = false;
    std::string received_path_;
};

LRESULT CALLBACK drop_test_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

TEST(MSWindowsDragSourceTests, dragFiles_releaseOverWindow_dropsAtCursor)
{
    const auto ole_result = OleInitialize(nullptr);
    ASSERT_TRUE(SUCCEEDED(ole_result));

    const auto module = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"InputLeapNativeDragIntegrationTarget";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = drop_test_window_proc;
    window_class.hInstance = module;
    window_class.lpszClassName = class_name;
    RegisterClassW(&window_class);

    const auto window = CreateWindowExW(
        WS_EX_TOPMOST, class_name, L"Input Leap native drag test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 320, 240, nullptr,
        nullptr, module, nullptr);
    ASSERT_NE(nullptr, window);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    auto* target = new TestDropTarget();
    ASSERT_TRUE(SUCCEEDED(RegisterDragDrop(window, target)));

    const auto test_path =
        fs::temp_directory_path() / fs::u8path("inputleap-native-drag-test.url");
    {
        std::ofstream stream(test_path, std::ios::binary | std::ios::trunc);
        stream << "[InternetShortcut]\r\nURL=https://example.com/\r\n";
    }

    SetForegroundWindow(window);
    BringWindowToTop(window);
    RECT window_rect{};
    ASSERT_TRUE(GetWindowRect(window, &window_rect));
    const POINT cursor{
        (window_rect.left + window_rect.right) / 2,
        (window_rect.top + window_rect.bottom) / 2};
    ASSERT_TRUE(SetCursorPos(window_rect.right + 50, cursor.y));
    std::atomic<bool> inputleap_button_down{true};
    std::thread release_mouse([cursor, &inputleap_button_down]() {
        Sleep(250);
        SetCursorPos(cursor.x, cursor.y);
        Sleep(100);
        inputleap_button_down = false;
    });

    const bool dropped = MSWindowsDragSource::drag_files(
        {test_path.u8string()},
        [&inputleap_button_down]() {
            return inputleap_button_down.load();
        });
    release_mouse.join();

    EXPECT_TRUE(dropped);
    EXPECT_TRUE(target->received());
    EXPECT_TRUE(target->received_url_format());
    EXPECT_TRUE(fs::equivalent(test_path, fs::u8path(target->received_path())));

    RevokeDragDrop(window);
    target->Release();
    DestroyWindow(window);
    UnregisterClassW(class_name, module);
    std::error_code error;
    fs::remove(test_path, error);
    OleUninitialize();
}

} // namespace inputleap
