/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#import "platform/OSXPasteboardPeeker.h"

#include "inputleap/DragPayload.h"
#include "base/Log.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <cctype>

namespace inputleap {

namespace {

constexpr NSUInteger kMaxMaterializedPayloadSize = 64u * 1024u * 1024u;

std::string to_utf8(NSString* value)
{
    if (value == nil) {
        return {};
    }
    const char* bytes = [value UTF8String];
    return bytes == nullptr ? std::string{} : std::string{bytes};
}

NSString* payload_directory()
{
    NSString* root = [NSTemporaryDirectory()
        stringByAppendingPathComponent:@"InputLeapDragPayloads"];
    NSError* error = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:root
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:&error]) {
        LOG_ERR("failed to create drag payload directory: %s",
                to_utf8([error localizedDescription]).c_str());
        return nil;
    }

    // Payload files must outlive the asynchronous transfer, so retain them for
    // the current session and prune only stale material from previous runs.
    NSDate* cutoff = [NSDate dateWithTimeIntervalSinceNow:-(24.0 * 60.0 * 60.0)];
    NSArray* children = [[NSFileManager defaultManager]
        contentsOfDirectoryAtPath:root error:nil];
    for (NSString* child in children) {
        NSString* path = [root stringByAppendingPathComponent:child];
        NSDictionary* attributes = [[NSFileManager defaultManager]
            attributesOfItemAtPath:path error:nil];
        NSDate* modified = [attributes objectForKey:NSFileModificationDate];
        if (modified != nil && [modified compare:cutoff] == NSOrderedAscending) {
            [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
        }
    }
    return root;
}

std::string write_payload(NSData* data, const std::string& suggested_name)
{
    if (data == nil || [data length] == 0 || [data length] > kMaxMaterializedPayloadSize) {
        LOG_WARN("refusing empty or oversized materialized drag payload");
        return {};
    }

    NSString* root = payload_directory();
    if (root == nil) {
        return {};
    }

    NSString* unique_directory = [root
        stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
    NSError* error = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:unique_directory
                                  withIntermediateDirectories:NO
                                                   attributes:nil
                                                        error:&error]) {
        LOG_ERR("failed to create unique drag directory: %s",
                to_utf8([error localizedDescription]).c_str());
        return {};
    }

    const auto safe_name = sanitize_drag_filename(suggested_name);
    NSString* filename = [NSString stringWithUTF8String:safe_name.c_str()];
    NSString* path = [unique_directory stringByAppendingPathComponent:filename];
    if (![data writeToFile:path options:NSDataWritingAtomic error:&error]) {
        LOG_ERR("failed to materialize drag payload: %s",
                to_utf8([error localizedDescription]).c_str());
        return {};
    }

    LOG_DEBUG("materialized drag payload: %s", to_utf8(path).c_str());
    return to_utf8(path);
}

std::string first_dragged_file(NSPasteboard* pasteboard)
{
    NSDictionary* options = [NSDictionary
        dictionaryWithObject:[NSNumber numberWithBool:YES]
                      forKey:NSPasteboardURLReadingFileURLsOnlyKey];
    NSArray* urls = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                               options:options];
    for (NSURL* url in urls) {
        if ([url isFileURL]) {
            return to_utf8([url path]);
        }
    }

    // Compatibility with applications still publishing the legacy filename list.
    NSArray* files = [pasteboard propertyListForType:NSFilenamesPboardType];
    if ([files count] > 0) {
        return to_utf8([files objectAtIndex:0]);
    }
    return {};
}

std::string materialize_image(NSPasteboard* pasteboard)
{
    NSData* data = [pasteboard dataForType:@"public.png"];
    if (data != nil) {
        return write_payload(data, "Dragged Image.png");
    }

    data = [pasteboard dataForType:NSTIFFPboardType];
    if (data != nil) {
        return write_payload(data, "Dragged Image.tiff");
    }
    return {};
}

std::string dragged_url(NSPasteboard* pasteboard)
{
    NSString* value = [pasteboard stringForType:@"public.url"];
    if (value == nil) {
        value = [pasteboard stringForType:NSURLPboardType];
    }
    if (value == nil) {
        value = [pasteboard stringForType:@"public.utf8-plain-text"];
    }

    const auto url = to_utf8(value);
    const auto shortcut = make_windows_internet_shortcut(url);
    if (shortcut.empty()) {
        return {};
    }

    NSString* title = [pasteboard stringForType:@"public.url-name"];
    auto filename = sanitize_drag_filename(to_utf8(title), "Dragged Link");
    std::string extension;
    if (filename.size() >= 4) {
        extension = filename.substr(filename.size() - 4);
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }
    if (extension != ".url") {
        filename += ".url";
    }

    NSData* data = [NSData dataWithBytes:shortcut.data() length:shortcut.size()];
    return write_payload(data, filename);
}

} // namespace

std::string getDraggedFilePath()
{
    NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];

    auto path = first_dragged_file(pasteboard);
    if (!path.empty()) {
        return path;
    }

    // Prefer actual image bytes over a source URL when both are present.
    path = materialize_image(pasteboard);
    if (!path.empty()) {
        return path;
    }

    return dragged_url(pasteboard);
}

} // namespace inputleap
