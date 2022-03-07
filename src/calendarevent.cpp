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
#include <QBitArray>

#include "calendarutils.h"
#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "calendardata.h"

CalendarEvent::CalendarEvent(KCalendarCore::Incidence::Ptr data, QObject *parent)
    : QObject(parent), mIncidence(data ? data : KCalendarCore::Incidence::Ptr(new KCalendarCore::Event))
{
    cacheIncidence();
}

CalendarEvent::CalendarEvent(const CalendarEvent *other, QObject *parent)
    : CalendarEvent(KCalendarCore::Incidence::Ptr(other ? other->mIncidence->clone() : nullptr), parent)
{
    if (other) {
        mCalendarUid = other->mCalendarUid;
        mReadOnly = other->mReadOnly;
        mRSVP = other->mRSVP;
        mExternalInvitation = other->mExternalInvitation;
        mOwnerStatus = other->mOwnerStatus;
    }
}

CalendarEvent::~CalendarEvent()
{
}

QString CalendarEvent::displayLabel() const
{
    return mIncidence->summary();
}

QString CalendarEvent::description() const
{
    return mIncidence->description();
}

QDateTime CalendarEvent::startTime() const
{
    // Cannot return the date time directly here. If UTC, the QDateTime
    // will be in UTC also and the UI will convert it to local when displaying
    // the time, while in every other case, it set the QDateTime in
    // local zone.
    const QDateTime dt = mIncidence->dtStart();
    return QDateTime(dt.date(), dt.time());
}

QDateTime CalendarEvent::endTime() const
{
    const QDateTime dt = mIncidence->dateTime(KCalendarCore::Incidence::RoleEnd);
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
    return toTimeSpec(mIncidence->dtStart());
}

Qt::TimeSpec CalendarEvent::endTimeSpec() const
{
    return toTimeSpec(mIncidence->dateTime(KCalendarCore::Incidence::RoleEnd));
}

QString CalendarEvent::startTimeZone() const
{
    return QString::fromLatin1(mIncidence->dtStart().timeZone().id());
}

QString CalendarEvent::endTimeZone() const
{
    return QString::fromLatin1(mIncidence->dateTime(KCalendarCore::Incidence::RoleEnd).timeZone().id());
}

bool CalendarEvent::allDay() const
{
    return mIncidence->allDay();
}

CalendarEvent::Recur CalendarEvent::recur() const
{
    return mRecur;
}

QDateTime CalendarEvent::recurEndDate() const
{
    return QDateTime(mRecurEndDate);
}

bool CalendarEvent::hasRecurEndDate() const
{
    return mRecurEndDate.isValid();
}

CalendarEvent::Days CalendarEvent::recurWeeklyDays() const
{
    return mRecurWeeklyDays;
}

int CalendarEvent::reminder() const
{
    return mReminder;
}

QDateTime CalendarEvent::reminderDateTime() const
{
    return mReminderDateTime;
}

QString CalendarEvent::uniqueId() const
{
    return mIncidence->uid();
}

bool CalendarEvent::readOnly() const
{
    return mReadOnly;
}

QString CalendarEvent::calendarUid() const
{
    return mCalendarUid;
}

QString CalendarEvent::location() const
{
    return mIncidence->location();
}

CalendarEvent::Secrecy CalendarEvent::secrecy() const
{
    KCalendarCore::Incidence::Secrecy s = mIncidence->secrecy();
    switch (s) {
    case KCalendarCore::Incidence::SecrecyPrivate:
        return CalendarEvent::SecrecyPrivate;
    case KCalendarCore::Incidence::SecrecyConfidential:
        return CalendarEvent::SecrecyConfidential;
    case KCalendarCore::Incidence::SecrecyPublic:
    default:
        return CalendarEvent::SecrecyPublic;
    }
}

CalendarEvent::Status CalendarEvent::status() const
{
    switch (mIncidence->status()) {
    case KCalendarCore::Incidence::StatusTentative:
        return CalendarEvent::StatusTentative;
    case KCalendarCore::Incidence::StatusConfirmed:
        return CalendarEvent::StatusConfirmed;
    case KCalendarCore::Incidence::StatusCanceled:
        return CalendarEvent::StatusCancelled;
    case KCalendarCore::Incidence::StatusNone:
    default:
        return CalendarEvent::StatusNone;
    }
}

