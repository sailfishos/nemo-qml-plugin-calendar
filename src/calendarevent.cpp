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

#include "calendarutils.h"
#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "calendardata.h"

CalendarEvent::CalendarEvent(const CalendarData::Event *data, QObject *parent)
    : QObject(parent), mData(new CalendarData::Event)
{
    if (data)
        *mData = *data;
}

CalendarEvent::CalendarEvent(const CalendarEvent *other, QObject *parent)
    : QObject(parent), mData(new CalendarData::Event)
{
    if (other) {
        *mData = *other->mData;
    } else {
        qWarning("Null source passed to CalendarEvent().");
    }
}

CalendarEvent::~CalendarEvent()
{
    delete mData;
}

QString CalendarEvent::displayLabel() const
{
    return mData->displayLabel;
}

QString CalendarEvent::description() const
{
    return mData->description;
}

QDateTime CalendarEvent::startTime() const
{
    // Cannot return the date time directly here. If UTC, the QDateTime
    // will be in UTC also and the UI will convert it to local when displaying
    // the time, while in every other case, it set the QDateTime in
    // local zone.
    const QDateTime dt = mData->startTime;
    return QDateTime(dt.date(), dt.time());
}

QDateTime CalendarEvent::endTime() const
{
    const QDateTime dt = mData->endTime;
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
    return toTimeSpec(mData->startTime);
}

Qt::TimeSpec CalendarEvent::endTimeSpec() const
{
    return toTimeSpec(mData->endTime);
}

QString CalendarEvent::startTimeZone() const
{
    return QString::fromLatin1(mData->startTime.timeZone().id());
}

QString CalendarEvent::endTimeZone() const
{
    return QString::fromLatin1(mData->endTime.timeZone().id());
}

bool CalendarEvent::allDay() const
{
    return mData->allDay;
}

CalendarEvent::Recur CalendarEvent::recur() const
{
    return mData->recur;
}

QDateTime CalendarEvent::recurEndDate() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return mData->recurEndDate.endOfDay();
#else
    return QDateTime(mData->recurEndDate);
#endif
}

bool CalendarEvent::hasRecurEndDate() const
{
    return mData->recurEndDate.isValid();
}

CalendarEvent::Days CalendarEvent::recurWeeklyDays() const
{
    return mData->recurWeeklyDays;
}

int CalendarEvent::reminder() const
{
    return mData->reminder;
}

QDateTime CalendarEvent::reminderDateTime() const
{
    return mData->reminderDateTime;
}

QString CalendarEvent::uniqueId() const
{
    return mData->uniqueId;
}

bool CalendarEvent::readOnly() const
{
    return mData->readOnly;
}

QString CalendarEvent::calendarUid() const
{
    return mData->calendarUid;
}

QString CalendarEvent::location() const
{
    return mData->location;
}

CalendarEvent::Secrecy CalendarEvent::secrecy() const
{
    return mData->secrecy;
}

CalendarEvent::Status CalendarEvent::status() const
{
    return mData->status;
}

CalendarEvent::SyncFailure CalendarEvent::syncFailure() const
{
    return mData->syncFailure;
}

CalendarEvent::SyncFailureResolution CalendarEvent::syncFailureResolution() const
{
    return mData->syncFailureResolution;
}

CalendarEvent::Response CalendarEvent::ownerStatus() const
{
    return mData->ownerStatus;
}

bool CalendarEvent::rsvp() const
{
    return mData->rsvp;
}

bool CalendarEvent::externalInvitation() const
{
    return mData->externalInvitation;
}

QDateTime CalendarEvent::recurrenceId() const
{
    return mData->recurrenceId;
}

QString CalendarEvent::recurrenceIdString() const
{
    if (mData->recurrenceId.isValid()) {
        return CalendarUtils::recurrenceIdToString(mData->recurrenceId);
    } else {
        return QString();
    }
}

bool CalendarEvent::thisAndFuture() const
{
    return mData->thisAndFuture;
}

