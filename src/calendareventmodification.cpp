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

#include "calendareventmodification.h"
#include "calendarmanager.h"
#include "calendarutils.h"

#include <QTimeZone>
#include <QBitArray>
#include <QDebug>

namespace {

void updateTime(QDateTime *dt, Qt::TimeSpec spec, const QString &timeZone)
{
    if (spec == Qt::TimeZone) {
        QTimeZone tz(timeZone.toUtf8());
        if (tz.isValid()) {
            dt->setTimeZone(tz);
        } else {
            qWarning() << "Cannot find time zone:" << timeZone;
        }
    } else {
        dt->setTimeSpec(spec);
    }
}

}

CalendarEventModification::CalendarEventModification(const CalendarStoredEvent *source, const CalendarEventOccurrence *occurrence, QObject *parent)
    : CalendarEvent(source, parent)
{
    if (source && occurrence)
        mIncidence = source->dissociateSingleOccurrence(occurrence);
}

CalendarEventModification::CalendarEventModification(QObject *parent)
    : CalendarEvent(nullptr, parent)
{
}

CalendarEventModification::~CalendarEventModification()
{
}

QDateTime CalendarEventModification::startTime() const
{
    return mIncidence->dtStart();
}

QDateTime CalendarEventModification::endTime() const
{
    return mIncidence->dateTime(KCalendarCore::Incidence::RoleEnd);
}

void CalendarEventModification::setDisplayLabel(const QString &displayLabel)
{
    if (mIncidence->summary() != displayLabel) {
        mIncidence->setSummary(displayLabel);
        emit displayLabelChanged();
    }
}

void CalendarEventModification::setDescription(const QString &description)
{
    if (mIncidence->description() != description) {
        mIncidence->setDescription(description);
        emit descriptionChanged();
    }
}

void CalendarEventModification::setStartTime(const QDateTime &startTime, Qt::TimeSpec spec, const QString &timezone)
{
    QDateTime newStartTimeInTz = startTime;
    updateTime(&newStartTimeInTz, spec, timezone);
    if (mIncidence->dtStart() != newStartTimeInTz
        || mIncidence->dtStart().timeSpec() != newStartTimeInTz.timeSpec()
        || mIncidence->dtStart().timeZone() != newStartTimeInTz.timeZone()) {
        mIncidence->setDtStart(newStartTimeInTz);
        emit startTimeChanged();
    }
}

void CalendarEventModification::setEndTime(const QDateTime &endTime, Qt::TimeSpec spec, const QString &timezone)
{
    if (mIncidence->type() != KCalendarCore::IncidenceBase::TypeEvent)
        return;
    QDateTime newEndTimeInTz = endTime;
    updateTime(&newEndTimeInTz, spec, timezone);
    const KCalendarCore::Event::Ptr &event = mIncidence.staticCast<KCalendarCore::Event>();
    if (event->dtEnd() != newEndTimeInTz
        || event->dtEnd().timeSpec() != newEndTimeInTz.timeSpec()
        || event->dtEnd().timeZone() != newEndTimeInTz.timeZone()) {
        event->setDtEnd(newEndTimeInTz);
        emit endTimeChanged();
    }
}

void CalendarEventModification::setAllDay(bool allDay)
{
    if (mIncidence->allDay() != allDay) {
        mIncidence->setAllDay(allDay);
        emit allDayChanged();
    }
}

void CalendarEventModification::setRecur(CalendarEvent::Recur recur)
{
    if (mRecur != recur) {
        mRecur = recur;
        emit recurChanged();
    }
}

void CalendarEventModification::setRecurEndDate(const QDateTime &dateTime)
{
    bool wasValid = mRecurEndDate.isValid();
    QDate date = dateTime.date();

    if (mRecurEndDate != date) {
        mRecurEndDate = date;
        emit recurEndDateChanged();

        if (date.isValid() != wasValid) {
            emit hasRecurEndDateChanged();
        }
    }
}

void CalendarEventModification::unsetRecurEndDate()
{
    setRecurEndDate(QDateTime());
}

void CalendarEventModification::setRecurWeeklyDays(CalendarEvent::Days days)
{
    if (mRecurWeeklyDays != days) {
        mRecurWeeklyDays = days;
        emit recurWeeklyDaysChanged();
    }
}

void CalendarEventModification::setReminder(int seconds)
{
    if (seconds != mReminder) {
        mReminder = seconds;
        emit reminderChanged();
    }
}

