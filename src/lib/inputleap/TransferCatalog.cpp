/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferCatalog.h"

#include "inputleap/DragPayload.h"
#include "inputleap/TransferHash.h"

#include <set>
#include <stdexcept>

namespace inputleap {

namespace {

std::string join_relative(const std::vector<std::string>& components)
{
    std::string result;
    for (const auto& component : components) {
        if (!result.empty()) {
            result += '/';
        }
        result += sanitize_drag_filename(component);
    }
    return result;
}

void check_entry_budget(const TransferManifest& manifest)
{
    if (manifest.entries().size() >= TransferManifest::kMaxEntries) {
        throw std::length_error("transfer contains too many entries");
    }
}

void add_file(TransferPlan& plan, const fs::path& path,
              const std::string& relative_path, TransferEntryKind kind)
{
    check_entry_budget(plan.manifest);
    TransferEntry entry;
    entry.kind = kind;
    entry.relative_path = relative_path;
    entry.size = static_cast<std::uint64_t>(fs::file_size(path));
    entry.sha256 = sha256_file(path);
    plan.manifest.entries().push_back(std::move(entry));
    plan.sources.push_back(path);
}

void add_directory(TransferPlan& plan, const std::string& relative_path)
{
    check_entry_budget(plan.manifest);
    TransferEntry entry;
    entry.kind = TransferEntryKind::Directory;
    entry.relative_path = relative_path;
    plan.manifest.entries().push_back(std::move(entry));
    plan.sources.emplace_back();
}

void add_path(TransferPlan& plan, const fs::path& source,
              const std::string& root_name)
{
    if (fs::is_symlink(source)) {
        throw std::invalid_argument("symbolic links cannot be transferred");
    }
    if (fs::is_regular_file(source)) {
        add_file(plan, source, root_name, TransferEntryKind::File);
        return;
    }
    if (!fs::is_directory(source)) {
        throw std::invalid_argument("transfer source is not a regular file or directory");
    }

    add_directory(plan, root_name);
    for (fs::recursive_directory_iterator iterator(source), end;
         iterator != end; ++iterator) {
        const auto& path = iterator->path();
        if (fs::is_symlink(path)) {
            throw std::invalid_argument("symbolic links cannot be transferred");
        }

        const auto relative = fs::relative(path, source);
        std::vector<std::string> components{root_name};
        for (const auto& component : relative) {
            components.push_back(component.u8string());
        }
        const auto safe_relative = join_relative(components);
        if (fs::is_directory(path)) {
            add_directory(plan, safe_relative);
        }
        else if (fs::is_regular_file(path)) {
            add_file(plan, path, safe_relative, TransferEntryKind::File);
        }
    }
}

} // namespace

TransferManifest TransferCatalog::from_paths(const std::vector<fs::path>& paths)
{
    return plan_from_paths(paths).manifest;
}

TransferPlan TransferCatalog::plan_from_paths(const std::vector<fs::path>& paths)
{
    if (paths.empty()) {
        throw std::invalid_argument("at least one transfer source is required");
    }

    TransferPlan plan;
    plan.manifest.set_transfer_id(create_transfer_id());
    std::set<std::string> root_names;

    for (const auto& source : paths) {
        if (!fs::exists(source)) {
            throw std::invalid_argument("transfer source does not exist");
        }
        auto root_name = sanitize_drag_filename(source.filename().u8string());
        auto candidate = root_name;
        unsigned suffix = 1;
        while (!root_names.insert(candidate).second) {
            candidate = root_name + " (" + std::to_string(suffix++) + ")";
        }
        add_path(plan, source, candidate);
    }

    std::string error;
    if (!plan.manifest.validate(&error)) {
        throw std::invalid_argument(error);
    }
    return plan;
}

} // namespace inputleap
