/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 */

#include "UpdateChecker.h"

#include <QRegularExpression>
#include <array>

namespace {

std::array<int, 3> versionParts(const QString& version)
{
    const QRegularExpression expression(
        QStringLiteral(R"((\d+)\.(\d+)\.(\d+))"));
    const auto match = expression.match(version);
    if (!match.hasMatch()) {
        return {0, 0, 0};
    }
    return {match.captured(1).toInt(), match.captured(2).toInt(),
            match.captured(3).toInt()};
}

} // namespace

bool isNewerVersion(const QString& candidate, const QString& current)
{
    return versionParts(candidate) > versionParts(current);
}
