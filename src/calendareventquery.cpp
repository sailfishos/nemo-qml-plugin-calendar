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
    : mIsComplete(true), mOccurrence(0), mAttendeesCached(false), mEventError(false), mUpdateOccurrence(false)
{
    connect(CalendarManager::instance(), SIGNAL(dataUpdated()), this, SLOT(refresh()));
    connect(CalendarManager::instance(), SIGNAL(storageModified()), this, SLOT(refresh()));
    connect(CalendarManager::instance(), &CalendarManager::timezoneChanged,
            this, &CalendarEventQuery::onTimezoneChanged);

    connect(CalendarManager::instance(), SIGNAL(eventUidChanged(QString,QString)),
            this, SLOT(eventUidChanged(QString,QString)));
}

CalendarEventQuery::~CalendarEventQuery()
{
    CalendarManager *manager = CalendarManager::instance(false);
    if (manager) {
        manager->cancelEventQueryRefresh(this);
    }
}

// The instanceId of the matched event
QString CalendarEventQuery::instanceId() const
{
    return mInstanceId;
}

void CalendarEventQuery::setInstanceId(const QString &instanceId)
{
    if (instanceId == mInstanceId)
        return;
    mInstanceId = instanceId;
    emit instanceIdChanged();

    if (mEvent.isValid()) {
        mEvent = CalendarData::Event();
        emit eventChanged();
    }
    if (mOccurrence) {
        delete mOccurrence;
        mOccurrence = 0;
        emit occurrenceChanged();
    }

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
    mUpdateOccurrence = true;
    emit startTimeChanged();

    refresh();
}

void CalendarEventQuery::resetStartTime()
{
    setStartTime(QDateTime());
}

QObject *CalendarEventQuery::event() const
{
    if (mEvent.isValid() && mEvent.instanceId == mInstanceId)
        return CalendarManager::instance()->eventObject(mInstanceId);
    else
        return nullptr;
}

QObject *CalendarEventQuery::occurrence() const
{
    return mOccurrence;
}

QList<QObject*> CalendarEventQuery::attendees()
{
    if (!mAttendeesCached) {
        bool resultValid = false;
        mAttendees = CalendarManager::instance()->getEventAttendees(mInstanceId, &resultValid);
        if (resultValid) {
            mAttendeesCached = true;
        }
    }

    return CalendarUtils::convertAttendeeList(mAttendees);
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

void CalendarEventQuery::doRefresh(CalendarData::Event event, bool eventError)
{
    // The value of mInstanceId may have changed, verify that we got what we asked for
    if (event.isValid() && event.instanceId != mInstanceId)
        return;

    bool updateOccurrence = mUpdateOccurrence;
    bool signalEventChanged = false;

    if (event.instanceId != mEvent.instanceId) {
        mEvent = event;
        signalEventChanged = true;
        updateOccurrence = true;
    } else if (mEvent.isValid()) { // The event may have changed even if the pointer did not
        if (mEvent.allDay != event.allDay
                || mEvent.endTime != event.endTime
                || mEvent.recur != event.recur
                || event.recur == CalendarEvent::RecurCustom
                || mEvent.startTime != event.startTime) {
            mEvent = event;
            updateOccurrence = true;
        }
    }

    if (updateOccurrence) { // Err on the safe side: always update occurrence if it may have changed
        delete mOccurrence;
        mOccurrence = 0;

        if (mEvent.isValid()) {
            CalendarEventOccurrence *occurrence = CalendarManager::instance()->getNextOccurrence(
                    mInstanceId, mStartTime);
            if (occurrence) {
                mOccurrence = occurrence;
                mOccurrence->setParent(this);
            }
        }
        mUpdateOccurrence = false;
        emit occurrenceChanged();
    }

    if (signalEventChanged)
        emit eventChanged();

    // check if attendees have changed.
    bool resultValid = false;
    QList<CalendarData::Attendee> attendees = CalendarManager::instance()->getEventAttendees(
            mInstanceId, &resultValid);
    if (resultValid && mAttendees != attendees) {
        mAttendees = attendees;
        mAttendeesCached = true;
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

void CalendarEventQuery::refresh()
{
    if (!mIsComplete || mInstanceId.isEmpty())
        return;

    CalendarManager::instance()->scheduleEventQueryRefresh(this);
}

void CalendarEventQuery::onTimezoneChanged()
{
    if (mOccurrence) {
        // Actually, the date times have not changed, but
        // their representations in local time (as used in QML)
        // have changed.
        mOccurrence->startTimeChanged();
        mOccurrence->endTimeChanged();
    }
}

void CalendarEventQuery::eventUidChanged(QString oldUid, QString newUid)
{
    if (mInstanceId == oldUid) {
        emit newUniqueId(newUid);
        refresh();
    }
}
