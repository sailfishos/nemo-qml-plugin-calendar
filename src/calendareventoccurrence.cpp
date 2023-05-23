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

CalendarEventOccurrence::CalendarEventOccurrence(const QString &instanceId,
                                                 const QDateTime &startTime,
                                                 const QDateTime &endTime,
                                                 QObject *parent)
    : QObject(parent), mInstanceId(instanceId), mStartTime(startTime), mEndTime(endTime)
{
    connect(CalendarManager::instance(), SIGNAL(eventUidChanged(QString,QString)),
            this, SLOT(eventUidChanged(QString,QString)));
}

CalendarEventOccurrence::~CalendarEventOccurrence()
{
}

bool CalendarEventOccurrence::operator<(const CalendarEventOccurrence &other)
{
    return (mStartTime == other.mStartTime) ? (mEndTime < other.mEndTime) : (mStartTime < other.mStartTime);
}

QDateTime CalendarEventOccurrence::startTime() const
{
    return mStartTime;
}

QDateTime CalendarEventOccurrence::endTime() const
{
    return mEndTime;
}

CalendarStoredEvent *CalendarEventOccurrence::eventObject() const
{
    return CalendarManager::instance()->eventObject(mInstanceId);
}

void CalendarEventOccurrence::eventUidChanged(QString oldUid, QString newUid)
{
    if (mInstanceId == oldUid)
        mInstanceId = newUid;
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
    return event ? toEventDateTime(mStartTime, event->startTimeSpec(), event->startTimeZone()) : mStartTime;
}

QDateTime CalendarEventOccurrence::endTimeInTz() const
{
    const CalendarEvent *event = eventObject();
    return event ? toEventDateTime(mEndTime, event->endTimeSpec(), event->endTimeZone()) : mEndTime;
}
