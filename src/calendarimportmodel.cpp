/*
 * Copyright (C) 2015 - 2019 Jolla Ltd.
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#include "calendarimportmodel.h"
#include "calendarimportevent.h"
#include "calendarutils.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>

// mkcal
#include <extendedcalendar.h>

// kcalendarcore
#include <KCalendarCore/MemoryCalendar>

CalendarImportModel::CalendarImportModel(QObject *parent)
    : QAbstractListModel(parent),
      m_error(false)
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = calendar->defaultStorage(calendar);
    if (!m_storage->open()) {
        qWarning() << "Unable to open calendar DB";
    }
}

CalendarImportModel::~CalendarImportModel()
{
}

int CalendarImportModel::count() const
{
    return m_eventList.count();
}

QString CalendarImportModel::fileName() const
{
    return m_fileName;
}

void CalendarImportModel::setFileName(const QString &fileName)
{
    if (m_fileName == fileName)
        return;

    m_fileName = fileName;
    emit fileNameChanged();
    setError(!importToMemory(m_fileName, m_icsRawData));
}

QString CalendarImportModel::icsString() const
{
    return QString::fromUtf8(m_icsRawData);
}

void CalendarImportModel::setIcsString(const QString &icsData)
{
    QByteArray data = icsData.toUtf8();
    if (m_icsRawData == data)
        return;

    m_icsRawData = data;
    emit icsStringChanged();
    setError(!importToMemory(m_fileName, m_icsRawData));
}

QString CalendarImportModel::notebookUid() const
{
    return m_notebookUid;
}

void CalendarImportModel::setNotebookUid(const QString &notebookUid)
{
    if (notebookUid == m_notebookUid)
        return;

    m_notebookUid = notebookUid;
    emit notebookUidChanged();

    setupDuplicates();
}

bool CalendarImportModel::hasDuplicates() const
{
    return !m_duplicates.isEmpty();
}

bool CalendarImportModel::hasInvitations() const
{
    return !m_invitations.isEmpty();
}

bool CalendarImportModel::error() const
{
    return m_error;
}

QObject *CalendarImportModel::getEvent(int index)
{
    if (index < 0 || index >= m_eventList.count())
        return 0;

    return new CalendarImportEvent(m_eventList.at(index));
}

int CalendarImportModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return m_eventList.count();
}

QVariant CalendarImportModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_eventList.count())
        return QVariant();

    KCalendarCore::Event::Ptr event = m_eventList.at(index.row());

    switch(role) {
    case DisplayLabelRole:
        return event->summary();
    case DescriptionRole:
        return event->description();
    case StartTimeRole:
        return event->dtStart();
    case EndTimeRole:
        return event->dtEnd();
    case AllDayRole:
        return event->allDay();
    case LocationRole:
        return event->location();
    case DuplicateRole:
        return m_duplicates.contains(event->instanceIdentifier());
    case InvitationRole:
        return m_invitations.contains(event->instanceIdentifier());
    case UidRole:
        return event->uid();
    default:
        return QVariant();
    }
}

bool CalendarImportModel::save(bool discardInvitation) const
{
    for (const KCalendarCore::Event::Ptr& incidence : m_eventList) {
        const KCalendarCore::Incidence::Ptr old =
            m_storage->calendar()->incidence(incidence->uid(), incidence->recurrenceId());
        if (old) {
            // Unconditionally overwrite existing incidence with the same UID/RecID.
            m_storage->calendar()->deleteIncidence(old);
        }
        if (discardInvitation) {
            incidence->setOrganizer(KCalendarCore::Person());
            incidence->clearAttendees();
        }
        m_storage->calendar().staticCast<mKCal::ExtendedCalendar>()->addIncidence(incidence, m_notebookUid);
    }

    return m_storage->save();
}

QHash<int, QByteArray> CalendarImportModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[DisplayLabelRole] = "displayLabel";
    roleNames[DescriptionRole] = "description";
    roleNames[StartTimeRole] = "startTime";
    roleNames[EndTimeRole] = "endTime";
    roleNames[AllDayRole] = "allDay";
    roleNames[LocationRole] = "location";
    roleNames[DuplicateRole] = "duplicate";
    roleNames[InvitationRole] = "invitation";
    roleNames[UidRole] = "uid";

    return roleNames;
}

void CalendarImportModel::setupDuplicates()
{
    m_duplicates.clear();
    if (!m_notebookUid.isEmpty()) {
        // To avoid detach here, use qAsConst when available.
        for (const KCalendarCore::Event::Ptr &event : m_eventList) {
            m_storage->load(event->uid());
            const KCalendarCore::Event::Ptr old =
                m_storage->calendar()->event(event->uid(), event->recurrenceId());
            if (old) {
                m_duplicates.insert(old->instanceIdentifier());
            }
        }
    }
    emit hasDuplicatesChanged();
}

bool CalendarImportModel::importToMemory(const QString &fileName, const QByteArray &icsData)
{
    bool success = false;

    KCalendarCore::MemoryCalendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    if (!fileName.isEmpty()) {
        success = CalendarUtils::importFromFile(fileName, cal);
    } else if (!icsData.isEmpty()) {
        success = CalendarUtils::importFromIcsRawData(icsData, cal);
    }

    beginResetModel();
    m_invitations.clear();
    m_eventList = cal->events(KCalendarCore::EventSortStartDate);
    // To avoid detach here, use qAsConst when available.
    for (const KCalendarCore::Event::Ptr &event : m_eventList) {
        if (!event->organizer().isEmpty()) {
            m_invitations.insert(event->instanceIdentifier());
        }
    }
    setupDuplicates();
    endResetModel();
    emit countChanged();
    emit hasInvitationsChanged();

    return success;
}

void CalendarImportModel::setError(bool error)
{
    if (error != m_error) {
        m_error = error;
        emit errorChanged();
    }
}