void CalendarEventModification::setReminderDateTime(const QDateTime &dateTime)
{
    if (dateTime != mReminderDateTime) {
        mReminderDateTime = dateTime;
        emit reminderDateTimeChanged();
    }
}

void CalendarEventModification::setLocation(const QString &newLocation)
{
    if (newLocation != mIncidence->location()) {
        mIncidence->setLocation(newLocation);
        emit locationChanged();
    }
}

void CalendarEventModification::setCalendarUid(const QString &uid)
{
    if (mCalendarUid != uid) {
        mCalendarUid = uid;
        mCalendarEmail = CalendarManager::instance()->getNotebookEmail(uid);
        emit calendarUidChanged();
    }
}

void CalendarEventModification::setAttendees(CalendarContactModel *required, CalendarContactModel *optional)
{
    if (!required || !optional) {
        qWarning() << "Missing attendeeList";
        return;
    }

    m_attendeesSet = true;
    m_requiredAttendees = required->getList();
    m_optionalAttendees = optional->getList();
}

void CalendarEventModification::save()
{
    updateIncidence();
    CalendarManager::instance()->saveModification(mIncidence, mCalendarUid);
}

static bool popByEmail(KCalendarCore::Person::List *list, const QString &email)
{
    KCalendarCore::Person::List::Iterator it
        = std::find_if(list->begin(), list->end(),
                       [email](const KCalendarCore::Person &person)
                       {return person.email() == email;});
    bool found = (it != list->end());
    if (found)
        list->erase(it);
    return found;
}

