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

#ifndef CALENDAREVENT_H
#define CALENDAREVENT_H

#include <QObject>
#include <QDateTime>

#include <KCalendarCore/Incidence>

#include "calendardata.h"

class CalendarEvent : public QObject
{
    Q_OBJECT
    Q_ENUMS(Reminder)
    Q_ENUMS(Secrecy)
    Q_ENUMS(Response)

    Q_PROPERTY(QString displayLabel READ displayLabel NOTIFY displayLabelChanged)
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)
    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY startTimeChanged)
    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY endTimeChanged)
    Q_PROPERTY(Qt::TimeSpec startTimeSpec READ startTimeSpec NOTIFY startTimeChanged)
    Q_PROPERTY(Qt::TimeSpec endTimeSpec READ endTimeSpec NOTIFY endTimeChanged)
    Q_PROPERTY(QString startTimeZone READ startTimeZone NOTIFY startTimeChanged)
    Q_PROPERTY(QString endTimeZone READ endTimeZone NOTIFY endTimeChanged)
    Q_PROPERTY(bool allDay READ allDay NOTIFY allDayChanged)
    Q_PROPERTY(CalendarEvent::Recur recur READ recur NOTIFY recurChanged)
    Q_PROPERTY(QDateTime recurEndDate READ recurEndDate NOTIFY recurEndDateChanged)
    Q_PROPERTY(bool hasRecurEndDate READ hasRecurEndDate NOTIFY hasRecurEndDateChanged)
    Q_PROPERTY(CalendarEvent::Days recurWeeklyDays READ recurWeeklyDays NOTIFY recurWeeklyDaysChanged)
    Q_PROPERTY(int reminder READ reminder NOTIFY reminderChanged)
    Q_PROPERTY(QDateTime reminderDateTime READ reminderDateTime NOTIFY reminderDateTimeChanged)
    Q_PROPERTY(QString uniqueId READ uniqueId NOTIFY uniqueIdChanged)
    Q_PROPERTY(QString recurrenceId READ recurrenceIdString CONSTANT)
    Q_PROPERTY(bool readOnly READ readOnly CONSTANT)
    Q_PROPERTY(QString calendarUid READ calendarUid NOTIFY calendarUidChanged)
    Q_PROPERTY(QString location READ location NOTIFY locationChanged)
    Q_PROPERTY(CalendarEvent::Secrecy secrecy READ secrecy NOTIFY secrecyChanged)
    Q_PROPERTY(CalendarEvent::Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(CalendarEvent::SyncFailure syncFailure READ syncFailure NOTIFY syncFailureChanged)
    Q_PROPERTY(CalendarEvent::SyncFailureResolution syncFailureResolution READ syncFailureResolution NOTIFY syncFailureResolutionChanged)
    Q_PROPERTY(CalendarEvent::Response ownerStatus READ ownerStatus NOTIFY ownerStatusChanged)
    Q_PROPERTY(bool rsvp READ rsvp NOTIFY rsvpChanged)
    Q_PROPERTY(bool externalInvitation READ externalInvitation NOTIFY externalInvitationChanged)

public:
    enum Recur {
        RecurOnce,
        RecurDaily,
        RecurWeekly,
        RecurBiweekly,
        RecurWeeklyByDays,
        RecurMonthly,
        RecurMonthlyByDayOfWeek,
        RecurMonthlyByLastDayOfWeek,
        RecurYearly,
        RecurCustom
    };
    Q_ENUM(Recur)

    enum Day {
        NoDays    = 0x00,
        Monday    = 0x01,
        Tuesday   = 0x02,
        Wednesday = 0x04,
        Thursday  = 0x08,
        Friday    = 0x10,
        Saturday  = 0x20,
        Sunday    = 0x40
    };
    Q_DECLARE_FLAGS(Days, Day)
    Q_FLAG(Days)

    enum Secrecy {
        SecrecyPublic,
        SecrecyPrivate,
        SecrecyConfidential
    };

    enum Response {
        ResponseUnspecified,
        ResponseAccept,
        ResponseTentative,
        ResponseDecline
    };

    enum SyncFailure {
        NoSyncFailure,
        CreationFailure,
        UploadFailure,
        UpdateFailure,
        DeleteFailure
    };
    Q_ENUM(SyncFailure)

    enum SyncFailureResolution {
        RetrySync,
        KeepOutOfSync,
        PushDeviceData,
        PullServerData
    };
    Q_ENUM(SyncFailureResolution)

    enum Status {
        StatusNone,
        StatusTentative,
        StatusConfirmed,
        StatusCancelled
    };
    Q_ENUM(Status)

    CalendarEvent(KCalendarCore::Incidence::Ptr data, QObject *parent);
    CalendarEvent(const CalendarEvent *other, QObject *parent);
    ~CalendarEvent();

    QString displayLabel() const;
    QString description() const;
    virtual QDateTime startTime() const;
    virtual QDateTime endTime() const;
    Qt::TimeSpec startTimeSpec() const;
    Qt::TimeSpec endTimeSpec() const;
    QString startTimeZone() const;
    QString endTimeZone() const;
    bool allDay() const;
    Recur recur() const;
    QDateTime recurEndDate() const;
    bool hasRecurEndDate() const;
    Days recurWeeklyDays() const;
    int reminder() const;
    QDateTime reminderDateTime() const;
    QString uniqueId() const;
    QString calendarUid() const;
    QString location() const;
    QDateTime recurrenceId() const;
    QString recurrenceIdString() const;
    Secrecy secrecy() const;
    Status status() const;
    SyncFailure syncFailure() const;
    SyncFailureResolution syncFailureResolution() const;
    virtual bool readOnly() const;
    virtual Response ownerStatus() const;
    virtual bool rsvp() const;
    virtual bool externalInvitation() const;
    QList<QObject *> attendees() const;

signals:
    void displayLabelChanged();
    void descriptionChanged();
    void startTimeChanged();
    void endTimeChanged();
    void allDayChanged();
    void recurChanged();
    void reminderChanged();
    void reminderDateTimeChanged();
    void uniqueIdChanged();
    void calendarUidChanged();
    void locationChanged();
    void recurEndDateChanged();
    void hasRecurEndDateChanged();
    void recurWeeklyDaysChanged();
    void secrecyChanged();
    void statusChanged();
    void syncFailureChanged();
    void syncFailureResolutionChanged();
    void ownerStatusChanged();
    void rsvpChanged();
    void externalInvitationChanged();

protected:
    void cacheIncidence();
    int fromKReminder() const;
    QDateTime fromKReminderDateTime() const;
    CalendarEvent::Recur fromKRecurrence() const;
    CalendarEvent::Days fromKDayPositions() const;

    KCalendarCore::Incidence::Ptr mIncidence;
    // Cached values, requiring a processing from mIncidence.
    Recur mRecur = RecurOnce;
    Days mRecurWeeklyDays = NoDays;
    QDate mRecurEndDate;
    int mReminder = -1;
    QDateTime mReminderDateTime;
    // Exists in the public API, but should be in CalendarStoredEvent
    QString mCalendarUid;
    QString mCalendarEmail;
    bool mReadOnly = false;
    bool mRSVP = false;
    bool mExternalInvitation = false;
    Response mOwnerStatus = ResponseUnspecified;
};

class CalendarManager;
class CalendarEventOccurrence;
class CalendarStoredEvent : public CalendarEvent
{
    Q_OBJECT
    Q_PROPERTY(QString color READ color NOTIFY colorChanged)
public:
    CalendarStoredEvent(CalendarManager *manager, KCalendarCore::Incidence::Ptr data,
                        const CalendarData::Notebook &notebook);
    ~CalendarStoredEvent();

    void setEvent(const KCalendarCore::Incidence::Ptr &incidence, const CalendarData::Notebook &notebook);
    KCalendarCore::Incidence::Ptr dissociateSingleOccurrence(const CalendarEventOccurrence *occurrence) const;

    QString color() const;

    Q_INVOKABLE bool sendResponse(int response);
    Q_INVOKABLE QString iCalendar(const QString &prodId = QString()) const;
    Q_INVOKABLE void deleteEvent();

signals:
    void colorChanged();

private slots:
    void notebookColorChanged(QString notebookUid);
    void eventUidChanged(QString oldUid, QString newUid);

private:
    void cacheIncidence(const CalendarData::Notebook &notebook);

    QString mNotebookColor;
    CalendarManager *mManager;
};

#endif // CALENDAREVENT_H
