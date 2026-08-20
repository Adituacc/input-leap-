/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "ConnectionStatus.h"

#include <QStringList>

namespace {
const auto kStatusMarker = QStringLiteral("INPUTLEAP_STATUS|");
}

bool parseConnectionStatusLine(
    const QString& line, ConnectionStatusUpdate& update)
{
    const auto marker = line.indexOf(kStatusMarker);
    if (marker < 0) {
        return false;
    }

    const auto fields = line.mid(marker + kStatusMarker.size()).split('|');
    if (fields.size() < 2) {
        return false;
    }

    const auto state = fields[0].trimmed().toLower();
    if (state == QStringLiteral("connecting")) {
        update.state = AppConnectionState::CONNECTING;
    }
    else if (state == QStringLiteral("reconnecting")) {
        update.state = AppConnectionState::RECONNECTING;
    }
    else if (state == QStringLiteral("connected")) {
        update.state = AppConnectionState::CONNECTED;
    }
    else if (state == QStringLiteral("disconnected")) {
        update.state = AppConnectionState::DISCONNECTED;
    }
    else if (state == QStringLiteral("error")) {
        update.state = AppConnectionState::ERROR;
    }
    else if (state == QStringLiteral("transferring")) {
        update.state = AppConnectionState::TRANSFERRING;
    }
    else {
        return false;
    }

    update.detail = fields.mid(1).join(QStringLiteral("|")).trimmed();
    return true;
}

QString connectionStateTitle(AppConnectionState state)
{
    switch (state) {
    case AppConnectionState::CONNECTING: return QStringLiteral("Connecting");
    case AppConnectionState::RECONNECTING: return QStringLiteral("Reconnecting");
    case AppConnectionState::CONNECTED: return QStringLiteral("Connected");
    case AppConnectionState::TRANSFERRING: return QStringLiteral("Transferring");
    case AppConnectionState::ERROR: return QStringLiteral("Needs attention");
    case AppConnectionState::DISCONNECTED: return QStringLiteral("Disconnected");
    }
    return QStringLiteral("Disconnected");
}