void CalendarEventModification::updateIncidence() const
{
    if (fromKReminder() != mReminder || fromKReminderDateTime() != mReminderDateTime) {
        KCalendarCore::Alarm::List alarms = mIncidence->alarms();
        for (int ii = 0; ii < alarms.count(); ++ii) {
            if (alarms.at(ii)->type() == KCalendarCore::Alarm::Procedure)
                continue;
            mIncidence->removeAlarm(alarms.at(ii));
        }

        // negative reminder seconds means "no reminder", so only
        // deal with positive (or zero = at time of event) reminders.
        if (mReminder >= 0) {
            KCalendarCore::Alarm::Ptr alarm = mIncidence->newAlarm();
            alarm->setEnabled(true);
            // backend stores as "offset to dtStart", i.e negative if reminder before event.
            alarm->setStartOffset(-1 * mReminder);
            alarm->setType(KCalendarCore::Alarm::Display);
        } else if (mReminderDateTime.isValid()) {
            KCalendarCore::Alarm::Ptr alarm = mIncidence->newAlarm();
            alarm->setEnabled(true);
            alarm->setTime(mReminderDateTime);
            alarm->setType(KCalendarCore::Alarm::Display);
        }
    }

    if (mRecur == CalendarEvent::RecurOnce)
        mIncidence->recurrence()->clear();

    if (fromKRecurrence() != mRecur
        || mRecur == CalendarEvent::RecurMonthlyByDayOfWeek
        || mRecur == CalendarEvent::RecurMonthlyByLastDayOfWeek
        || mRecur == CalendarEvent::RecurWeeklyByDays) {
        switch (mRecur) {
        case CalendarEvent::RecurOnce:
            break;
        case CalendarEvent::RecurDaily:
            mIncidence->recurrence()->setDaily(1);
            break;
        case CalendarEvent::RecurWeekly:
            mIncidence->recurrence()->setWeekly(1);
            break;
        case CalendarEvent::RecurBiweekly:
            mIncidence->recurrence()->setWeekly(2);
            break;
        case CalendarEvent::RecurWeeklyByDays: {
            QBitArray rDays(7);
            rDays.setBit(0, mRecurWeeklyDays & CalendarEvent::Monday);
            rDays.setBit(1, mRecurWeeklyDays & CalendarEvent::Tuesday);
            rDays.setBit(2, mRecurWeeklyDays & CalendarEvent::Wednesday);
            rDays.setBit(3, mRecurWeeklyDays & CalendarEvent::Thursday);
            rDays.setBit(4, mRecurWeeklyDays & CalendarEvent::Friday);
            rDays.setBit(5, mRecurWeeklyDays & CalendarEvent::Saturday);
            rDays.setBit(6, mRecurWeeklyDays & CalendarEvent::Sunday);
            mIncidence->recurrence()->setWeekly(1, rDays);
            break;
        }
        case CalendarEvent::RecurMonthly:
            mIncidence->recurrence()->setMonthly(1);
            break;
        case CalendarEvent::RecurMonthlyByDayOfWeek: {
            mIncidence->recurrence()->setMonthly(1);
            const QDate at(mIncidence->dtStart().date());
            mIncidence->recurrence()->addMonthlyPos((at.day() - 1) / 7 + 1, at.dayOfWeek());
            break;
        }
        case CalendarEvent::RecurMonthlyByLastDayOfWeek: {
            mIncidence->recurrence()->setMonthly(1);
            const QDate at(mIncidence->dtStart().date());
            mIncidence->recurrence()->addMonthlyPos(-1, at.dayOfWeek());
            break;
        }
        case CalendarEvent::RecurYearly:
            mIncidence->recurrence()->setYearly(1);
            break;
        case CalendarEvent::RecurCustom:
            // Unable to handle the recurrence rules, keep the existing ones.
            break;
        }
    }

    if (mRecur != CalendarEvent::RecurOnce) {
        mIncidence->recurrence()->setEndDate(mRecurEndDate);
        if (!mRecurEndDate.isValid()) {
            // Recurrence/RecurrenceRule don't have separate method to clear the end date, and currently
            // setting invalid date doesn't make the duration() indicate recurring infinitely.
            mIncidence->recurrence()->setDuration(-1);
        }
    }

    if (m_attendeesSet) {
        KCalendarCore::Person::List required = m_requiredAttendees;
        KCalendarCore::Person::List optional = m_optionalAttendees;
        const KCalendarCore::Attendee::List oldAttendees = mIncidence->attendees();
        mIncidence->clearAttendees();
        for (const KCalendarCore::Attendee &attendee : oldAttendees) {
            const QString &email = attendee.email();
            if (attendee.role() == KCalendarCore::Attendee::ReqParticipant) {
                if (popByEmail(&required, email)) {
                    mIncidence->addAttendee(attendee);
                } else if (popByEmail(&optional, email)) {
                    KCalendarCore::Attendee at(attendee);
                    at.setRole(KCalendarCore::Attendee::OptParticipant);
                    mIncidence->addAttendee(at);
                }
            } else if (attendee.role() == KCalendarCore::Attendee::OptParticipant) {
                if (popByEmail(&optional, email)) {
                    mIncidence->addAttendee(attendee);
                } else if (popByEmail(&required, email)) {
                    KCalendarCore::Attendee at(attendee);
                    at.setRole(KCalendarCore::Attendee::ReqParticipant);
                    mIncidence->addAttendee(at);
                }
            } else {
                mIncidence->addAttendee(attendee);
            }
        }
        for (const KCalendarCore::Person &person : required) {
            KCalendarCore::Attendee at(person.name(), person.email(), true /* rsvp */,
                                       KCalendarCore::Attendee::NeedsAction,
                                       KCalendarCore::Attendee::ReqParticipant);
            mIncidence->addAttendee(at);
        }
        for (const KCalendarCore::Person &person : optional) {
            KCalendarCore::Attendee at(person.name(), person.email(), true /* rsvp */,
                                       KCalendarCore::Attendee::NeedsAction,
                                       KCalendarCore::Attendee::OptParticipant);
            mIncidence->addAttendee(at);
        }
        // set the notebook email address as the organizer email address
        // if no explicit organizer is set (i.e. assume we are the organizer).
        if (mIncidence->organizer().email().isEmpty() && !mCalendarEmail.isEmpty()) {
            KCalendarCore::Person organizer = mIncidence->organizer();
            organizer.setEmail(mCalendarEmail);
            mIncidence->setOrganizer(organizer);
        }
    }
}

void CalendarEventModification::setSyncFailureResolution(CalendarEvent::SyncFailureResolution resolution)
{
    if (syncFailureResolution() != resolution) {
        if (resolution == CalendarEvent::RetrySync) {
            mIncidence->removeCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION");
        } else if (resolution == CalendarEvent::KeepOutOfSync) {
            mIncidence->setCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION", "keep-out-of-sync");
        } else if (resolution == CalendarEvent::PushDeviceData) {
            mIncidence->setCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION", "device-reset");
        } else if (resolution == CalendarEvent::PullServerData) {
            mIncidence->setCustomProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION", "server-reset");
        } else {
            qWarning() << "No support for sync failure resolution" << resolution;
        }
        emit syncFailureResolutionChanged();
    }
}
