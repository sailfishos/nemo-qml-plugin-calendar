/*
 * Copyright (C) 2021 Damien Caliste <dcaliste@free.fr>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "calendareventlistmodel.h"

#include "calendarmanager.h"
#include "calendareventoccurrence.h"

#include <QDebug>

CalendarEventListModel::CalendarEventListModel(QObject *parent)
    : QAbstractListModel(parent), mIsComplete(true)
{
    connect(CalendarManager::instance(), &CalendarManager::storageModified,
            this, &CalendarEventListModel::refresh);
    connect(CalendarManager::instance(), &CalendarManager::dataUpdated,
            this, &CalendarEventListModel::doRefresh);
    connect(CalendarManager::instance(), &CalendarManager::timezoneChanged,
            this, &CalendarEventListModel::onTimezoneChanged);
}

CalendarEventListModel::~CalendarEventListModel()
{
    CalendarManager::instance()->cancelEventListRefresh(this);
    qDeleteAll(mEvents);
    mEvents.clear();
}

QHash<int, QByteArray> CalendarEventListModel::roleNames() const
{
    QHash<int,QByteArray> roleNames;
    roleNames[EventObjectRole] = "event";
    roleNames[OccurrenceObjectRole] = "occurrence";
    roleNames[IdentifierRole] = "identifier";
    return roleNames;
}

void CalendarEventListModel::refresh()
{
    if (!mIsComplete)
        return;

    if (!mIdentifiers.isEmpty()) {
        CalendarManager::instance()->scheduleEventListRefresh(this);
    }

    doRefresh();
}

void CalendarEventListModel::setIdentifiers(const QStringList &identifiers)
{
    if (identifiers == mIdentifiers)
        return;

    mIdentifiers = identifiers;
    emit identifiersChanged();

    refresh();
}

QStringList CalendarEventListModel::identifiers() const
{
    return mIdentifiers;
}

QStringList CalendarEventListModel::missingItems() const
{
    return mMissingItems;
}

// For recurring events, if mStartTime is valid, the stored occurrence
// is the closest to mStartTime. When mStartTime is invalid, the
// initial occurrence is used.
QDateTime CalendarEventListModel::startTime() const
{
    return mStartTime;
}

void CalendarEventListModel::setStartTime(const QDateTime &time)
{
    if (time == mStartTime)
        return;
    mStartTime = time;
    emit startTimeChanged();

    doRefresh();
}

void CalendarEventListModel::resetStartTime()
{
    setStartTime(QDateTime());
}

bool CalendarEventListModel::loading() const
{
    return (mEvents.count() + mMissingItems.count()) < mIdentifiers.count();
}

int CalendarEventListModel::count() const
{
    return mEvents.size();
}

int CalendarEventListModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return mEvents.size();
}

void CalendarEventListModel::doRefresh()
{
    beginResetModel();
    qDeleteAll(mEvents);
    mEvents.clear();
    mMissingItems.clear();

    for (const QString &id : mIdentifiers) {
        bool loaded;
        CalendarData::Event event = CalendarManager::instance()->getEvent(id, &loaded);
        if (event.isValid()) {
            CalendarEventOccurrence *occurrence;
            if (event.recur != CalendarEvent::RecurOnce && mStartTime.isValid()) {
                occurrence = CalendarManager::instance()->getNextOccurrence
                    (event.uniqueId, event.recurrenceId, mStartTime);
            } else {
                occurrence = new CalendarEventOccurrence(event.uniqueId, event.recurrenceId,
                                                         event.startTime, event.endTime, this);
            }
            if (occurrence->startTime().isValid()) {
                occurrence->setProperty("identifier", id);
                mEvents.append(occurrence);
            } else {
                delete occurrence;
                mMissingItems.append(id);
            }
        } else if (loaded) {
            mMissingItems.append(id);
        }
    }

    if (mStartTime.isValid()) {
        std::sort(mEvents.begin(), mEvents.end(),
                  [](CalendarEventOccurrence *a, CalendarEventOccurrence *b) {
                      // Inverse sort: earlier first
                      return a && b && (*b < *a);
                  });
    }

    endResetModel();
    emit countChanged();
    emit missingItemsChanged();
    emit loadingChanged();
}

QVariant CalendarEventListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() < 0 || index.row() >= mEvents.count()) {
        qWarning() << "CalendarEventListModel: Invalid index";
        return QVariant();
    }

    switch (role) {
    case EventObjectRole:
        return QVariant::fromValue<QObject *>(mEvents.at(index.row())->eventObject());
    case OccurrenceObjectRole:
        return QVariant::fromValue<QObject *>(mEvents.at(index.row()));
    case IdentifierRole:
        return mEvents.at(index.row())->property("identifier");
    default:
        qWarning() << "CalendarEventListModel: Unknown role asked";
        return QVariant();
    }
}

void CalendarEventListModel::onTimezoneChanged()
{
    QList<CalendarEventOccurrence *>::ConstIterator it;
    for (it = mEvents.constBegin(); it != mEvents.constEnd(); it++) {
        // Actually, the date times have not changed, but
        // their representations in local time (as used in QML)
        // have changed.
        (*it)->startTimeChanged();
        (*it)->endTimeChanged();
    }
}

void CalendarEventListModel::classBegin()
{
    mIsComplete = false;
}

void CalendarEventListModel::componentComplete()
{
    mIsComplete = true;
    refresh();
}
