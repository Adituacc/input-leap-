/*
    InputLeap -- mouse and keyboard sharing utility
    Copyright (C) InputLeap contributors

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/

#include "AppConfig.h"

#include <QSettings>
#include <QTemporaryDir>
#include <gtest/gtest.h>

TEST(AppConfigTests, NewProfileUsesAuthenticatedEncryption)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
    AppConfig config(&settings);

    EXPECT_TRUE(config.getCryptoEnabled());
    EXPECT_TRUE(config.getRequireClientCertificate());
}

TEST(AppConfigTests, ExplicitLegacySecuritySettingsArePreserved)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
    settings.setValue("cryptoEnabled", false);
    settings.setValue("requireClientCertificate", false);

    AppConfig config(&settings);

    EXPECT_FALSE(config.getCryptoEnabled());
    EXPECT_FALSE(config.getRequireClientCertificate());
}
