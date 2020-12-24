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

CalendarEventModification::CalendarEventModification(CalendarData::Event data, QObject *parent)
    : QObject(parent), m_event(data), m_attendeesSet(false)
{
}

CalendarEventModification::CalendarEventModification(QObject *parent)
    : QObject(parent), m_attendeesSet(false)
{
    m_event.recur = CalendarEvent::RecurOnce;
    m_event.reminder = -1; // ReminderNone
    m_event.allDay = false;
    m_event.readOnly = false;
}

CalendarEventModification::~CalendarEventModification()
{
}

QString CalendarEventModification::displayLabel() const
{
    return m_event.displayLabel;
}

void CalendarEventModification::setDisplayLabel(const QString &displayLabel)
{
    if (m_event.displayLabel != displayLabel) {
        m_event.displayLabel = displayLabel;
        emit displayLabelChanged();
    }
}

QString CalendarEventModification::description() const
{
    return m_event.description;
}

void CalendarEventModification::setDescription(const QString &description)
{
    if (m_event.description != description) {
        m_event.description = description;
        emit descriptionChanged();
    }
}

QDateTime CalendarEventModification::startTime() const
{
    return m_event.startTime;
}

void CalendarEventModification::setStartTime(const QDateTime &startTime, Qt::TimeSpec spec, const QString &timezone)
{
    QDateTime newStartTimeInTz = startTime;
    updateTime(&newStartTimeInTz, spec, timezone);
    if (m_event.startTime != newStartTimeInTz) {
        m_event.startTime = newStartTimeInTz;
        emit startTimeChanged();
    }
}

QDateTime CalendarEventModification::endTime() const
{
    return m_event.endTime;
}

void CalendarEventModification::setEndTime(const QDateTime &endTime, Qt::TimeSpec spec, const QString &timezone)
{
    QDateTime newEndTimeInTz = endTime;
    updateTime(&newEndTimeInTz, spec, timezone);
    if (m_event.endTime != newEndTimeInTz) {
        m_event.endTime = newEndTimeInTz;
        emit endTimeChanged();
    }
}

bool CalendarEventModification::allDay() const
{
    return m_event.allDay;
}

void CalendarEventModification::setAllDay(bool allDay)
{
    if (m_event.allDay != allDay) {
        m_event.allDay = allDay;
        emit allDayChanged();
    }
}

CalendarEvent::Recur CalendarEventModification::recur() const
{
    return m_event.recur;
}

void CalendarEventModification::setRecur(CalendarEvent::Recur recur)
{
    if (m_event.recur != recur) {
        m_event.recur = recur;
        emit recurChanged();
    }
}

QDateTime CalendarEventModification::recurEndDate() const
{
    return QDateTime(m_event.recurEndDate);
}

bool CalendarEventModification::hasRecurEndDate() const
{
    return m_event.recurEndDate.isValid();
}

void CalendarEventModification::setRecurEndDate(const QDateTime &dateTime)
{
    bool wasValid = m_event.recurEndDate.isValid();
    QDate date = dateTime.date();

    if (m_event.recurEndDate != date) {
        m_event.recurEndDate = date;
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

CalendarEvent::Days CalendarEventModification::recurWeeklyDays() const
{
    return m_event.recurWeeklyDays;
}

void CalendarEventModification::setRecurWeeklyDays(CalendarEvent::Days days)
{
    if (m_event.recurWeeklyDays != days) {
        m_event.recurWeeklyDays = days;
        emit recurWeeklyDaysChanged();
    }
}

QString CalendarEventModification::recurrenceIdString() const
{
    if (m_event.recurrenceId.isValid()) {
        return CalendarUtils::recurrenceIdToString(m_event.recurrenceId);
    } else {
        return QString();
    }
}

int CalendarEventModification::reminder() const
{
    return m_event.reminder;
}

void CalendarEventModification::setReminder(int seconds)
{
    if (seconds != m_event.reminder) {
        m_event.reminder = seconds;
        emit reminderChanged();
    }
}

QString CalendarEventModification::location() const
{
    return m_event.location;
}

void CalendarEventModification::setLocation(const QString &newLocation)
{
    if (newLocation != m_event.location) {
        m_event.location = newLocation;
        emit locationChanged();
    }
}

QString CalendarEventModification::calendarUid() const
{
    return m_event.calendarUid;
}

void CalendarEventModification::setCalendarUid(const QString &uid)
{
    if (m_event.calendarUid != uid) {
        m_event.calendarUid = uid;
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
    CalendarManager::instance()->saveModification(m_event, m_attendeesSet,
                                                  m_requiredAttendees, m_optionalAttendees);
}

CalendarChangeInformation *
CalendarEventModification::replaceOccurrence(CalendarEventOccurrence *occurrence)
{
    return CalendarManager::instance()->replaceOccurrence(m_event, occurrence, m_attendeesSet,
                                                          m_requiredAttendees, m_optionalAttendees);
}
