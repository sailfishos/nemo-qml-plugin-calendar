/*
 * Copyright (C) 2013 - 2019 Jolla Ltd.
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

#include "calendareventoccurrence.h"

#include <QTimeZone>

#include "calendarevent.h"
#include "calendarmanager.h"

CalendarEventOccurrence::CalendarEventOccurrence(QObject *parent)
    : QObject(parent)
{
}

CalendarEventOccurrence::CalendarEventOccurrence(const CalendarData::EventOccurrence &occurrence,
                                                 QObject *parent)
    : QObject(parent)
    , m_instanceId(occurrence.instanceId)
    , m_startTime(occurrence.startTime)
    , m_endTime(occurrence.endTime)
{
    connect(CalendarManager::instance(), &CalendarManager::instanceIdChanged,
            this, &CalendarEventOccurrence::instanceIdChanged);
}

CalendarEventOccurrence::~CalendarEventOccurrence()
{
}

bool CalendarEventOccurrence::operator<(const CalendarEventOccurrence &other)
{
    return (m_startTime == other.m_startTime) ? (m_endTime < other.m_endTime) : (m_startTime < other.m_startTime);
}

QDateTime CalendarEventOccurrence::startTime() const
{
    return m_startTime;
}

QDateTime CalendarEventOccurrence::endTime() const
{
    return m_endTime;
}

CalendarStoredEvent *CalendarEventOccurrence::eventObject() const
{
    return CalendarManager::instance()->eventObject(m_instanceId);
}

void CalendarEventOccurrence::instanceIdChanged(QString oldId, QString newId, QString notebookUid)
{
    Q_UNUSED(notebookUid);

    if (m_instanceId == oldId)
        m_instanceId = newId;
}

static QDateTime toEventDateTime(const QDateTime &dateTime,
                                 Qt::TimeSpec eventSpec,
                                 const QString &eventTimezone)
{
    switch (eventSpec) {
    case (Qt::TimeZone): {
        const QDateTime dt = dateTime.toTimeZone(QTimeZone(eventTimezone.toUtf8()));
        return QDateTime(dt.date(), dt.time());
    }
    case (Qt::UTC): {
        const QDateTime dt = dateTime.toUTC();
        return QDateTime(dt.date(), dt.time());
    }
    default:
        return dateTime;
    }
}

QDateTime CalendarEventOccurrence::startTimeInTz() const
{
    const CalendarEvent *event = eventObject();
    return event ? toEventDateTime(m_startTime, event->startTimeSpec(), event->startTimeZone()) : m_startTime;
}

QDateTime CalendarEventOccurrence::endTimeInTz() const
{
    const CalendarEvent *event = eventObject();
    return event ? toEventDateTime(m_endTime, event->endTimeSpec(), event->endTimeZone()) : m_endTime;
}
