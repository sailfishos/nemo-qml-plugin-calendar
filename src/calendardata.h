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

#ifndef NEMOCALENDARDATA_H
#define NEMOCALENDARDATA_H

#include <QString>
#include <QUrl>
#include <QDateTime>

// KCalendarCore
#include <KCalendarCore/Event>

#include "calendarevent.h"

namespace CalendarData {

struct EventOccurrence {
    QString eventUid;
    QDateTime recurrenceId;
    QDateTime startTime;
    QDateTime endTime;
    bool eventAllDay;

    QString getId() const
    {
        return QString("%1-%2").arg(eventUid).arg(startTime.toMSecsSinceEpoch());
    }
};

struct Event {
    QString displayLabel;
    QString description;
    QDateTime startTime;
    QDateTime endTime;
    bool allDay = false;
    bool readOnly = false;
    bool rsvp = false;
    bool externalInvitation = false;
    CalendarEvent::Recur recur = CalendarEvent::RecurOnce;
    QDate recurEndDate;
    CalendarEvent::Days recurWeeklyDays;
    int reminder = -1; // seconds; 15 minutes before event = +900, at time of event = 0, no reminder = negative value.
    QDateTime reminderDateTime; // Valid when reminder is at a given date and time.
    QString uniqueId;
    QDateTime recurrenceId;
    bool thisAndFuture = false; // Used only when recurrenceId is valid.
    QString location;
    CalendarEvent::Secrecy secrecy = CalendarEvent::SecrecyPublic;
    QString calendarUid;
    CalendarEvent::Response ownerStatus = CalendarEvent::ResponseUnspecified;
    CalendarEvent::Status status = CalendarEvent::StatusNone;
    CalendarEvent::SyncFailure syncFailure = CalendarEvent::NoSyncFailure;
    CalendarEvent::SyncFailureResolution syncFailureResolution = CalendarEvent::RetrySync;

    Event() {}
    Event(const KCalendarCore::Event &event);

    void toKCalendarCore(KCalendarCore::Event::Ptr &event) const;

    bool operator==(const Event& other) const
    {
        return uniqueId == other.uniqueId;
    }

    bool isValid() const
    {
        return !uniqueId.isEmpty();
    }

private:
    int fromKReminder(const KCalendarCore::Event &event) const;
    QDateTime fromKReminderDateTime(const KCalendarCore::Event &event) const;
    void toKReminder(KCalendarCore::Event &event) const;
    CalendarEvent::Days fromKDayPositions(const KCalendarCore::Event &event) const;
    CalendarEvent::Recur fromKRecurrence(const KCalendarCore::Event &event) const;
    void toKRecurrence(KCalendarCore::Event &event) const;
};

struct Notebook {
    QString name;
    QString uid;
    QString description;
    QString color;
    QString emailAddress;
    int accountId;
    QUrl accountIcon;
    bool isDefault;
    bool readOnly;
    bool localCalendar;
    bool excluded;

    Notebook() : accountId(0), isDefault(false), readOnly(false), localCalendar(false), excluded(false) { }

    bool operator==(const Notebook other) const
    {
        return uid == other.uid && name == other.name && description == other.description
                && color == other.color && emailAddress == other.emailAddress
                && accountId == other.accountId && accountIcon == other.accountIcon
                && isDefault == other.isDefault && readOnly == other.readOnly && localCalendar == other.localCalendar
                && excluded == other.excluded;
    }

    bool operator!=(const Notebook other) const
    {
        return !operator==(other);
    }
};

typedef QPair<QDate,QDate> Range;

struct Attendee {
    bool isOrganizer = false;
    QString name;
    QString email;
    KCalendarCore::Attendee::Role participationRole = KCalendarCore::Attendee::OptParticipant;
    KCalendarCore::Attendee::PartStat status = KCalendarCore::Attendee::None;

    bool operator==(const Attendee &other) const {
        return isOrganizer == other.isOrganizer
                && name == other.name
                && email == other.email
                && participationRole == other.participationRole
                && status == other.status;
    }
};

struct EmailContact {
    EmailContact(const QString &aName, const QString &aEmail)
        : name(aName), email(aEmail) {}
    QString name;
    QString email;
};

}
#endif // NEMOCALENDARDATA_H
