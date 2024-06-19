/*
 * Copyright (c) 2013 - 2019 Jolla Ltd.
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

#include "calendarevent.h"

#include <QQmlInfo>
#include <QDateTime>
#include <QTimeZone>

#include <KCalendarCore/Event>

#include "calendarutils.h"
#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "calendardata.h"

CalendarEvent::CalendarEvent(const CalendarData::Event *data, QObject *parent)
    : QObject(parent), m_data(new CalendarData::Event)
{
    if (data)
        *m_data = *data;
}

CalendarEvent::CalendarEvent(const CalendarEvent *other, QObject *parent)
    : QObject(parent), m_data(new CalendarData::Event)
{
    if (other) {
        *m_data = *other->m_data;
    } else {
        qWarning("Null source passed to CalendarEvent().");
    }
}

CalendarEvent::~CalendarEvent()
{
    delete m_data;
}

QString CalendarEvent::displayLabel() const
{
    return m_data->displayLabel;
}

QString CalendarEvent::description() const
{
    return m_data->description;
}

QDateTime CalendarEvent::startTime() const
{
    // Cannot return the date time directly here. If UTC, the QDateTime
    // will be in UTC also and the UI will convert it to local when displaying
    // the time, while in every other case, it set the QDateTime in
    // local zone.
    const QDateTime dt = m_data->startTime;
    return QDateTime(dt.date(), dt.time());
}

QDateTime CalendarEvent::endTime() const
{
    const QDateTime dt = m_data->endTime;
    return QDateTime(dt.date(), dt.time());
}

static Qt::TimeSpec toTimeSpec(const QDateTime &dt)
{
    if (dt.timeZone() == QTimeZone::utc()) {
        return Qt::UTC;
    }

    return dt.timeSpec();
}

Qt::TimeSpec CalendarEvent::startTimeSpec() const
{
    return toTimeSpec(m_data->startTime);
}

Qt::TimeSpec CalendarEvent::endTimeSpec() const
{
    return toTimeSpec(m_data->endTime);
}

QString CalendarEvent::startTimeZone() const
{
    return QString::fromLatin1(m_data->startTime.timeZone().id());
}

QString CalendarEvent::endTimeZone() const
{
    return QString::fromLatin1(m_data->endTime.timeZone().id());
}

bool CalendarEvent::allDay() const
{
    return m_data->allDay;
}

CalendarEvent::Recur CalendarEvent::recur() const
{
    return m_data->recur;
}

QDateTime CalendarEvent::recurEndDate() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return m_data->recurEndDate.endOfDay();
#else
    return QDateTime(m_data->recurEndDate);
#endif
}

bool CalendarEvent::hasRecurEndDate() const
{
    return m_data->recurEndDate.isValid();
}

CalendarEvent::Days CalendarEvent::recurWeeklyDays() const
{
    return m_data->recurWeeklyDays;
}

int CalendarEvent::reminder() const
{
    return m_data->reminder;
}

QDateTime CalendarEvent::reminderDateTime() const
{
    return m_data->reminderDateTime;
}

QString CalendarEvent::instanceId() const
{
    return m_data->instanceId;
}

bool CalendarEvent::isException() const
{
    return m_data->recurrenceId.isValid();
}

bool CalendarEvent::readOnly() const
{
    return m_data->readOnly;
}

QString CalendarEvent::calendarUid() const
{
    return m_data->calendarUid;
}

QString CalendarEvent::location() const
{
    return m_data->location;
}

CalendarEvent::Secrecy CalendarEvent::secrecy() const
{
    return m_data->secrecy;
}

CalendarEvent::Status CalendarEvent::status() const
{
    return m_data->status;
}

CalendarEvent::SyncFailure CalendarEvent::syncFailure() const
{
    return m_data->syncFailure;
}

CalendarEvent::SyncFailureResolution CalendarEvent::syncFailureResolution() const
{
    return m_data->syncFailureResolution;
}

CalendarEvent::Response CalendarEvent::ownerStatus() const
{
    return m_data->ownerStatus;
}

bool CalendarEvent::rsvp() const
{
    return m_data->rsvp;
}

bool CalendarEvent::externalInvitation() const
{
    return m_data->externalInvitation;
}

CalendarStoredEvent::CalendarStoredEvent(CalendarManager *manager, const CalendarData::Event *data)
    : CalendarEvent(data, manager)
    , m_manager(manager)
{
    connect(m_manager, SIGNAL(notebookColorChanged(QString)),
            this, SLOT(notebookColorChanged(QString)));
    connect(m_manager, &CalendarManager::instanceIdChanged,
            this, &CalendarStoredEvent::instanceIdNotified);
}

CalendarStoredEvent::~CalendarStoredEvent()
{
}

void CalendarStoredEvent::notebookColorChanged(QString notebookUid)
{
    if (m_data->calendarUid == notebookUid)
        emit colorChanged();
}

void CalendarStoredEvent::instanceIdNotified(QString oldId, QString newId, QString notebookUid)
{
    if (m_data->instanceId == oldId) {
        m_data->instanceId = newId;
        emit instanceIdChanged();
        // Event uid changes when the event is moved between notebooks, calendar uid has changed
        m_data->calendarUid = notebookUid;
        emit calendarUidChanged();
        emit colorChanged();
    }
}

bool CalendarStoredEvent::sendResponse(int response)
{
    if (m_manager->sendResponse(m_data->instanceId, (Response)response)) {
        m_manager->save();
        return true;
    } else {
        return false;
    }
}

void CalendarStoredEvent::deleteEvent()
{
    m_manager->deleteEvent(m_data->instanceId, QDateTime());
    m_manager->save();
}

// Returns the event as a iCalendar string
QString CalendarStoredEvent::iCalendar(const QString &prodId) const
{
    Q_UNUSED(prodId);
    if (m_data->instanceId.isEmpty()) {
        qWarning() << "Event has no uid, returning empty iCalendar string."
                   << "Save event before calling this function";
        return QString();
    }

    return m_manager->convertEventToICalendarSync(m_data->instanceId, prodId);
}

CalendarStoredEvent* CalendarStoredEvent::parent() const
{
    if (isException()) {
        KCalendarCore::Event event;
        event.setUid(m_data->incidenceUid);
        return m_manager->eventObject(event.instanceIdentifier());
    } else {
        return nullptr;
    }
}

QString CalendarStoredEvent::color() const
{
    return m_manager->getNotebookColor(m_data->calendarUid);
}

void CalendarStoredEvent::setEvent(const CalendarData::Event *data)
{
    if (!data)
        return;

    CalendarData::Event old = *m_data;
    *m_data = *data;

    if (m_data->allDay != old.allDay)
        emit allDayChanged();
    if (m_data->displayLabel != old.displayLabel)
        emit displayLabelChanged();
    if (m_data->description != old.description)
        emit descriptionChanged();
    if (m_data->endTime != old.endTime)
        emit endTimeChanged();
    if (m_data->location != old.location)
        emit locationChanged();
    if (m_data->secrecy != old.secrecy)
        emit secrecyChanged();
    if (m_data->status != old.status)
        emit statusChanged();
    if (m_data->recur != old.recur)
        emit recurChanged();
    if (m_data->reminder != old.reminder)
        emit reminderChanged();
    if (m_data->reminderDateTime != old.reminderDateTime)
        emit reminderDateTimeChanged();
    if (m_data->startTime != old.startTime)
        emit startTimeChanged();
    if (m_data->rsvp != old.rsvp)
        emit rsvpChanged();
    if (m_data->externalInvitation != old.externalInvitation)
        emit externalInvitationChanged();
    if (m_data->ownerStatus != old.ownerStatus)
        emit ownerStatusChanged();
    if (m_data->syncFailure != old.syncFailure)
        emit syncFailureChanged();
}

CalendarData::Event CalendarStoredEvent::dissociateSingleOccurrence(const CalendarEventOccurrence *occurrence) const
{
    return occurrence ? m_manager->dissociateSingleOccurrence(m_data->instanceId, occurrence->startTime()) : CalendarData::Event();
}
