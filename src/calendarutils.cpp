/*
 * Copyright (c) 2015-2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * Contact: Petri M. Gerdt <petri.gerdt@jollamobile.com>
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

#include "calendarutils.h"

#include "calendareventquery.h"

// kcalcore
#include <icalformat.h>
#include <vcalformat.h>

//mkcal
#include <servicehandler.h>

// Qt
#include <QFile>
#include <QUrl>
#include <QString>
#include <QBitArray>
#include <QByteArray>
#include <QtDebug>

CalendarEvent::Recur CalendarUtils::convertRecurrence(const KCalCore::Event::Ptr &event)
{
    if (!event->recurs())
        return CalendarEvent::RecurOnce;

    if (event->recurrence()->rRules().count() != 1)
        return CalendarEvent::RecurCustom;

    ushort rt = event->recurrence()->recurrenceType();
    int freq = event->recurrence()->frequency();

    if (rt == KCalCore::Recurrence::rDaily && freq == 1) {
        return CalendarEvent::RecurDaily;
    } else if (rt == KCalCore::Recurrence::rWeekly && freq == 1 && event->recurrence()->days().count(true) == 0) {
        return CalendarEvent::RecurWeekly;
    } else if (rt == KCalCore::Recurrence::rWeekly && freq == 2 && event->recurrence()->days().count(true) == 0) {
        return CalendarEvent::RecurBiweekly;
    } else if (rt == KCalCore::Recurrence::rMonthlyDay && freq == 1) {
        return CalendarEvent::RecurMonthly;
    } else if (rt == KCalCore::Recurrence::rYearlyMonth && freq == 1) {
        return CalendarEvent::RecurYearly;
    }

    return CalendarEvent::RecurCustom;
}

CalendarEvent::Secrecy CalendarUtils::convertSecrecy(const KCalCore::Event::Ptr &event)
{
    KCalCore::Incidence::Secrecy s = event->secrecy();
    switch (s) {
    case KCalCore::Incidence::SecrecyPrivate:
        return CalendarEvent::SecrecyPrivate;
    case KCalCore::Incidence::SecrecyConfidential:
        return CalendarEvent::SecrecyConfidential;
    case KCalCore::Incidence::SecrecyPublic:
    default:
        return CalendarEvent::SecrecyPublic;
    }
}

int CalendarUtils::getReminder(const KCalCore::Event::Ptr &event)
{
    KCalCore::Alarm::List alarms = event->alarms();

    KCalCore::Alarm::Ptr alarm;

    int seconds = -1;
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure)
            continue;
        alarm = alarms.at(ii);
        if (alarm) {
            KCalCore::Duration d = alarm->startOffset();
            seconds = d.asSeconds() * -1; // backend stores as "offset in seconds to dtStart", we return "seconds before"
            if (seconds >= 0) {
                break;
            }
        }
        break;
    }

    return seconds;
}

QList<CalendarData::Attendee> CalendarUtils::getEventAttendees(const KCalCore::Event::Ptr &event)
{
    QList<CalendarData::Attendee> result;
    KCalCore::Person::Ptr calOrganizer = event->organizer();

    CalendarData::Attendee organizer;

    if (!calOrganizer.isNull() && !calOrganizer->isEmpty()) {
        organizer.isOrganizer = true;
        organizer.name = calOrganizer->name();
        organizer.email = calOrganizer->email();
        organizer.participationRole = KCalCore::Attendee::Chair;
        result.append(organizer);
    }

    KCalCore::Attendee::List attendees = event->attendees();
    CalendarData::Attendee attendee;
    attendee.isOrganizer = false;

    foreach (KCalCore::Attendee::Ptr calAttendee, attendees) {
        attendee.name = calAttendee->name();
        attendee.email = calAttendee->email();
        if (attendee.name == organizer.name && attendee.email == organizer.email) {
            // avoid duplicate info
            continue;
        }

        attendee.status = calAttendee->status();
        attendee.participationRole = calAttendee->role();
        result.append(attendee);
    }

    return result;
}

QList<QObject *> CalendarUtils::convertAttendeeList(const QList<CalendarData::Attendee> &list)
{
    QList<QObject*> result;
    foreach (const CalendarData::Attendee &attendee, list) {
        Person::AttendeeRole role;
        switch (attendee.participationRole) {
        case KCalCore::Attendee::ReqParticipant:
            role = Person::RequiredParticipant;
            break;
        case KCalCore::Attendee::OptParticipant:
            role = Person::OptionalParticipant;
            break;
        case KCalCore::Attendee::Chair:
            role = Person::ChairParticipant;
            break;
        default:
            role = Person::NonParticipant;
            break;
        }

        Person::ParticipationStatus status;
        switch (attendee.status) {
        case KCalCore::Attendee::Accepted:
            status = Person::AcceptedParticipation;
            break;
        case KCalCore::Attendee::Declined:
            status = Person::DeclinedParticipation;
            break;
        case KCalCore::Attendee::Tentative:
            status = Person::TentativeParticipation;
            break;
        default:
            status = Person::UnknownParticipation;
        }

        QObject *person = new Person(attendee.name, attendee.email, attendee.isOrganizer, role, status);
        result.append(person);
    }

    return result;
}

CalendarData::EventOccurrence CalendarUtils::getNextOccurrence(const KCalCore::Event::Ptr &event,
                                                               const QDateTime &start)
{
    CalendarData::EventOccurrence occurrence;
    if (event) {
        QDateTime dtStart = event->dtStart().toLocalZone().dateTime();
        QDateTime dtEnd = event->dtEnd().toLocalZone().dateTime();

        if (!start.isNull() && event->recurs()) {
            KDateTime startTime = KDateTime(start, KDateTime::Spec(KDateTime::LocalZone));
            KCalCore::Recurrence *recurrence = event->recurrence();
            if (recurrence->recursAt(startTime)) {
                dtStart = startTime.toLocalZone().dateTime();
                dtEnd = KCalCore::Duration(event->dtStart(), event->dtEnd()).end(startTime).toLocalZone().dateTime();
            } else {
                KDateTime match = recurrence->getNextDateTime(startTime);
                if (match.isNull())
                    match = recurrence->getPreviousDateTime(startTime);

                if (!match.isNull()) {
                    dtStart = match.toLocalZone().dateTime();
                    dtEnd = KCalCore::Duration(event->dtStart(), event->dtEnd()).end(match).toLocalZone().dateTime();
                }
            }
        }

        occurrence.eventUid = event->uid();
        occurrence.recurrenceId = event->recurrenceId();
        occurrence.startTime = dtStart;
        occurrence.endTime = dtEnd;
    }

    return occurrence;
}

bool CalendarUtils::importFromFile(const QString &fileName,
                                   KCalCore::Calendar::Ptr calendar)
{
    QString filePath;
    QUrl url(fileName);
    if (url.isLocalFile())
        filePath = url.toLocalFile();
    else
        filePath = fileName;

    if (!(filePath.endsWith(".vcs") || filePath.endsWith(".ics"))) {
        qWarning() << "Unsupported file format" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Unable to open file for reading" << filePath;
        return false;
    }
    QByteArray fileContent(file.readAll());

    bool ok = false;
    if (filePath.endsWith(".vcs")) {
        KCalCore::VCalFormat vcalFormat;
        ok = vcalFormat.fromRawString(calendar, fileContent);
    } else if (filePath.endsWith(".ics")) {
        KCalCore::ICalFormat icalFormat;
        ok = icalFormat.fromRawString(calendar, fileContent);
    }
    if (!ok)
        qWarning() << "Failed to import from file" << filePath;

    return ok;
}

bool CalendarUtils::importFromIcsRawData(const QByteArray &icsData,
                                         KCalCore::Calendar::Ptr calendar)
{
    bool ok = false;
    KCalCore::ICalFormat icalFormat;
    ok = icalFormat.fromRawString(calendar, icsData);
    if (!ok)
        qWarning() << "Failed to import from raw data";

    return ok;
}

CalendarEvent::Response CalendarUtils::convertPartStat(KCalCore::Attendee::PartStat status)
{
    switch (status) {
    case KCalCore::Attendee::Accepted:
        return CalendarEvent::ResponseAccept;
    case KCalCore::Attendee::Declined:
        return CalendarEvent::ResponseDecline;
    case KCalCore::Attendee::Tentative:
        return CalendarEvent::ResponseTentative;
    case KCalCore::Attendee::NeedsAction:
    case KCalCore::Attendee::None:
    default:
        return CalendarEvent::ResponseUnspecified;
    }
}

KCalCore::Attendee::PartStat CalendarUtils::convertResponse(CalendarEvent::Response response)
{
    switch (response) {
    case CalendarEvent::ResponseAccept:
        return KCalCore::Attendee::Accepted;
    case CalendarEvent::ResponseTentative:
        return KCalCore::Attendee::Tentative;
    case CalendarEvent::ResponseDecline:
        return KCalCore::Attendee::Declined;
    default:
        return KCalCore::Attendee::NeedsAction;
    }
}

CalendarEvent::Response CalendarUtils::convertResponseType(const QString &responseType)
{
    // QString::toInt() conversion defaults to 0 on failure
    switch (responseType.toInt()) {
    case 1: // OrganizerResponseType (Organizer's acceptance is implicit)
    case 3: // AcceptedResponseType
        return CalendarEvent::ResponseAccept;
    case 2: // TentativeResponseType
        return CalendarEvent::ResponseTentative;
    case 4: // DeclinedResponseType
        return CalendarEvent::ResponseDecline;
    case -1: // ResponseTypeUnset
    case 0: // NoneResponseType
    case 5: // NotRespondedResponseType
    default:
        return CalendarEvent::ResponseUnspecified;
    }
}