CalendarEvent::SyncFailure CalendarEvent::syncFailure() const
{
    const QString &syncFailure = mIncidence->customProperty("VOLATILE", "SYNC-FAILURE");
    if (syncFailure.compare("upload-new", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::CreationFailure;
    } else if (syncFailure.compare("upload", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::UploadFailure;
    } else if (syncFailure.compare("update", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::UpdateFailure;
    } else if (syncFailure.compare("delete", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::DeleteFailure;
    }
    return CalendarEvent::NoSyncFailure;
}

CalendarEvent::SyncFailureResolution CalendarEvent::syncFailureResolution() const
{
    const QString &syncResolution = mIncidence->customProperty("VOLATILE", "SYNC-FAILURE-RESOLUTION");
    if (syncResolution.compare("keep-out-of-sync", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::KeepOutOfSync;
    } else if (syncResolution.compare("server-reset", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::PullServerData;
    } else if (syncResolution.compare("device-reset", Qt::CaseInsensitive) == 0) {
        return CalendarEvent::PushDeviceData;
    } else if (!syncResolution.isEmpty()) {
        qWarning() << "unsupported sync failure resolution" << syncResolution;
    }
    return CalendarEvent::RetrySync;
}

CalendarEvent::Response CalendarEvent::ownerStatus() const
{
    return mOwnerStatus;
}

bool CalendarEvent::rsvp() const
{
    return mRSVP;
}

bool CalendarEvent::externalInvitation() const
{
    return mExternalInvitation;
}

QDateTime CalendarEvent::recurrenceId() const
{
    return mIncidence->recurrenceId();
}

QString CalendarEvent::recurrenceIdString() const
{
    if (mIncidence->hasRecurrenceId()) {
        return CalendarUtils::recurrenceIdToString(mIncidence->recurrenceId());
    } else {
        return QString();
    }
}

void CalendarEvent::cacheIncidence()
{
    mRecur = fromKRecurrence();
    mRecurWeeklyDays = fromKDayPositions();
    mRecurEndDate = QDate();
    if (mIncidence->recurs()) {
        KCalendarCore::RecurrenceRule *defaultRule = mIncidence->recurrence()->defaultRRule();
        if (defaultRule) {
            mRecurEndDate = defaultRule->endDt().date();
        }
    }
    mReminder = fromKReminder();
    mReminderDateTime = fromKReminderDateTime();
}

CalendarEvent::Recur CalendarEvent::fromKRecurrence() const
{
    if (!mIncidence->recurs())
        return CalendarEvent::RecurOnce;

    if (mIncidence->recurrence()->rRules().count() != 1)
        return CalendarEvent::RecurCustom;

    ushort rt = mIncidence->recurrence()->recurrenceType();
    int freq = mIncidence->recurrence()->frequency();

    if (rt == KCalendarCore::Recurrence::rDaily && freq == 1) {
        return CalendarEvent::RecurDaily;
    } else if (rt == KCalendarCore::Recurrence::rWeekly && freq == 1) {
        if (mIncidence->recurrence()->days().count(true) == 0) {
            return CalendarEvent::RecurWeekly;
        } else {
            return CalendarEvent::RecurWeeklyByDays;
        }
    } else if (rt == KCalendarCore::Recurrence::rWeekly && freq == 2 && mIncidence->recurrence()->days().count(true) == 0) {
        return CalendarEvent::RecurBiweekly;
    } else if (rt == KCalendarCore::Recurrence::rMonthlyDay && freq == 1) {
        return CalendarEvent::RecurMonthly;
    } else if (rt == KCalendarCore::Recurrence::rMonthlyPos && freq == 1) {
        const QList<KCalendarCore::RecurrenceRule::WDayPos> monthPositions = mIncidence->recurrence()->monthPositions();
        if (monthPositions.length() == 1
            && monthPositions.first().day() == mIncidence->dtStart().date().dayOfWeek()) {
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

CalendarEvent::Days CalendarEvent::fromKDayPositions() const
{
    if (!mIncidence->recurs())
        return CalendarEvent::NoDays;

    if (mIncidence->recurrence()->rRules().count() != 1)
        return CalendarEvent::NoDays;

    if (mIncidence->recurrence()->recurrenceType() != KCalendarCore::Recurrence::rWeekly
        || mIncidence->recurrence()->frequency() != 1)
        return CalendarEvent::NoDays;

    const CalendarEvent::Day week[7] = {CalendarEvent::Monday,
                                        CalendarEvent::Tuesday,
                                        CalendarEvent::Wednesday,
                                        CalendarEvent::Thursday,
                                        CalendarEvent::Friday,
                                        CalendarEvent::Saturday,
                                        CalendarEvent::Sunday};

    const QList<KCalendarCore::RecurrenceRule::WDayPos> monthPositions = mIncidence->recurrence()->monthPositions();
    CalendarEvent::Days days = CalendarEvent::NoDays;
    for (QList<KCalendarCore::RecurrenceRule::WDayPos>::ConstIterator it = monthPositions.constBegin();
         it != monthPositions.constEnd(); ++it) {
        days |= week[it->day() - 1];
    }
    return days;
}

int CalendarEvent::fromKReminder() const
{
    KCalendarCore::Alarm::List alarms = mIncidence->alarms();

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

QDateTime CalendarEvent::fromKReminderDateTime() const
{
    for (const KCalendarCore::Alarm::Ptr &alarm : mIncidence->alarms()) {
        if (alarm && alarm->type() == KCalendarCore::Alarm::Display && alarm->hasTime()) {
            return alarm->time();
        }
    }

    return QDateTime();
}

CalendarStoredEvent::CalendarStoredEvent(CalendarManager *manager, KCalendarCore::Incidence::Ptr data,
                                         const CalendarData::Notebook *notebook)
    : CalendarEvent(data, manager)
    , mManager(manager)
{
    cacheIncidence(notebook);

    connect(mManager, SIGNAL(notebookColorChanged(QString)),
            this, SLOT(notebookColorChanged(QString)));
    connect(mManager, SIGNAL(eventUidChanged(QString,QString)),
            this, SLOT(eventUidChanged(QString,QString)));
}

CalendarStoredEvent::~CalendarStoredEvent()
{
}

void CalendarStoredEvent::cacheIncidence(const CalendarData::Notebook *notebook)
{
    mCalendarUid = notebook->uid;
    mReadOnly = notebook->readOnly;
    mNotebookColor = notebook->color;

    const QString &calendarOwnerEmail = notebook->emailAddress;
    const KCalendarCore::Person &organizer = mIncidence->organizer();
    const QString organizerEmail = organizer.email();
    mExternalInvitation = (!organizerEmail.isEmpty() && organizerEmail != calendarOwnerEmail
                           && notebook->sharedWith.contains(organizerEmail));

    // It would be good to set the attendance status directly in the event within the plugin,
    // however in some cases the account email and owner attendee email won't necessarily match
    // (e.g. in the case where server-side aliases are defined but unknown to the plugin).
    // So we handle this here to avoid "missing" some status changes due to owner email mismatch.
    // This defaults to QString() -> ResponseUnspecified in case the property is undefined
    // QString::toInt() conversion defaults to 0 on failure
    switch (mIncidence->nonKDECustomProperty("X-EAS-RESPONSE-TYPE").toInt()) {
    case 1: // OrganizerResponseType (Organizer's acceptance is implicit)
    case 3: // AcceptedResponseType
        mOwnerStatus = CalendarEvent::ResponseAccept;
        break;
    case 2: // TentativeResponseType
        mOwnerStatus = CalendarEvent::ResponseTentative;
        break;
    case 4: // DeclinedResponseType
        mOwnerStatus = CalendarEvent::ResponseDecline;
        break;
    case -1: // ResponseTypeUnset
    case 0: // NoneResponseType
    case 5: // NotRespondedResponseType
    default:
        mOwnerStatus = CalendarEvent::ResponseUnspecified;
        break;
    }
    const KCalendarCore::Attendee::List attendees = mIncidence->attendees();
    for (const KCalendarCore::Attendee &calAttendee : attendees) {
        if (calAttendee.email() == calendarOwnerEmail) {
            // Override the ResponseType
            switch (calAttendee.status()) {
            case KCalendarCore::Attendee::Accepted:
                mOwnerStatus = CalendarEvent::ResponseAccept;
                break;
            case KCalendarCore::Attendee::Declined:
                mOwnerStatus = CalendarEvent::ResponseDecline;
                break;
            case KCalendarCore::Attendee::Tentative:
                mOwnerStatus = CalendarEvent::ResponseTentative;
                break;
            default:
                break;
            }            
            //TODO: KCalendarCore::Attendee::RSVP() returns false even if response was requested for some accounts like Google.
            // We can use attendee role until the problem is not fixed (probably in Google plugin).
            // To be updated later when google account support for responses is added.
            mRSVP = calAttendee.RSVP();// || calAttendee->role() != KCalendarCore::Attendee::Chair;
        }
    }
}

void CalendarStoredEvent::notebookColorChanged(QString notebookUid)
{
    if (mCalendarUid == notebookUid)
        emit colorChanged();
}

void CalendarStoredEvent::eventUidChanged(QString oldUid, QString newUid)
{
    if (mIncidence->uid() == oldUid) {
        mIncidence->setUid(newUid);
        emit uniqueIdChanged();
        // Event uid changes when the event is moved between notebooks, calendar uid has changed
        emit calendarUidChanged();
    }
}

bool CalendarStoredEvent::sendResponse(int response)
{
    if (mManager->sendResponse(mIncidence->uid(), mIncidence->recurrenceId(), (Response)response)) {
        mManager->save();
        return true;
    } else {
        return false;
    }
}

void CalendarStoredEvent::deleteEvent()
{
    mManager->deleteEvent(mIncidence->uid(), mIncidence->recurrenceId(), QDateTime());
    mManager->save();
}

// Returns the event as a iCalendar string
QString CalendarStoredEvent::iCalendar(const QString &prodId) const
{
    if (mIncidence->uid().isEmpty()) {
        qWarning() << "Event has no uid, returning empty iCalendar string."
                   << "Save event before calling this function";
        return QString();
    }

    return mManager->convertEventToICalendarSync(mIncidence->uid(), prodId);
}

QString CalendarStoredEvent::color() const
{
    return mNotebookColor;
}

void CalendarStoredEvent::setEvent(const KCalendarCore::Incidence::Ptr &incidence,
                                   const CalendarData::Notebook *notebook)
{
    if (!incidence)
        return;

    CalendarEvent::Recur oldRecur = mRecur;
    CalendarEvent::Days oldDays = mRecurWeeklyDays;
    int oldReminder = mReminder;
    QDateTime oldReminderDateTime = mReminderDateTime;
    CalendarEvent::SyncFailure oldSyncFailure = syncFailure();
    CalendarEvent::SyncFailureResolution oldSyncFailureResolution = syncFailureResolution();
    bool oldRSVP = mRSVP;
    bool oldExternalInvitation = mExternalInvitation;
    CalendarEvent::Response oldOwnerStatus = mOwnerStatus;

    const bool mAllDayChanged = mIncidence->allDay() != incidence->allDay();
    const bool mSummaryChanged = mIncidence->summary() != incidence->summary();
    const bool mDescriptionChanged = mIncidence->description() != incidence->description();
    const bool mDtEndChanged = mIncidence->type() == KCalendarCore::IncidenceBase::TypeEvent
        && incidence->type() == KCalendarCore::IncidenceBase::TypeEvent
        && mIncidence.staticCast<KCalendarCore::Event>()->dtEnd() != incidence.staticCast<KCalendarCore::Event>()->dtEnd();
    const bool mLocationChanged = mIncidence->location() != incidence->location();
    const bool mSecrecyChanged = mIncidence->secrecy() != incidence->secrecy();
    const bool mStatusChanged = mIncidence->status() != incidence->status();
    const bool mDtStartChanged = mIncidence->dtStart() != incidence->dtStart();
    const bool mNbColorChanged = notebook->color != mNotebookColor;

    mIncidence = incidence;
    CalendarEvent::cacheIncidence();
    cacheIncidence(notebook);

    if (mAllDayChanged)
        emit allDayChanged();
    if (mSummaryChanged)
        emit displayLabelChanged();
    if (mDescriptionChanged)
        emit descriptionChanged();
    if (mDtEndChanged)
        emit endTimeChanged();
    if (mLocationChanged)
        emit locationChanged();
    if (mSecrecyChanged)
        emit secrecyChanged();
    if (mStatusChanged)
        emit statusChanged();
    if (mRecur != oldRecur)
        emit recurChanged();
    if (mReminder != oldReminder)
        emit reminderChanged();
    if (mReminderDateTime != oldReminderDateTime)
        emit reminderDateTimeChanged();
    if (mDtStartChanged)
        emit startTimeChanged();
    if (mRecurWeeklyDays != oldDays)
        emit recurWeeklyDaysChanged();
    if (mRSVP != oldRSVP)
        emit rsvpChanged();
    if (mExternalInvitation != oldExternalInvitation)
        emit externalInvitationChanged();
    if (mOwnerStatus != oldOwnerStatus)
        emit ownerStatusChanged();
    if (syncFailure() != oldSyncFailure)
        emit syncFailureChanged();
    if (syncFailureResolution() != oldSyncFailureResolution)
        emit syncFailureResolutionChanged();
    if (mNbColorChanged)
        emit colorChanged();
}

KCalendarCore::Incidence::Ptr CalendarStoredEvent::dissociateSingleOccurrence(const CalendarEventOccurrence *occurrence) const
{
    return occurrence ? mManager->dissociateSingleOccurrence(mIncidence->uid(), occurrence->startTime()) : KCalendarCore::Incidence::Ptr();
}