CalendarStoredEvent::CalendarStoredEvent(CalendarManager *manager, const CalendarData::Event *data)
    : CalendarEvent(data, manager)
    , mManager(manager)
{
    connect(mManager, SIGNAL(notebookColorChanged(QString)),
            this, SLOT(notebookColorChanged(QString)));
    connect(mManager, SIGNAL(eventUidChanged(QString,QString)),
            this, SLOT(eventUidChanged(QString,QString)));
}

CalendarStoredEvent::~CalendarStoredEvent()
{
}

void CalendarStoredEvent::notebookColorChanged(QString notebookUid)
{
    if (mData->calendarUid == notebookUid)
        emit colorChanged();
}

void CalendarStoredEvent::eventUidChanged(QString oldUid, QString newUid)
{
    if (mData->uniqueId == oldUid) {
        mData->uniqueId = newUid;
        emit uniqueIdChanged();
        // Event uid changes when the event is moved between notebooks, calendar uid has changed
        emit calendarUidChanged();
    }
}

bool CalendarStoredEvent::sendResponse(int response)
{
    if (mManager->sendResponse(mData->uniqueId, mData->recurrenceId, (Response)response)) {
        mManager->save();
        return true;
    } else {
        return false;
    }
}

void CalendarStoredEvent::deleteEvent()
{
    mManager->deleteEvent(mData->uniqueId, mData->recurrenceId, QDateTime());
    mManager->save();
}

// Returns the event as a iCalendar string
QString CalendarStoredEvent::iCalendar(const QString &prodId) const
{
    Q_UNUSED(prodId);
    if (mData->uniqueId.isEmpty()) {
        qWarning() << "Event has no uid, returning empty iCalendar string."
                   << "Save event before calling this function";
        return QString();
    }

    return mManager->convertEventToICalendarSync(mData->uniqueId, prodId);
}

QString CalendarStoredEvent::color() const
{
    return mManager->getNotebookColor(mData->calendarUid);
}

void CalendarStoredEvent::setEvent(const CalendarData::Event *data)
{
    if (!data)
        return;

    CalendarData::Event old = *mData;
    *mData = *data;

    if (mData->allDay != old.allDay)
        emit allDayChanged();
    if (mData->displayLabel != old.displayLabel)
        emit displayLabelChanged();
    if (mData->description != old.description)
        emit descriptionChanged();
    if (mData->endTime != old.endTime)
        emit endTimeChanged();
    if (mData->location != old.location)
        emit locationChanged();
    if (mData->secrecy != old.secrecy)
        emit secrecyChanged();
    if (mData->status != old.status)
        emit statusChanged();
    if (mData->recur != old.recur)
        emit recurChanged();
    if (mData->reminder != old.reminder)
        emit reminderChanged();
    if (mData->reminderDateTime != old.reminderDateTime)
        emit reminderDateTimeChanged();
    if (mData->startTime != old.startTime)
        emit startTimeChanged();
    if (mData->rsvp != old.rsvp)
        emit rsvpChanged();
    if (mData->externalInvitation != old.externalInvitation)
        emit externalInvitationChanged();
    if (mData->ownerStatus != old.ownerStatus)
        emit ownerStatusChanged();
    if (mData->syncFailure != old.syncFailure)
        emit syncFailureChanged();
    if (mData->thisAndFuture != old.thisAndFuture)
        emit thisAndFutureChanged();
}

CalendarData::Event CalendarStoredEvent::dissociateSingleOccurrence(const CalendarEventOccurrence *occurrence) const
{
    if (occurrence && mData->thisAndFuture) {
        const QDateTime recId = occurrence->startTime().addSecs(mData->startTime.secsTo(mData->recurrenceId));
        return mManager->dissociateSingleOccurrence(mData->uniqueId, recId);
    } else if (occurrence) {
        return mManager->dissociateSingleOccurrence(mData->uniqueId,
                                                    occurrence->startTime());
    } else {
        return CalendarData::Event();
    }
}
