/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "inputleap/TransferHash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace inputleap {

namespace {

using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::string to_hex(const unsigned char* data, std::size_t size)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        stream << std::setw(2) << static_cast<unsigned int>(data[index]);
    }
    return stream.str();
}

DigestContext begin_sha256()
{
    DigestContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("failed to initialize SHA-256");
    }
    return context;
}

std::string finish_sha256(EVP_MD_CTX* context)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context, digest.data(), &length) != 1) {
        throw std::runtime_error("failed to finalize SHA-256");
    }
    return to_hex(digest.data(), length);
}

} // namespace

std::string sha256_bytes(const void* data, std::size_t size)
{
    auto context = begin_sha256();
    if (size != 0 &&
        EVP_DigestUpdate(context.get(), data, size) != 1) {
        throw std::runtime_error("failed to update SHA-256");
    }
    return finish_sha256(context.get());
}

std::string sha256_file(const fs::path& path)
{
    std::ifstream stream;
    open_utf8_path(stream, path, std::ios::in | std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("failed to open file for SHA-256");
    }

    auto context = begin_sha256();
    // Keep the transfer buffer off the thread stack. Windows worker threads
    // commonly have a 1 MiB stack, so a 1 MiB local array can overflow before
    // the first byte is hashed.
    std::vector<char> buffer(1024 * 1024);
    while (stream.good()) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        if (count > 0 &&
            EVP_DigestUpdate(context.get(), buffer.data(),
                             static_cast<std::size_t>(count)) != 1) {
            throw std::runtime_error("failed to update file SHA-256");
        }
    }
    if (!stream.eof()) {
        throw std::runtime_error("failed while reading file for SHA-256");
    }
    return finish_sha256(context.get());
}

std::string create_transfer_id()
{
    std::array<unsigned char, 16> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("failed to generate a transfer ID");
    }
    return to_hex(bytes.data(), bytes.size());
}

} // namespace inputleap
