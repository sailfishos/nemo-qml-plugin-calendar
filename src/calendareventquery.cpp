/*
 * Copyright (c) 2013 - 2019 Jolla Ltd.
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

#include "calendareventquery.h"

#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "calendarutils.h"

#include <QDebug>

CalendarEventQuery::CalendarEventQuery()
    : mIsComplete(true), mOccurrence(0), mEventError(false)
{
    connect(CalendarManager::instance(), SIGNAL(dataUpdated()), this, SLOT(refresh()));
    connect(CalendarManager::instance(), SIGNAL(storageModified()), this, SLOT(refresh()));

    connect(CalendarManager::instance(), SIGNAL(eventUidChanged(QString,QString)),
            this, SLOT(eventUidChanged(QString,QString)));
}

CalendarEventQuery::~CalendarEventQuery()
{
    CalendarManager::instance()->cancelEventQueryRefresh(this);
}

// The uid of the matched event
QString CalendarEventQuery::uniqueId() const
{
    return mUid;
}

void CalendarEventQuery::setUniqueId(const QString &uid)
{
    if (uid == mUid)
        return;
    mUid = uid;
    emit uniqueIdChanged();

    if (mEvent) {
        mEvent.clear();
        emit eventChanged();
    }
    if (mOccurrence) {
        delete mOccurrence;
        mOccurrence = 0;
        emit occurrenceChanged();
    }

    refresh();
}

QString CalendarEventQuery::recurrenceIdString()
{
    if (mRecurrenceId.isValid()) {
        return CalendarUtils::recurrenceIdToString(mRecurrenceId);
    } else {
        return QString();
    }
}

void CalendarEventQuery::setRecurrenceIdString(const QString &recurrenceId)
{
    QDateTime recurrenceIdTime = QDateTime::fromString(recurrenceId, Qt::ISODate);
    if (mRecurrenceId == recurrenceIdTime) {
        return;
    }

    mRecurrenceId = recurrenceIdTime;
    emit recurrenceIdStringChanged();

    refresh();
}

// The ideal start time of the occurrence.  If there is no occurrence with
// the exact start time, the first occurrence following startTime is returned.
// If there is no following occurrence, the previous occurrence is returned.
QDateTime CalendarEventQuery::startTime() const
{
    return mStartTime;
}

void CalendarEventQuery::setStartTime(const QDateTime &t)
{
    if (t == mStartTime)
        return;
    mStartTime = t;
    emit startTimeChanged();

    refresh();
}

void CalendarEventQuery::resetStartTime()
{
    setStartTime(QDateTime());
}

QObject *CalendarEventQuery::event() const
{
    if (mEvent && mEvent->uid() == mUid)
        return CalendarManager::instance()->eventObject(mUid, mRecurrenceId);
    else
        return nullptr;
}

QObject *CalendarEventQuery::occurrence() const
{
    return mOccurrence;
}

QList<QObject*> CalendarEventQuery::attendees()
{
    CalendarStoredEvent *object = qobject_cast<CalendarStoredEvent*>(event());
    if (object)
        return object->attendees();
    else
        return {};
}

void CalendarEventQuery::classBegin()
{
    mIsComplete = false;
}

void CalendarEventQuery::componentComplete()
{
    mIsComplete = true;
    refresh();
}

void CalendarEventQuery::doRefresh(KCalendarCore::Incidence::Ptr event, bool eventError)
{
    // The value of mUid may have changed, verify that we got what we asked for
    if (event && (event->uid() != mUid || event->recurrenceId() != mRecurrenceId))
        return;

    bool updateOccurrence = false;
    bool signalEventChanged = false;

    KCalendarCore::Attendee::List oldAttendees;
    if (mEvent)
        oldAttendees = mEvent->attendees();

    if ((event && !mEvent) || (!event && mEvent)
        || (event && mEvent && event->uid() != mEvent->uid())
        || (event && mEvent && event->recurrenceId() != mEvent->recurrenceId())) {
        if (event) {
            mEvent = KCalendarCore::Incidence::Ptr(event->clone());
        } else {
            mEvent.clear();
        }
        signalEventChanged = true;
        updateOccurrence = true;
    } else if (mEvent && event) { // The event may have changed even if the pointer did not
        if (mEvent->allDay() != event->allDay()
            || mEvent->dateTime(KCalendarCore::Incidence::RoleEnd) != event->dateTime(KCalendarCore::Incidence::RoleEnd)
            || (mEvent->recurs() && !event->recurs())
            || (!mEvent->recurs() && event->recurs())
            || (mEvent->recurs() && event->recurs() && (*mEvent->recurrence() != *event->recurrence()))
            || mEvent->dtStart() != event->dtStart()) {
            if (event) {
                mEvent = KCalendarCore::Incidence::Ptr(event->clone());
            } else {
                mEvent.clear();
            }
            updateOccurrence = true;
        }
    }

    if (updateOccurrence) { // Err on the safe side: always update occurrence if it may have changed
        delete mOccurrence;
        mOccurrence = 0;

        if (mEvent) {
            CalendarEventOccurrence *occurrence = CalendarManager::instance()->getNextOccurrence(
                    mUid, mRecurrenceId, mStartTime);
            if (occurrence) {
                mOccurrence = occurrence;
                mOccurrence->setParent(this);
            }
        }
        emit occurrenceChanged();
    }

    if (signalEventChanged)
        emit eventChanged();

    // check if attendees have changed.
    KCalendarCore::Attendee::List attendees;
    if (event)
        attendees = event->attendees();
    if (oldAttendees != attendees) {
        emit attendeesChanged();
    }

    if (mEventError != eventError) {
        mEventError = eventError;
        emit eventErrorChanged();
    }
}

bool CalendarEventQuery::eventError() const
{
    return mEventError;
}

QDateTime CalendarEventQuery::recurrenceId()
{
    return mRecurrenceId;
}

void CalendarEventQuery::refresh()
{
    if (!mIsComplete || mUid.isEmpty())
        return;

    CalendarManager::instance()->scheduleEventQueryRefresh(this);
}

void CalendarEventQuery::eventUidChanged(QString oldUid, QString newUid)
{
    if (mUid == oldUid) {
        emit newUniqueId(newUid);
        refresh();
    }
}
