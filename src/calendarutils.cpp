/*
 * Copyright (c) 2015-2019 Jolla Ltd.
 * Copyright (c) 2019 - 2020 Open Mobile Platform LLC.
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

// kcalendarcore
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/VCalFormat>

//mkcal
#include <servicehandler.h>

// Qt
#include <QFile>
#include <QUrl>
#include <QString>
#include <QBitArray>
#include <QDebug>

CalendarData::Event::Event(const KCalendarCore::Event &event)
    : displayLabel(event.summary())
    , description(event.description())
    , startTime(event.dtStart())
    , endTime(event.dtEnd())
    , allDay(event.allDay())
    , uniqueId(event.uid())
    , recurrenceId(event.recurrenceId())
    , location(event.location())
{
    switch (event.secrecy()) {
    case KCalendarCore::Incidence::SecrecyPrivate:
        secrecy = CalendarEvent::SecrecyPrivate;
        break;
    case KCalendarCore::Incidence::SecrecyConfidential:
        secrecy = CalendarEvent::SecrecyConfidential;
        break;
    default:
        break;
    }
    switch (event.status()) {
    case KCalendarCore::Incidence::StatusTentative:
        status = CalendarEvent::StatusTentative;
        break;
    case KCalendarCore::Incidence::StatusConfirmed:
        status = CalendarEvent::StatusConfirmed;
        break;
    case KCalendarCore::Incidence::StatusCanceled:
        status = CalendarEvent::StatusCancelled;
        break;
    default:
        break;
    }
    const QString &failure = event.customProperty("VOLATILE", "SYNC-FAILURE");
    if (failure.compare("upload-new", Qt::CaseInsensitive) == 0) {
        syncFailure = CalendarEvent::CreationFailure;
    } else if (failure.compare("upload", Qt::CaseInsensitive) == 0) {
        syncFailure = CalendarEvent::UploadFailure;
    } else if (failure.compare("update", Qt::CaseInsensitive) == 0) {
        syncFailure = CalendarEvent::UpdateFailure;
    } else if (failure.compare("delete", Qt::CaseInsensitive) == 0) {
        syncFailure = CalendarEvent::DeleteFailure;
    }
    const QString &syncResolution = event.customProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION");
    if (syncResolution.compare("keep-out-of-sync", Qt::CaseInsensitive) == 0) {
        syncFailureResolution = CalendarEvent::KeepOutOfSync;
    } else if (syncResolution.compare("server-reset", Qt::CaseInsensitive) == 0) {
        syncFailureResolution = CalendarEvent::PullServerData;
    } else if (syncResolution.compare("device-reset", Qt::CaseInsensitive) == 0) {
        syncFailureResolution = CalendarEvent::PushDeviceData;
    } else if (!syncResolution.isEmpty()) {
        qWarning() << "unsupported sync failure resolution" << syncResolution;
    }
    recur = fromKRecurrence(event);
    recurWeeklyDays = fromKDayPositions(event);
    KCalendarCore::RecurrenceRule *defaultRule = event.recurrence()->defaultRRule();
    if (defaultRule) {
        recurEndDate = defaultRule->endDt().date();
    }
    reminder = fromKReminder(event);
    reminderDateTime = fromKReminderDateTime(event);
}

void CalendarData::Event::toKCalendarCore(KCalendarCore::Event::Ptr &event) const
{
    event->setDescription(description);
    event->setSummary(displayLabel);
    event->setDtStart(startTime);
    event->setDtEnd(endTime);
    event->setAllDay(allDay);
    event->setLocation(location);
    toKReminder(*event);
    toKRecurrence(*event);
    switch (status) {
    case CalendarEvent::StatusNone:
        event->setStatus(KCalendarCore::Incidence::StatusNone);
        break;
    case CalendarEvent::StatusTentative:
        event->setStatus(KCalendarCore::Incidence::StatusTentative);
        break;
    case CalendarEvent::StatusConfirmed:
        event->setStatus(KCalendarCore::Incidence::StatusConfirmed);
        break;
    case CalendarEvent::StatusCancelled:
        event->setStatus(KCalendarCore::Incidence::StatusCanceled);
        break;
    default:
        qWarning() << "unknown status value" << status;
    }

    if (recur != CalendarEvent::RecurOnce) {
        event->recurrence()->setEndDate(recurEndDate);
        if (!recurEndDate.isValid()) {
            // Recurrence/RecurrenceRule don't have separate method to clear the end date, and currently
            // setting invalid date doesn't make the duration() indicate recurring infinitely.
            event->recurrence()->setDuration(-1);
        }
    }
    if (syncFailureResolution == CalendarEvent::RetrySync) {
        event->removeCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION");
    } else if (syncFailureResolution == CalendarEvent::KeepOutOfSync) {
        event->setCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION", "keep-out-of-sync");
    } else if (syncFailureResolution == CalendarEvent::PushDeviceData) {
        event->setCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION", "device-reset");
    } else if (syncFailureResolution == CalendarEvent::PullServerData) {
        event->setCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION", "server-reset");
    } else {
        qWarning() << "No support for sync failure resolution" << syncFailureResolution;
    }
}

void CalendarData::Event::toKReminder(KCalendarCore::Event &event) const
{
    if (fromKReminder(event) == reminder
        && fromKReminderDateTime(event) == reminderDateTime)
        return;

    KCalendarCore::Alarm::List alarms = event.alarms();
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalendarCore::Alarm::Procedure)
            continue;
        event.removeAlarm(alarms.at(ii));
    }

    // negative reminder seconds means "no reminder", so only
    // deal with positive (or zero = at time of event) reminders.
    if (reminder >= 0) {
        KCalendarCore::Alarm::Ptr alarm = event.newAlarm();
        alarm->setEnabled(true);
        // backend stores as "offset to dtStart", i.e negative if reminder before event.
        alarm->setStartOffset(-1 * reminder);
        alarm->setType(KCalendarCore::Alarm::Display);
    } else if (reminderDateTime.isValid()) {
        KCalendarCore::Alarm::Ptr alarm = event.newAlarm();
        alarm->setEnabled(true);
        alarm->setTime(reminderDateTime);
        alarm->setType(KCalendarCore::Alarm::Display);
    }
}

void CalendarData::Event::toKRecurrence(KCalendarCore::Event &event) const
{
    CalendarEvent::Recur oldRecur = fromKRecurrence(event);

    if (recur == CalendarEvent::RecurOnce)
        event.recurrence()->clear();

    if (oldRecur != recur
        || recur == CalendarEvent::RecurMonthlyByDayOfWeek
        || recur == CalendarEvent::RecurMonthlyByLastDayOfWeek
        || recur == CalendarEvent::RecurWeeklyByDays) {
        switch (recur) {
        case CalendarEvent::RecurOnce:
            break;
        case CalendarEvent::RecurDaily:
            event.recurrence()->setDaily(1);
            break;
        case CalendarEvent::RecurWeekly:
            event.recurrence()->setWeekly(1);
            break;
        case CalendarEvent::RecurBiweekly:
            event.recurrence()->setWeekly(2);
            break;
        case CalendarEvent::RecurWeeklyByDays: {
            QBitArray rDays(7);
            rDays.setBit(0, recurWeeklyDays & CalendarEvent::Monday);
            rDays.setBit(1, recurWeeklyDays & CalendarEvent::Tuesday);
            rDays.setBit(2, recurWeeklyDays & CalendarEvent::Wednesday);
            rDays.setBit(3, recurWeeklyDays & CalendarEvent::Thursday);
            rDays.setBit(4, recurWeeklyDays & CalendarEvent::Friday);
            rDays.setBit(5, recurWeeklyDays & CalendarEvent::Saturday);
            rDays.setBit(6, recurWeeklyDays & CalendarEvent::Sunday);
            event.recurrence()->setWeekly(1, rDays);
            break;
        }
        case CalendarEvent::RecurMonthly:
            event.recurrence()->setMonthly(1);
            break;
        case CalendarEvent::RecurMonthlyByDayOfWeek: {
            event.recurrence()->setMonthly(1);
            const QDate at(event.dtStart().date());
            event.recurrence()->addMonthlyPos((at.day() - 1) / 7 + 1, at.dayOfWeek());
            break;
        }
        case CalendarEvent::RecurMonthlyByLastDayOfWeek: {
            event.recurrence()->setMonthly(1);
            const QDate at(event.dtStart().date());
            event.recurrence()->addMonthlyPos(-1, at.dayOfWeek());
            break;
        }
        case CalendarEvent::RecurYearly:
            event.recurrence()->setYearly(1);
            break;
        case CalendarEvent::RecurCustom:
            // Unable to handle the recurrence rules, keep the existing ones.
            break;
        }
    }
}

CalendarEvent::Recur CalendarData::Event::fromKRecurrence(const KCalendarCore::Event &event) const
{
    if (!event.recurs())
        return CalendarEvent::RecurOnce;

    if (event.recurrence()->rRules().count() != 1)
        return CalendarEvent::RecurCustom;

    ushort rt = event.recurrence()->recurrenceType();
    int freq = event.recurrence()->frequency();

    if (rt == KCalendarCore::Recurrence::rDaily && freq == 1) {
        return CalendarEvent::RecurDaily;
    } else if (rt == KCalendarCore::Recurrence::rWeekly && freq == 1) {
        if (event.recurrence()->days().count(true) == 0) {
            return CalendarEvent::RecurWeekly;
        } else {
            return CalendarEvent::RecurWeeklyByDays;
        }
    } else if (rt == KCalendarCore::Recurrence::rWeekly && freq == 2 && event.recurrence()->days().count(true) == 0) {
        return CalendarEvent::RecurBiweekly;
    } else if (rt == KCalendarCore::Recurrence::rMonthlyDay && freq == 1) {
        return CalendarEvent::RecurMonthly;
    } else if (rt == KCalendarCore::Recurrence::rMonthlyPos && freq == 1) {
        const QList<KCalendarCore::RecurrenceRule::WDayPos> monthPositions = event.recurrence()->monthPositions();
        if (monthPositions.length() == 1
            && monthPositions.first().day() == event.dtStart().date().dayOfWeek()) {
            if (monthPositions.first().pos() > 0) {
                return CalendarEvent::RecurMonthlyByDayOfWeek;
            } else if (monthPositions.first().pos() == -1) {
                return CalendarEvent::RecurMonthlyByLastDayOfWeek;
            }
        }
    } else if (rt == KCalendarCore::Recurrence::rYearlyMonth && freq == 1) {
        return CalendarEvent::RecurYearly;
    }

    return CalendarEvent::RecurCustom;
}

CalendarEvent::Days CalendarData::Event::fromKDayPositions(const KCalendarCore::Event &event) const
{
    if (!event.recurs())
        return CalendarEvent::NoDays;

    if (event.recurrence()->rRules().count() != 1)
        return CalendarEvent::NoDays;

    if (event.recurrence()->recurrenceType() != KCalendarCore::Recurrence::rWeekly
        || event.recurrence()->frequency() != 1)
        return CalendarEvent::NoDays;

    const CalendarEvent::Day week[7] = {CalendarEvent::Monday,
                                        CalendarEvent::Tuesday,
                                        CalendarEvent::Wednesday,
                                        CalendarEvent::Thursday,
                                        CalendarEvent::Friday,
                                        CalendarEvent::Saturday,
                                        CalendarEvent::Sunday};

    const QList<KCalendarCore::RecurrenceRule::WDayPos> monthPositions = event.recurrence()->monthPositions();
    CalendarEvent::Days days = CalendarEvent::NoDays;
    for (QList<KCalendarCore::RecurrenceRule::WDayPos>::ConstIterator it = monthPositions.constBegin();
         it != monthPositions.constEnd(); ++it) {
        days |= week[it->day() - 1];
    }
    return days;
}

int CalendarData::Event::fromKReminder(const KCalendarCore::Event &event) const
{
    KCalendarCore::Alarm::List alarms = event.alarms();

    KCalendarCore::Alarm::Ptr alarm;

    int seconds = -1; // Any negative values means "no reminder"
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalendarCore::Alarm::Procedure)
            continue;
        alarm = alarms.at(ii);
        if (alarm && !alarm->hasTime()) {
            KCalendarCore::Duration d = alarm->startOffset();
            seconds = d.asSeconds() * -1; // backend stores as "offset in seconds to dtStart", we return "seconds before"
            if (seconds >= 0) {
                break;
            }
        }
        break;
    }

    return seconds;
}

QDateTime CalendarData::Event::fromKReminderDateTime(const KCalendarCore::Event &event) const
{
    for (const KCalendarCore::Alarm::Ptr &alarm : event.alarms()) {
        if (alarm && alarm->type() == KCalendarCore::Alarm::Display && alarm->hasTime()) {
            return alarm->time();
        }
    }

    return QDateTime();
}

QList<CalendarData::Attendee> CalendarUtils::getEventAttendees(const KCalendarCore::Event::Ptr &event)
{
    QList<CalendarData::Attendee> result;
    const KCalendarCore::Person calOrganizer = event->organizer();

    const KCalendarCore::Attendee::List attendees = event->attendees();
    CalendarData::Attendee attendee;
    for (const KCalendarCore::Attendee &calAttendee : attendees) {
        attendee.name = calAttendee.name();
        attendee.email = calAttendee.email();
        attendee.isOrganizer = (attendee.email == calOrganizer.email());
        if (attendee.isOrganizer) {
            // If organizer is in the attendee list, we prioritize the
            // details from the attendee structure.
            attendee.participationRole = KCalendarCore::Attendee::Chair;
            result.prepend(attendee);
        } else {
            attendee.status = calAttendee.status();
            attendee.participationRole = calAttendee.role();
            result.append(attendee);
        }
    }

    if (result.isEmpty() || !result.first().isOrganizer) {
        // We always prepend the organizer in the list returned to QML.
        // If it was not present in the attendee list, we create one attendee
        // from the data in the ::Person structure.
        CalendarData::Attendee organizer;
        organizer.isOrganizer = true;
        organizer.name = calOrganizer.name();
        organizer.email = calOrganizer.email();
        organizer.participationRole = KCalendarCore::Attendee::Chair;
        result.prepend(organizer);
    }

    return result;
}

QList<QObject *> CalendarUtils::convertAttendeeList(const QList<CalendarData::Attendee> &list)
{
    QList<QObject*> result;
    foreach (const CalendarData::Attendee &attendee, list) {
        Person::AttendeeRole role;
        switch (attendee.participationRole) {
        case KCalendarCore::Attendee::ReqParticipant:
            role = Person::RequiredParticipant;
            break;
        case KCalendarCore::Attendee::OptParticipant:
            role = Person::OptionalParticipant;
            break;
        case KCalendarCore::Attendee::Chair:
            role = Person::ChairParticipant;
            break;
        default:
            role = Person::NonParticipant;
            break;
        }

        Person::ParticipationStatus status;
        switch (attendee.status) {
        case KCalendarCore::Attendee::Accepted:
            status = Person::AcceptedParticipation;
            break;
        case KCalendarCore::Attendee::Declined:
            status = Person::DeclinedParticipation;
            break;
        case KCalendarCore::Attendee::Tentative:
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

CalendarData::EventOccurrence CalendarUtils::getNextOccurrence(const KCalendarCore::Event::Ptr &event,
                                                               const QDateTime &start,
                                                               const KCalendarCore::Incidence::List &exceptions)
{
    const QTimeZone systemTimeZone = QTimeZone::systemTimeZone();

    CalendarData::EventOccurrence occurrence;
    if (event) {
        occurrence.eventUid = event->uid();
        occurrence.recurrenceId = event->recurrenceId();
        occurrence.eventAllDay = event->allDay();
        occurrence.startTime = event->dtStart().toTimeZone(systemTimeZone);
        occurrence.endTime = event->dtEnd().toTimeZone(systemTimeZone);

        if (!start.isNull() && event->recurs()) {
            KCalendarCore::Recurrence *recurrence = event->recurrence();
            QSet<QDateTime> recurrenceIds;
            for (const KCalendarCore::Incidence::Ptr &exception : exceptions)
                recurrenceIds.insert(exception->recurrenceId());
            const KCalendarCore::Duration period(event->dtStart(), event->dtEnd());

            QDateTime match;
            if (recurrence->recursAt(start) && !recurrenceIds.contains(start))
                match = start;
            if (match.isNull()) {
                match = start;
                do {
                    match = recurrence->getNextDateTime(match);
                } while (match.isValid() && recurrenceIds.contains(match));
            }
            if (match.isNull()) {
                match = start;
                do {
                    match = recurrence->getPreviousDateTime(match);
                } while (match.isValid() && recurrenceIds.contains(match));
            }
            if (match.isValid()) {
                occurrence.startTime = match.toTimeZone(systemTimeZone);
                occurrence.endTime = period.end(match).toTimeZone(systemTimeZone);
            }
        }
    }

    return occurrence;
}

bool CalendarUtils::importFromFile(const QString &fileName,
                                   KCalendarCore::Calendar::Ptr calendar)
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
        KCalendarCore::VCalFormat vcalFormat;
        ok = vcalFormat.fromRawString(calendar, fileContent);
    } else if (filePath.endsWith(".ics")) {
        KCalendarCore::ICalFormat icalFormat;
        ok = icalFormat.fromRawString(calendar, fileContent);
    }
    if (!ok)
        qWarning() << "Failed to import from file" << filePath;

    return ok;
}

bool CalendarUtils::importFromIcsRawData(const QByteArray &icsData,
                                         KCalendarCore::Calendar::Ptr calendar)
{
    bool ok = false;
    KCalendarCore::ICalFormat icalFormat;
    ok = icalFormat.fromRawString(calendar, icsData);
    if (!ok)
        qWarning() << "Failed to import from raw data";

    return ok;
}

CalendarEvent::Response CalendarUtils::convertPartStat(KCalendarCore::Attendee::PartStat status)
{
    switch (status) {
    case KCalendarCore::Attendee::Accepted:
        return CalendarEvent::ResponseAccept;
    case KCalendarCore::Attendee::Declined:
        return CalendarEvent::ResponseDecline;
    case KCalendarCore::Attendee::Tentative:
        return CalendarEvent::ResponseTentative;
    case KCalendarCore::Attendee::NeedsAction:
    case KCalendarCore::Attendee::None:
    default:
        return CalendarEvent::ResponseUnspecified;
    }
}

KCalendarCore::Attendee::PartStat CalendarUtils::convertResponse(CalendarEvent::Response response)
{
    switch (response) {
    case CalendarEvent::ResponseAccept:
        return KCalendarCore::Attendee::Accepted;
    case CalendarEvent::ResponseTentative:
        return KCalendarCore::Attendee::Tentative;
    case CalendarEvent::ResponseDecline:
        return KCalendarCore::Attendee::Declined;
    default:
        return KCalendarCore::Attendee::NeedsAction;
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

QString CalendarUtils::recurrenceIdToString(const QDateTime &dt)
{
    // Convert to Qt::OffsetFromUTC spec to ensure time zone offset is included in string format,
    // to be consistent with previous versions that used KDateTime::toString() to produce the
    // same string format for recurrence ids.
    return dt.toOffsetFromUtc(dt.offsetFromUtc()).toString(Qt::ISODate);
}
