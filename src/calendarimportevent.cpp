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

CalendarImportEvent::CalendarImportEvent(const KCalendarCore::Event::Ptr &event)
    : CalendarEvent(event, nullptr)
    , mColor("#ffffff")
{
    if (event) {
        mOccurrence = CalendarUtils::getNextOccurrence(event);
    }
    mReadOnly = true;
}

QString CalendarImportEvent::color() const
{
    return mColor;
}

QString CalendarImportEvent::organizer() const
{
    return mIncidence->organizer().fullName();
}

QString CalendarImportEvent::organizerEmail() const
{
    return mIncidence->organizer().email();
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
    return new CalendarEventOccurrence(mOccurrence.eventUid,
                                       mOccurrence.recurrenceId,
                                       mOccurrence.startTime,
                                       mOccurrence.endTime);
}
