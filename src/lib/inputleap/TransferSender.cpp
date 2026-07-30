/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferSender.h"
#include "inputleap/TransferControl.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace inputleap {

TransferSender::TransferSender(TransferProgress* progress) :
    progress_(progress)
{
}

void TransferSender::wait_if_paused() const
{
    if (progress_ == nullptr) {
        return;
    }
    if (progress_->should_cancel()) {
        throw std::runtime_error("transfer was cancelled");
    }
    const auto now = std::chrono::steady_clock::now();
    if (last_control_check_ !=
            std::chrono::steady_clock::time_point{} &&
        now - last_control_check_ < std::chrono::milliseconds(40)) {
        return;
    }
    last_control_check_ = now;
    auto snapshot = progress_->snapshot();
    if (consume_transfer_control(snapshot.transfer_id,
                                 TransferControlAction::Cancel)) {
        progress_->cancel();
    }
    if (consume_transfer_control(snapshot.transfer_id,
                                 TransferControlAction::Pause)) {
        progress_->pause();
    }
    while (progress_->snapshot().state == TransferState::Paused) {
        snapshot = progress_->snapshot();
        if (consume_transfer_control(snapshot.transfer_id,
                                     TransferControlAction::Resume)) {
            progress_->resume();
            break;
        }
        if (consume_transfer_control(snapshot.transfer_id,
                                     TransferControlAction::Cancel)) {
            progress_->cancel();
        }
        if (progress_->should_cancel()) {
            throw std::runtime_error("transfer was cancelled");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (progress_->should_cancel()) {
        throw std::runtime_error("transfer was cancelled");
    }
}

void TransferSender::send(
    const TransferPlan& plan, const FrameSink& sink,
    const std::vector<std::uint64_t>& resume_offsets,
    const ResumeOffsetProvider& resume_provider,
    const CompletionWaiter& completion_waiter)
{
    std::string error;
    if (!plan.manifest.validate(&error) ||
        plan.sources.size() != plan.manifest.entries().size()) {
        throw std::invalid_argument(
            error.empty() ? "transfer plan is not aligned" : error);
    }
    if (!resume_offsets.empty() &&
        resume_offsets.size() != plan.manifest.entries().size()) {
        throw std::invalid_argument("transfer resume state is not aligned");
    }
    if (!sink) {
        throw std::invalid_argument("transfer frame sink is required");
    }

    if (progress_ != nullptr) {
        progress_->begin(plan.manifest);
    }

    try {
        sink({TransferFrameType::Manifest, plan.manifest.transfer_id(), 0, 0,
              plan.manifest.serialize()});

        auto active_resume_offsets = resume_offsets;
        if (resume_provider) {
            active_resume_offsets = resume_provider(plan.manifest);
            if (active_resume_offsets.size() !=
                plan.manifest.entries().size()) {
                throw std::runtime_error(
                    "received an invalid transfer resume state");
            }
        }

        std::vector<char> buffer(TransferFrame::kMaxChunkPayload);
        for (std::size_t index = 0;
             index < plan.manifest.entries().size(); ++index) {
            wait_if_paused();
            const auto& entry = plan.manifest.entries()[index];
            const auto resume_offset =
                active_resume_offsets.empty()
                    ? 0
                    : active_resume_offsets[index];
            if (resume_offset > entry.size) {
                throw std::invalid_argument("resume offset exceeds entry size");
            }

            if (progress_ != nullptr) {
                progress_->set_current_entry(index);
                progress_->add_bytes(resume_offset);
            }

            if (entry.kind != TransferEntryKind::Directory) {
                std::ifstream stream;
                open_utf8_path(stream, plan.sources[index],
                               std::ios::in | std::ios::binary);
                if (!stream.is_open()) {
                    throw std::runtime_error("failed to open transfer source");
                }
                stream.seekg(static_cast<std::streamoff>(resume_offset),
                             std::ios::beg);
                if (!stream.good()) {
                    throw std::runtime_error("failed to seek transfer source");
                }

                std::uint64_t offset = resume_offset;
                while (offset < entry.size) {
                    wait_if_paused();
                    const auto count = static_cast<std::size_t>((std::min)(
                        static_cast<std::uint64_t>(buffer.size()),
                        entry.size - offset));
                    stream.read(buffer.data(), static_cast<std::streamsize>(count));
                    if (static_cast<std::size_t>(stream.gcount()) != count) {
                        throw std::runtime_error("failed while reading transfer source");
                    }
                    sink({TransferFrameType::Chunk,
                          plan.manifest.transfer_id(),
                          static_cast<std::uint32_t>(index), offset,
                          std::string(buffer.data(), count)});
                    offset += count;
                    if (progress_ != nullptr) {
                        progress_->add_bytes(count);
                    }
                }
            }

            sink({TransferFrameType::EntryComplete,
                  plan.manifest.transfer_id(),
                  static_cast<std::uint32_t>(index), entry.size, {}});
            if (progress_ != nullptr) {
                progress_->finish_entry();
            }
        }

        sink({TransferFrameType::TransferComplete,
              plan.manifest.transfer_id(), 0, plan.manifest.total_size(), {}});
        if (completion_waiter) {
            completion_waiter(plan.manifest);
        }
        if (progress_ != nullptr) {
            progress_->complete();
        }
    }
    catch (const std::exception& exception) {
        if (progress_ != nullptr &&
            progress_->snapshot().state != TransferState::Cancelled) {
            progress_->fail(exception.what());
        }
        try {
            sink({TransferFrameType::Cancel, plan.manifest.transfer_id(), 0, 0, {}});
        }
        catch (...) {
        }
        throw;
    }
}

} // namespace inputleap
