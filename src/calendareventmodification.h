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

#ifndef CALENDAREVENTMODIFICATION_H
#define CALENDAREVENTMODIFICATION_H

#include <QObject>

#include "calendardata.h"
#include "calendarevent.h"
#include "calendareventoccurrence.h"
#include "calendarchangeinformation.h"
#include "calendarcontactmodel.h"

class CalendarEventModification : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString displayLabel READ displayLabel WRITE setDisplayLabel NOTIFY displayLabelChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY startTimeChanged)
    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY endTimeChanged)
    Q_PROPERTY(bool allDay READ allDay WRITE setAllDay NOTIFY allDayChanged)
    Q_PROPERTY(CalendarEvent::Recur recur READ recur WRITE setRecur NOTIFY recurChanged)
    Q_PROPERTY(QDateTime recurEndDate READ recurEndDate NOTIFY recurEndDateChanged)
    Q_PROPERTY(CalendarEvent::Days recurWeeklyDays READ recurWeeklyDays WRITE setRecurWeeklyDays NOTIFY recurWeeklyDaysChanged)
    Q_PROPERTY(bool hasRecurEndDate READ hasRecurEndDate NOTIFY hasRecurEndDateChanged)
    Q_PROPERTY(QString recurrenceId READ recurrenceIdString CONSTANT)
    Q_PROPERTY(int reminder READ reminder WRITE setReminder NOTIFY reminderChanged)
    Q_PROPERTY(QString location READ location WRITE setLocation NOTIFY locationChanged)
    Q_PROPERTY(QString calendarUid READ calendarUid WRITE setCalendarUid NOTIFY calendarUidChanged)

public:
    CalendarEventModification(CalendarData::Event data, QObject *parent = 0);
    explicit CalendarEventModification(QObject *parent = 0);
    ~CalendarEventModification();

    QString displayLabel() const;
    void setDisplayLabel(const QString &displayLabel);

    QString description() const;
    void setDescription(const QString &description);

    QDateTime startTime() const;
    Q_INVOKABLE void setStartTime(const QDateTime &startTime, Qt::TimeSpec spec, const QString &timezone = QString());

    QDateTime endTime() const;
    Q_INVOKABLE void setEndTime(const QDateTime &endTime, Qt::TimeSpec spec, const QString &timezone = QString());

    bool allDay() const;
    void setAllDay(bool);

    CalendarEvent::Recur recur() const;
    void setRecur(CalendarEvent::Recur);

    QDateTime recurEndDate() const;
    bool hasRecurEndDate() const;
    Q_INVOKABLE void setRecurEndDate(const QDateTime &dateTime);
    Q_INVOKABLE void unsetRecurEndDate();

    CalendarEvent::Days recurWeeklyDays() const;
    void setRecurWeeklyDays(CalendarEvent::Days days);

    QString recurrenceIdString() const;

    int reminder() const;
    void setReminder(int seconds);

    QString location() const;
    void setLocation(const QString &newLocation);

    QString calendarUid() const;
    void setCalendarUid(const QString &uid);

    Q_INVOKABLE void setAttendees(CalendarContactModel *required, CalendarContactModel *optional);

    Q_INVOKABLE void save();
    Q_INVOKABLE CalendarChangeInformation * replaceOccurrence(CalendarEventOccurrence *occurrence);

signals:
    void displayLabelChanged();
    void descriptionChanged();
    void startTimeChanged();
    void endTimeChanged();
    void allDayChanged();
    void recurChanged();
    void recurWeeklyDaysChanged();
    void reminderChanged();
    void locationChanged();
    void recurEndDateChanged();
    void hasRecurEndDateChanged();
    void calendarUidChanged();

private:
    CalendarData::Event m_event;
    bool m_attendeesSet;
    QList<CalendarData::EmailContact> m_requiredAttendees;
    QList<CalendarData::EmailContact> m_optionalAttendees;
};

#endif
