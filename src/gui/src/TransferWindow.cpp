/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "TransferWindow.h"

#include "common/DataDirectories.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int kMaximumHistory = 50;
constexpr int kTransferIdRole = Qt::UserRole + 1;

QString formattedSize(qulonglong bytes)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    return QLocale().formattedDataSize(
        static_cast<qint64>(bytes), 1, QLocale::DataSizeTraditionalFormat);
#else
    return QString::number(bytes) + QStringLiteral(" bytes");
#endif
}

QHash<QString, QString> parseFields(const QString& payload)
{
    QHash<QString, QString> fields;
    static const QRegularExpression fieldExpression(
        QStringLiteral("(\\w+)=([^\\s]*)"));
    auto matches = fieldExpression.globalMatch(payload);
    while (matches.hasNext()) {
        const auto match = matches.next();
        fields.insert(match.captured(1), match.captured(2));
    }
    return fields;
}

} // namespace

TransferWindow::TransferWindow(QSettings& settings, QWidget* parent) :
    QDialog(parent),
    settings_(settings),
    tree_(new QTreeWidget(this)),
    pauseButton_(new QPushButton(tr("Pause"), this)),
    cancelButton_(new QPushButton(tr("Cancel"), this)),
    retryButton_(new QPushButton(tr("Retry"), this)),
    showFolderButton_(new QPushButton(tr("Show in folder"), this)),
    clearButton_(new QPushButton(tr("Clear finished"), this))
{
    setWindowTitle(tr("Transfers"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(760, 360);

    tree_->setColumnCount(ColumnCount);
    tree_->setHeaderLabels(
        {tr("Direction"), tr("Item"), tr("Progress"), tr("Speed"), tr("Status")});
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->header()->setSectionResizeMode(ItemColumn, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(DirectionColumn,
                                          QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(ProgressColumn,
                                          QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(SpeedColumn,
                                          QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(StatusColumn,
                                          QHeaderView::ResizeToContents);

    auto* introduction = new QLabel(
        tr("Files, images, folders, and links shared between your computers."),
        this);
    introduction->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(pauseButton_, QDialogButtonBox::ActionRole);
    buttons->addButton(cancelButton_, QDialogButtonBox::ActionRole);
    buttons->addButton(retryButton_, QDialogButtonBox::ActionRole);
    buttons->addButton(showFolderButton_, QDialogButtonBox::ActionRole);
    buttons->addButton(clearButton_, QDialogButtonBox::ActionRole);
    auto* closeButton = buttons->addButton(QDialogButtonBox::Close);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(introduction);
    layout->addWidget(tree_, 1);
    layout->addWidget(buttons);

    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::hide);
    connect(cancelButton_, &QAbstractButton::clicked,
            this, &TransferWindow::cancelSelected);
    connect(pauseButton_, &QAbstractButton::clicked,
            this, &TransferWindow::pauseOrResumeSelected);
    connect(retryButton_, &QAbstractButton::clicked,
            this, &TransferWindow::retrySelected);
    connect(showFolderButton_, &QAbstractButton::clicked,
            this, &TransferWindow::showSelectedInFolder);
    connect(clearButton_, &QAbstractButton::clicked,
            this, &TransferWindow::clearFinished);
    connect(tree_, &QTreeWidget::itemSelectionChanged,
            this, &TransferWindow::updateButtons);

    loadHistory();
    updateButtons();
}

bool TransferWindow::processLogLine(const QString& line)
{
    const auto progressPosition =
        line.indexOf(QStringLiteral("TRANSFER_PROGRESS "));
    if (progressPosition >= 0) {
        const auto fields = parseFields(
            line.mid(progressPosition + QStringLiteral("TRANSFER_PROGRESS ").size()));
        const auto id = fields.value(QStringLiteral("id"));
        if (id.size() != 32) {
            return false;
        }

        auto& record = recordForId(id);
        const auto previousState = record.state;
        record.direction = fields.value(QStringLiteral("direction"));
        record.state = fields.value(QStringLiteral("state"));
        record.path = decodeField(fields.value(QStringLiteral("path")));
        record.error = decodeField(fields.value(QStringLiteral("error")));
        record.entries = fields.value(QStringLiteral("entries")).toInt();
        record.completedEntries =
            fields.value(QStringLiteral("completed")).toInt();
        record.transferred =
            fields.value(QStringLiteral("bytes")).toULongLong();
        record.total = fields.value(QStringLiteral("total")).toULongLong();
        record.speed = fields.value(QStringLiteral("speed")).toULongLong();
        updateRecord(record);

        if (previousState.isEmpty() && record.state == QStringLiteral("running")) {
            show();
            raise();
        }
        if (!isTerminal(previousState) && isTerminal(record.state)) {
            saveHistory();
            if (record.state == QStringLiteral("completed")) {
                Q_EMIT notificationRequested(
                    tr("Transfer complete"),
                    record.path.isEmpty()
                        ? tr("%1 item(s) transferred successfully.")
                              .arg(record.entries)
                        : tr("%1 transferred successfully.").arg(record.path));
            }
            else if (record.state == QStringLiteral("failed")) {
                Q_EMIT notificationRequested(
                    tr("Transfer failed"),
                    record.error.isEmpty() ? tr("The transfer could not be completed.")
                                           : record.error);
            }
        }
        updateButtons();
        return true;
    }

    const auto resultPosition = line.indexOf(QStringLiteral("TRANSFER_RESULT "));
    if (resultPosition >= 0) {
        const auto fields = parseFields(
            line.mid(resultPosition + QStringLiteral("TRANSFER_RESULT ").size()));
        const auto id = fields.value(QStringLiteral("id"));
        if (id.size() != 32) {
            return false;
        }
        auto& record = recordForId(id);
        record.destination =
            decodeField(fields.value(QStringLiteral("path")));
        updateRecord(record);
        saveHistory();
        updateButtons();
        return true;
    }
    return false;
}

QString TransferWindow::decodeField(const QString& value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}

QString TransferWindow::stateLabel(const QString& state)
{
    if (state == QStringLiteral("running")) return tr("Transferring");
    if (state == QStringLiteral("paused")) return tr("Paused");
    if (state == QStringLiteral("completed")) return tr("Completed");
    if (state == QStringLiteral("cancelled")) return tr("Cancelled");
    if (state == QStringLiteral("failed")) return tr("Failed");
    return tr("Pending");
}

QString TransferWindow::directionLabel(const QString& direction)
{
    return direction == QStringLiteral("receive") ? tr("Receiving")
                                                  : tr("Sending");
}

bool TransferWindow::isTerminal(const QString& state)
{
    return state == QStringLiteral("completed") ||
           state == QStringLiteral("cancelled") ||
           state == QStringLiteral("failed");
}

TransferWindow::TransferRecord* TransferWindow::selectedRecord()
{
    const auto selection = tree_->selectedItems();
    if (selection.isEmpty()) {
        return nullptr;
    }
    const auto id = selection.front()->data(0, kTransferIdRole).toString();
    auto iterator = records_.find(id);
    return iterator == records_.end() ? nullptr : &iterator.value();
}

TransferWindow::TransferRecord& TransferWindow::recordForId(const QString& id)
{
    auto iterator = records_.find(id);
    if (iterator != records_.end()) {
        return iterator.value();
    }
    TransferRecord record;
    record.id = id;
    record.item = new QTreeWidgetItem(tree_);
    record.item->setData(0, kTransferIdRole, id);
    tree_->insertTopLevelItem(0, record.item);
    records_.insert(id, record);
    return records_[id];
}

void TransferWindow::updateRecord(TransferRecord& record)
{
    if (record.item == nullptr) {
        return;
    }
    record.item->setText(DirectionColumn, directionLabel(record.direction));
    auto itemName = record.path;
    if (itemName.isEmpty()) {
        itemName = record.entries == 1
                       ? tr("1 item")
                       : tr("%1 items").arg(record.entries);
    }
    record.item->setText(ItemColumn, itemName);

    int percent = 0;
    if (record.total > 0) {
        percent = static_cast<int>(
            qMin<qulonglong>(100, record.transferred * 100 / record.total));
    }
    else if (record.state == QStringLiteral("completed")) {
        percent = 100;
    }
    record.item->setText(
        ProgressColumn,
        tr("%1% - %2 / %3")
            .arg(percent)
            .arg(formattedSize(record.transferred))
            .arg(formattedSize(record.total)));
    record.item->setText(
        SpeedColumn,
        record.speed == 0 ? QStringLiteral("-")
                          : tr("%1/s").arg(formattedSize(record.speed)));
    record.item->setText(
        StatusColumn,
        record.error.isEmpty()
            ? stateLabel(record.state)
            : tr("%1: %2").arg(stateLabel(record.state), record.error));
    record.item->setToolTip(ItemColumn,
                            record.destination.isEmpty()
                                ? record.path
                                : record.destination);
}

void TransferWindow::requestControl(const TransferRecord& record,
                                    const QString& action)
{
    const auto profile =
        QString::fromUtf8(inputleap::DataDirectories::profile().u8string().c_str());
    QDir directory(profile + QStringLiteral("/transfer-control"));
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return;
    }
    QFile request(directory.filePath(record.id + QStringLiteral(".") + action));
    if (request.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        request.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
        request.close();
    }
}

void TransferWindow::cancelSelected()
{
    if (const auto* record = selectedRecord()) {
        requestControl(*record, QStringLiteral("cancel"));
    }
}

void TransferWindow::pauseOrResumeSelected()
{
    if (const auto* record = selectedRecord()) {
        requestControl(*record,
                       record->state == QStringLiteral("paused")
                           ? QStringLiteral("resume")
                           : QStringLiteral("pause"));
    }
}

void TransferWindow::retrySelected()
{
    if (const auto* record = selectedRecord()) {
        requestControl(*record, QStringLiteral("retry"));
    }
}

void TransferWindow::showSelectedInFolder()
{
    const auto* record = selectedRecord();
    if (record == nullptr || record->destination.isEmpty()) {
        return;
    }
    QFileInfo information(record->destination);
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(information.isDir()
                                ? information.absoluteFilePath()
                                : information.absolutePath()));
}

void TransferWindow::clearFinished()
{
    QStringList removeIds;
    for (auto iterator = records_.cbegin(); iterator != records_.cend();
         ++iterator) {
        if (isTerminal(iterator.value().state)) {
            delete iterator.value().item;
            removeIds.append(iterator.key());
        }
    }
    for (const auto& id : removeIds) {
        records_.remove(id);
    }
    saveHistory();
    updateButtons();
}

void TransferWindow::updateButtons()
{
    const auto* record = selectedRecord();
    const bool active =
        record != nullptr && !isTerminal(record->state);
    pauseButton_->setEnabled(
        active && record->direction == QStringLiteral("send"));
    pauseButton_->setText(
        record != nullptr && record->state == QStringLiteral("paused")
            ? tr("Resume")
            : tr("Pause"));
    cancelButton_->setEnabled(active);
    retryButton_->setEnabled(
        record != nullptr &&
        record->direction == QStringLiteral("send") &&
        (record->state == QStringLiteral("failed") ||
         record->state == QStringLiteral("cancelled")));
    showFolderButton_->setEnabled(
        record != nullptr && !record->destination.isEmpty());
}

void TransferWindow::loadHistory()
{
    const int count = settings_.beginReadArray(QStringLiteral("transferHistory"));
    for (int index = 0; index < count; ++index) {
        settings_.setArrayIndex(index);
        const auto id = settings_.value(QStringLiteral("id")).toString();
        if (id.size() != 32) {
            continue;
        }
        auto& record = recordForId(id);
        record.direction =
            settings_.value(QStringLiteral("direction")).toString();
        record.path = settings_.value(QStringLiteral("path")).toString();
        record.destination =
            settings_.value(QStringLiteral("destination")).toString();
        record.state = settings_.value(QStringLiteral("state")).toString();
        record.error = settings_.value(QStringLiteral("error")).toString();
        record.transferred =
            settings_.value(QStringLiteral("transferred")).toULongLong();
        record.total = settings_.value(QStringLiteral("total")).toULongLong();
        record.entries = settings_.value(QStringLiteral("entries")).toInt();
        record.completedEntries =
            settings_.value(QStringLiteral("completedEntries")).toInt();
        updateRecord(record);
    }
    settings_.endArray();
}

void TransferWindow::saveHistory()
{
    settings_.beginWriteArray(QStringLiteral("transferHistory"));
    int index = 0;
    for (int row = 0; row < tree_->topLevelItemCount() &&
                      index < kMaximumHistory;
         ++row) {
        const auto id =
            tree_->topLevelItem(row)->data(0, kTransferIdRole).toString();
        const auto iterator = records_.constFind(id);
        if (iterator == records_.cend() ||
            !isTerminal(iterator.value().state)) {
            continue;
        }
        const auto& record = iterator.value();
        settings_.setArrayIndex(index++);
        settings_.setValue(QStringLiteral("id"), record.id);
        settings_.setValue(QStringLiteral("direction"), record.direction);
        settings_.setValue(QStringLiteral("path"), record.path);
        settings_.setValue(QStringLiteral("destination"), record.destination);
        settings_.setValue(QStringLiteral("state"), record.state);
        settings_.setValue(QStringLiteral("error"), record.error);
        settings_.setValue(QStringLiteral("transferred"), record.transferred);
        settings_.setValue(QStringLiteral("total"), record.total);
        settings_.setValue(QStringLiteral("entries"), record.entries);
        settings_.setValue(QStringLiteral("completedEntries"),
                           record.completedEntries);
    }
    settings_.endArray();
    settings_.sync();
}
