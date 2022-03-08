/*
 * Copyright (c) 2014 - 2019 Jolla Ltd.
 * Copyright (c) 2020 Open Mobile Platform LLC.
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

#include "calendareventmodification.h"
#include "calendarmanager.h"
#include "calendarutils.h"

#include <QTimeZone>
#include <QDebug>

namespace {

void updateTime(QDateTime *dt, Qt::TimeSpec spec, const QString &timeZone)
{
    if (spec == Qt::TimeZone) {
        QTimeZone tz(timeZone.toUtf8());
        if (tz.isValid()) {
            dt->setTimeZone(tz);
        } else {
            qWarning() << "Cannot find time zone:" << timeZone;
        }
    } else {
        dt->setTimeSpec(spec);
    }
}

}

CalendarEventModification::CalendarEventModification(const CalendarStoredEvent *source, QObject *parent)
    : CalendarEvent(source, parent)
{
}

CalendarEventModification::CalendarEventModification(QObject *parent)
    : CalendarEvent((CalendarData::Event*)0, parent)
{
}

CalendarEventModification::~CalendarEventModification()
{
}

QDateTime CalendarEventModification::startTime() const
{
    return mData->startTime;
}

QDateTime CalendarEventModification::endTime() const
{
    return mData->endTime;
}

void CalendarEventModification::setDisplayLabel(const QString &displayLabel)
{
    if (mData->displayLabel != displayLabel) {
        mData->displayLabel = displayLabel;
        emit displayLabelChanged();
    }
}

void CalendarEventModification::setDescription(const QString &description)
{
    if (mData->description != description) {
        mData->description = description;
        emit descriptionChanged();
    }
}

void CalendarEventModification::setStartTime(const QDateTime &startTime, Qt::TimeSpec spec, const QString &timezone)
{
    QDateTime newStartTimeInTz = startTime;
    updateTime(&newStartTimeInTz, spec, timezone);
    if (mData->startTime != newStartTimeInTz
        || mData->startTime.timeSpec() != newStartTimeInTz.timeSpec()
        || (mData->startTime.timeSpec() == Qt::TimeZone
            && mData->startTime.timeZone() != newStartTimeInTz.timeZone())) {
        mData->startTime = newStartTimeInTz;
        emit startTimeChanged();
    }
}

void CalendarEventModification::setEndTime(const QDateTime &endTime, Qt::TimeSpec spec, const QString &timezone)
{
    QDateTime newEndTimeInTz = endTime;
    updateTime(&newEndTimeInTz, spec, timezone);
    if (mData->endTime != newEndTimeInTz
        || mData->endTime.timeSpec() != newEndTimeInTz.timeSpec()
        || (mData->endTime.timeSpec() == Qt::TimeZone
            && mData->endTime.timeZone() != newEndTimeInTz.timeZone())) {
        mData->endTime = newEndTimeInTz;
        emit endTimeChanged();
    }
}

void CalendarEventModification::setAllDay(bool allDay)
{
    if (mData->allDay != allDay) {
        mData->allDay = allDay;
        emit allDayChanged();
    }
}

void CalendarEventModification::setRecur(CalendarEvent::Recur recur)
{
    if (mData->recur != recur) {
        mData->recur = recur;
        emit recurChanged();
    }
}

void CalendarEventModification::setRecurEndDate(const QDateTime &dateTime)
{
    bool wasValid = mData->recurEndDate.isValid();
    QDate date = dateTime.date();

    if (mData->recurEndDate != date) {
        mData->recurEndDate = date;
        emit recurEndDateChanged();

        if (date.isValid() != wasValid) {
            emit hasRecurEndDateChanged();
        }
    }
}

void CalendarEventModification::unsetRecurEndDate()
{
    setRecurEndDate(QDateTime());
}

void CalendarEventModification::setRecurWeeklyDays(CalendarEvent::Days days)
{
    if (mData->recurWeeklyDays != days) {
        mData->recurWeeklyDays = days;
        emit recurWeeklyDaysChanged();
    }
}

void CalendarEventModification::setReminder(int seconds)
{
    if (seconds != mData->reminder) {
        mData->reminder = seconds;
        emit reminderChanged();
    }
}

void CalendarEventModification::setReminderDateTime(const QDateTime &dateTime)
{
    if (dateTime != mData->reminderDateTime) {
        mData->reminderDateTime = dateTime;
        emit reminderDateTimeChanged();
    }
}

void CalendarEventModification::setLocation(const QString &newLocation)
{
    if (newLocation != mData->location) {
        mData->location = newLocation;
        emit locationChanged();
    }
}

void CalendarEventModification::setCalendarUid(const QString &uid)
{
    if (mData->calendarUid != uid) {
        mData->calendarUid = uid;
        emit calendarUidChanged();
    }
}

void CalendarEventModification::setAttendees(CalendarContactModel *required, CalendarContactModel *optional)
{
    if (!required || !optional) {
        qWarning() << "Missing attendeeList";
        return;
    }

    m_attendeesSet = true;
    m_requiredAttendees = required->getList();
    m_optionalAttendees = optional->getList();
}

void CalendarEventModification::save()
{
    CalendarManager::instance()->saveModification(*mData, m_attendeesSet,
                                                  m_requiredAttendees, m_optionalAttendees);
}

CalendarChangeInformation *
CalendarEventModification::replaceOccurrence(CalendarEventOccurrence *occurrence)
{
    return CalendarManager::instance()->replaceOccurrence(*mData, occurrence, m_attendeesSet,
                                                          m_requiredAttendees, m_optionalAttendees);
}

void CalendarEventModification::setSyncFailureResolution(CalendarEvent::SyncFailureResolution resolution)
{
    if (mData->syncFailureResolution != resolution) {
        mData->syncFailureResolution = resolution;
        emit syncFailureResolutionChanged();
    }
}

