/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#include "DiagnosticBundle.h"

#include <gtest/gtest.h>

#include <QDir>

TEST(DiagnosticBundleTests, removesAddressesFingerprintsAndHomePath)
{
    const auto input = QStringLiteral(
        "connect 192.168.1.10 fingerprint "
        "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99 path %1/private")
                           .arg(QDir::homePath());
    const auto output = sanitizeDiagnosticText(input);
    EXPECT_FALSE(output.contains(QStringLiteral("192.168.1.10")));
    EXPECT_FALSE(output.contains(QStringLiteral("AA:BB:CC")));
    EXPECT_FALSE(output.contains(QDir::homePath()));
    EXPECT_TRUE(output.contains(QStringLiteral("[ip-address]")));
    EXPECT_TRUE(output.contains(QStringLiteral("[fingerprint]")));
}
