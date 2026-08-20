/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#pragma once

#include "AppConnectionState.h"

#include <QString>

struct ConnectionStatusUpdate {
    AppConnectionState state = AppConnectionState::DISCONNECTED;
    QString detail;
};

bool parseConnectionStatusLine(
    const QString& line, ConnectionStatusUpdate& update);

QString connectionStateTitle(AppConnectionState state);
