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
    : m_isComplete(true), m_occurrence(0), m_attendeesCached(false), m_eventError(false), m_updateOccurrence(false)
{
    connect(CalendarManager::instance(), SIGNAL(dataUpdated()), this, SLOT(refresh()));
    connect(CalendarManager::instance(), SIGNAL(storageModified()), this, SLOT(refresh()));
    connect(CalendarManager::instance(), &CalendarManager::timezoneChanged,
            this, &CalendarEventQuery::onTimezoneChanged);

    connect(CalendarManager::instance(), &CalendarManager::instanceIdChanged,
            this, &CalendarEventQuery::instanceIdNotified);
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
    return m_instanceId;
}

void CalendarEventQuery::setInstanceId(const QString &instanceId)
{
    if (instanceId == m_instanceId)
        return;
    m_instanceId = instanceId;
    emit instanceIdChanged();

    if (m_event.isValid()) {
        m_event = CalendarData::Event();
        emit eventChanged();
    }
    if (m_occurrence) {
        delete m_occurrence;
        m_occurrence = 0;
        emit occurrenceChanged();
    }

    refresh();
}

// The ideal start time of the occurrence.  If there is no occurrence with
// the exact start time, the first occurrence following startTime is returned.
// If there is no following occurrence, the previous occurrence is returned.
QDateTime CalendarEventQuery::startTime() const
{
    return m_startTime;
}

void CalendarEventQuery::setStartTime(const QDateTime &t)
{
    if (t == m_startTime)
        return;
    m_startTime = t;
    m_updateOccurrence = true;
    emit startTimeChanged();

    refresh();
}

void CalendarEventQuery::resetStartTime()
{
    setStartTime(QDateTime());
}

QObject *CalendarEventQuery::event() const
{
    if (m_event.isValid() && m_event.instanceId == m_instanceId)
        return CalendarManager::instance()->eventObject(m_instanceId);
    else
        return nullptr;
}

QObject *CalendarEventQuery::occurrence() const
{
    return m_occurrence;
}

QList<QObject*> CalendarEventQuery::attendees()
{
    if (!m_attendeesCached) {
        bool resultValid = false;
        m_attendees = CalendarManager::instance()->getEventAttendees(m_instanceId, &resultValid);
        if (resultValid) {
            m_attendeesCached = true;
        }
    }

    return CalendarUtils::convertAttendeeList(m_attendees);
}

void CalendarEventQuery::classBegin()
{
    m_isComplete = false;
}

void CalendarEventQuery::componentComplete()
{
    m_isComplete = true;
    refresh();
}

void CalendarEventQuery::doRefresh(CalendarData::Event event, bool eventError)
{
    // The value of m_instanceId may have changed, verify that we got what we asked for
    if (event.isValid() && event.instanceId != m_instanceId)
        return;

    bool updateOccurrence = m_updateOccurrence;
    bool signalEventChanged = false;

    if (event.instanceId != m_event.instanceId) {
        m_event = event;
        signalEventChanged = true;
        updateOccurrence = true;
    } else if (m_event.isValid()) { // The event may have changed even if the pointer did not
        if (m_event.allDay != event.allDay
                || m_event.endTime != event.endTime
                || m_event.recur != event.recur
                || event.recur == CalendarEvent::RecurCustom
                || m_event.startTime != event.startTime) {
            m_event = event;
            updateOccurrence = true;
        }
    }

    if (updateOccurrence) { // Err on the safe side: always update occurrence if it may have changed
        delete m_occurrence;
        m_occurrence = 0;

        if (m_event.isValid()) {
            CalendarEventOccurrence *occurrence = CalendarManager::instance()->getNextOccurrence(
                    m_instanceId, m_startTime);
            if (occurrence) {
                m_occurrence = occurrence;
                m_occurrence->setParent(this);
            }
        }
        m_updateOccurrence = false;
        emit occurrenceChanged();
    }

    if (signalEventChanged)
        emit eventChanged();

    // check if attendees have changed.
    bool resultValid = false;
    QList<CalendarData::Attendee> attendees = CalendarManager::instance()->getEventAttendees(
            m_instanceId, &resultValid);
    if (resultValid && m_attendees != attendees) {
        m_attendees = attendees;
        m_attendeesCached = true;
        emit attendeesChanged();
    }

    if (m_eventError != eventError) {
        m_eventError = eventError;
        emit eventErrorChanged();
    }
}

bool CalendarEventQuery::eventError() const
{
    return m_eventError;
}

void CalendarEventQuery::refresh()
{
    if (!m_isComplete || m_instanceId.isEmpty())
        return;

    CalendarManager::instance()->scheduleEventQueryRefresh(this);
}

void CalendarEventQuery::onTimezoneChanged()
{
    if (m_occurrence) {
        // Actually, the date times have not changed, but
        // their representations in local time (as used in QML)
        // have changed.
        m_occurrence->startTimeChanged();
        m_occurrence->endTimeChanged();
    }
}

void CalendarEventQuery::instanceIdNotified(QString oldId, QString newId, QString notebookUid)
{
    if (m_instanceId == oldId) {
        m_instanceId = newId;
        emit instanceIdChanged();

        if (m_event.isValid()) {
            m_event.instanceId = newId;
            m_event.calendarUid = notebookUid;
        } else {
            refresh();
        }
    }
}
