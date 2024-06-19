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
    : QAbstractListModel(parent), m_isComplete(true)
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
    CalendarManager *manager = CalendarManager::instance(false);
    if (manager) {
        manager->cancelEventListRefresh(this);
    }
    qDeleteAll(m_events);
    m_events.clear();
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
    if (!m_isComplete)
        return;

    if (!m_identifiers.isEmpty()) {
        CalendarManager::instance()->scheduleEventListRefresh(this);
    }

    doRefresh();
}

void CalendarEventListModel::setIdentifiers(const QStringList &identifiers)
{
    if (identifiers == m_identifiers)
        return;

    m_identifiers = identifiers;
    emit identifiersChanged();

    refresh();
}

QStringList CalendarEventListModel::identifiers() const
{
    return m_identifiers;
}

QStringList CalendarEventListModel::missingItems() const
{
    return m_missingItems;
}

// For recurring events, if m_startTime is valid, the stored occurrence
// is the closest to m_startTime. When m_startTime is invalid, the
// initial occurrence is used.
QDateTime CalendarEventListModel::startTime() const
{
    return m_startTime;
}

void CalendarEventListModel::setStartTime(const QDateTime &time)
{
    if (time == m_startTime)
        return;
    m_startTime = time;
    emit startTimeChanged();

    doRefresh();
}

void CalendarEventListModel::resetStartTime()
{
    setStartTime(QDateTime());
}

bool CalendarEventListModel::loading() const
{
    return (m_events.count() + m_missingItems.count()) < m_identifiers.count();
}

int CalendarEventListModel::count() const
{
    return m_events.size();
}

int CalendarEventListModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return m_events.size();
}

void CalendarEventListModel::doRefresh()
{
    beginResetModel();
    qDeleteAll(m_events);
    m_events.clear();
    m_missingItems.clear();

    for (const QString &id : m_identifiers) {
        bool loaded;
        CalendarData::Event event = CalendarManager::instance()->getEvent(id, &loaded);
        if (event.isValid()) {
            CalendarEventOccurrence *occurrence =
                CalendarManager::instance()->getNextOccurrence
                (event.instanceId, m_startTime);
            if (occurrence->startTime().isValid()) {
                occurrence->setProperty("identifier", id);
                m_events.append(occurrence);
            } else {
                delete occurrence;
                m_missingItems.append(id);
            }
        } else if (loaded) {
            m_missingItems.append(id);
        }
    }

    if (m_startTime.isValid()) {
        std::sort(m_events.begin(), m_events.end(),
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

    if (index.row() < 0 || index.row() >= m_events.count()) {
        qWarning() << "CalendarEventListModel: Invalid index";
        return QVariant();
    }

    switch (role) {
    case EventObjectRole:
        return QVariant::fromValue<QObject *>(m_events.at(index.row())->eventObject());
    case OccurrenceObjectRole:
        return QVariant::fromValue<QObject *>(m_events.at(index.row()));
    case IdentifierRole:
        return m_events.at(index.row())->property("identifier");
    default:
        qWarning() << "CalendarEventListModel: Unknown role asked";
        return QVariant();
    }
}

void CalendarEventListModel::onTimezoneChanged()
{
    QList<CalendarEventOccurrence *>::ConstIterator it;
    for (it = m_events.constBegin(); it != m_events.constEnd(); it++) {
        // Actually, the date times have not changed, but
        // their representations in local time (as used in QML)
        // have changed.
        (*it)->startTimeChanged();
        (*it)->endTimeChanged();
    }
}

void CalendarEventListModel::classBegin()
{
    m_isComplete = false;
}

void CalendarEventListModel::componentComplete()
{
    m_isComplete = true;
    refresh();
}
