/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#pragma once

#include "AppConnectionState.h"

#include <QString>

QString sanitizeDiagnosticText(const QString& text);

QString createDiagnosticReport(
    AppConnectionState state, const QString& role,
    bool automaticDiscovery, bool encryptionEnabled,
    const QString& logs);
