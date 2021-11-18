/*
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2020 - 2021 Open Mobile Platform LLC.
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

#include "calendarimportevent.h"

#include "calendareventoccurrence.h"
#include "calendarutils.h"
#include "calendarmanager.h"

#include <QDebug>

CalendarImportEvent::CalendarImportEvent(KCalendarCore::Event::Ptr event)
    : QObject(),
      mEvent(event),
      mColor("#ffffff")
{
}

QString CalendarImportEvent::displayLabel() const
{
    if (!mEvent)
        return QString();

    return mEvent->summary();
}

QString CalendarImportEvent::description() const
{
    if (!mEvent)
        return QString();

    return mEvent->description();
}

QDateTime CalendarImportEvent::startTime() const
{
    if (!mEvent)
        return QDateTime();

    return mEvent->dtStart();
}

QDateTime CalendarImportEvent::endTime() const
{
    if (!mEvent)
        return QDateTime();

    return mEvent->dtEnd();
}

bool CalendarImportEvent::allDay() const
{
    if (!mEvent)
        return false;

    return mEvent->allDay();
}

CalendarEvent::Recur CalendarImportEvent::recur()
{
    if (!mEvent)
        return CalendarEvent::RecurOnce;

    return CalendarUtils::convertRecurrence(mEvent);
}

CalendarEvent::Days CalendarImportEvent::recurWeeklyDays()
{
    if (!mEvent)
        return CalendarEvent::NoDays;

    return CalendarUtils::convertDayPositions(mEvent);
}

int CalendarImportEvent::reminder() const
{
    // note: returns seconds before event, so 15 minutes before = 900.
    //       zero value means "reminder at time of the event".
    //       negative value means "no reminder".
    return CalendarUtils::getReminder(mEvent);
}

QDateTime CalendarImportEvent::reminderDateTime() const
{
    return mEvent ? CalendarUtils::getReminderDateTime(mEvent) : QDateTime();
}

QString CalendarImportEvent::uniqueId() const
{
    if (!mEvent)
        return QString();

    return mEvent->uid();
}

QString CalendarImportEvent::color() const
{
    return mColor;
}

bool CalendarImportEvent::readOnly() const
{
    return true;
}

QString CalendarImportEvent::location() const
{
    if (!mEvent)
        return QString();

    return mEvent->location();
}

QList<QObject *> CalendarImportEvent::attendees() const
{
    if (!mEvent)
        return QList<QObject *>();

    return CalendarUtils::convertAttendeeList(CalendarUtils::getEventAttendees(mEvent));
}

CalendarEvent::Secrecy CalendarImportEvent::secrecy() const
{
    if (!mEvent)
        return CalendarEvent::SecrecyPublic;

    return CalendarUtils::convertSecrecy(mEvent);
}

QString CalendarImportEvent::organizer() const
{
    if (!mEvent)
        return QString();

    return mEvent->organizer().fullName();
}

QString CalendarImportEvent::organizerEmail() const
{
    if (!mEvent)
        return QString();

    return mEvent->organizer().email();
}

void CalendarImportEvent::setColor(const QString &color)
{
    if (mColor == color)
        return;

    mColor = color;
    emit colorChanged();
}

CalendarEvent::Response CalendarImportEvent::ownerStatus() const
{
    return CalendarEvent::ResponseUnspecified;
}

bool CalendarImportEvent::rsvp() const
{
    return false;
}

bool CalendarImportEvent::sendResponse(int response)
{
    Q_UNUSED(response)
    return false;
}

QObject *CalendarImportEvent::nextOccurrence()
{
    if (!mEvent)
        return 0;

    return new CalendarEventOccurrence(CalendarUtils::getNextOccurrence(mEvent));
}
