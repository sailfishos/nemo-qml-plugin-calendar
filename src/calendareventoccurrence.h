/*
 * Copyright (C) 2013 - 2019 Jolla Ltd.
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#ifndef CALENDAREVENTOCCURRENCE_H
#define CALENDAREVENTOCCURRENCE_H

#include <QObject>
#include <QDateTime>

#include "calendardata.h"

class CalendarEvent;

class CalendarEventOccurrence : public QObject
{
    Q_OBJECT
    // startTime and endTime are givent in local time.
    Q_PROPERTY(QDateTime startTime READ startTime CONSTANT)
    Q_PROPERTY(QDateTime endTime READ endTime CONSTANT)
    // startTimeInTz and endTimeInTz are given in event startTime / endTime timezone
    Q_PROPERTY(QDateTime startTimeInTz READ startTimeInTz CONSTANT)
    Q_PROPERTY(QDateTime endTimeInTz READ endTimeInTz CONSTANT)
    Q_PROPERTY(QString displayLabel READ displayLabel CONSTANT)
    Q_PROPERTY(CalendarEvent *event READ eventObject CONSTANT)

public:
    CalendarEventOccurrence(const QString &eventUid,
                            const QDateTime &recurrenceId,
                            const QDateTime &startTime,
                            const QDateTime &endTime,
                            QObject *parent = 0);
    CalendarEventOccurrence(const CalendarData::EventOccurrence &occurrence,
                            QObject *parent = 0);
    ~CalendarEventOccurrence();

    QDateTime startTime() const;
    QDateTime endTime() const;
    QDateTime startTimeInTz() const;
    QDateTime endTimeInTz() const;
    QString displayLabel() const;
    CalendarEvent *eventObject() const;

private slots:
    void eventUidChanged(QString oldUid, QString newUid);

private:
    QString mEventUid;
    QDateTime mRecurrenceId;
    QDateTime mStartTime;
    QDateTime mEndTime;
    QString mDisplayLabel;
};

#endif // CALENDAREVENTOCCURRENCE_H
