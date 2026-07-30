/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <QDialog>
#include <QHash>

class QAbstractButton;
class QPushButton;
class QSettings;
class QTreeWidget;
class QTreeWidgetItem;

class TransferWindow : public QDialog
{
    Q_OBJECT

public:
    explicit TransferWindow(QSettings& settings, QWidget* parent = nullptr);

    bool processLogLine(const QString& line);

Q_SIGNALS:
    void notificationRequested(const QString& title, const QString& message);

private Q_SLOTS:
    void cancelSelected();
    void pauseOrResumeSelected();
    void retrySelected();
    void showSelectedInFolder();
    void clearFinished();
    void updateButtons();

private:
    enum Column {
        DirectionColumn,
        ItemColumn,
        ProgressColumn,
        SpeedColumn,
        StatusColumn,
        ColumnCount
    };

    struct TransferRecord {
        QString id;
        QString direction;
        QString path;
        QString destination;
        QString state;
        QString error;
        qulonglong transferred = 0;
        qulonglong total = 0;
        qulonglong speed = 0;
        int entries = 0;
        int completedEntries = 0;
        QTreeWidgetItem* item = nullptr;
    };

    static QString decodeField(const QString& value);
    static QString stateLabel(const QString& state);
    static QString directionLabel(const QString& direction);
    static bool isTerminal(const QString& state);

    TransferRecord* selectedRecord();
    TransferRecord& recordForId(const QString& id);
    void updateRecord(TransferRecord& record);
    void requestControl(const TransferRecord& record, const QString& action);
    void loadHistory();
    void saveHistory();

    QSettings& settings_;
    QTreeWidget* tree_;
    QPushButton* pauseButton_;
    QPushButton* cancelButton_;
    QPushButton* retryButton_;
    QPushButton* showFolderButton_;
    QPushButton* clearButton_;
    QHash<QString, TransferRecord> records_;
};
