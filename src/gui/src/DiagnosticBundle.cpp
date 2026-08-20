/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "DiagnosticBundle.h"
#include "ConnectionStatus.h"

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QSysInfo>

QString sanitizeDiagnosticText(const QString& text)
{
    auto result = text;
    const auto home = QDir::homePath();
    if (!home.isEmpty()) {
        result.replace(home, QStringLiteral("~"), Qt::CaseInsensitive);
    }

    result.replace(
        QRegularExpression(QStringLiteral(
            R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)")),
        QStringLiteral("[ip-address]"));
    result.replace(
        QRegularExpression(QStringLiteral(
            R"(\b(?:[A-Fa-f0-9]{2}:){15,63}[A-Fa-f0-9]{2}\b)")),
        QStringLiteral("[fingerprint]"));
    return result;
}

QString createDiagnosticReport(
    AppConnectionState state, const QString& role,
    bool automaticDiscovery, bool encryptionEnabled,
    const QString& logs)
{
    QString report;
    report += QStringLiteral("Input Leap diagnostic report\n");
    report += QStringLiteral("Generated: %1\n")
                  .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    report += QStringLiteral("Operating system: %1 (%2)\n")
                  .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    report += QStringLiteral("Kernel: %1 %2\n")
                  .arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    report += QStringLiteral("Qt: %1\n").arg(QString::fromLatin1(qVersion()));
    report += QStringLiteral("Role: %1\n").arg(role);
    report += QStringLiteral("State: %1\n").arg(connectionStateTitle(state));
    report += QStringLiteral("Automatic discovery: %1\n")
                  .arg(automaticDiscovery ? QStringLiteral("enabled")
                                          : QStringLiteral("disabled"));
    report += QStringLiteral("Encryption: %1\n")
                  .arg(encryptionEnabled ? QStringLiteral("enabled")
                                         : QStringLiteral("disabled"));
    report += QStringLiteral("\nSanitized recent log\n--------------------\n");
    report += sanitizeDiagnosticText(logs);
    return report;
}
