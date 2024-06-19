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

#ifndef CALENDAREVENTQUERY_H
#define CALENDAREVENTQUERY_H

#include <QObject>
#include <QDateTime>
#include <QQmlParserStatus>

#include "calendardata.h"

class CalendarEventOccurrence;

class Person : public QObject
{
    Q_OBJECT
    Q_ENUMS(AttendeeRole)
    Q_ENUMS(ParticipationStatus)
    Q_PROPERTY(QString name READ name CONSTANT FINAL)
    Q_PROPERTY(QString email READ email CONSTANT FINAL)
    Q_PROPERTY(bool isOrganizer READ isOrganizer CONSTANT FINAL)
    Q_PROPERTY(int participationRole READ participationRole CONSTANT FINAL)
    Q_PROPERTY(int participationStatus READ participationStatus CONSTANT FINAL)

public:
    enum AttendeeRole {
        RequiredParticipant,
        OptionalParticipant,
        NonParticipant,
        ChairParticipant
    };

    enum ParticipationStatus {
        UnknownParticipation,
        AcceptedParticipation,
        DeclinedParticipation,
        TentativeParticipation
    };

    Person(const QString &aName, const QString &aEmail, bool aIsOrganizer, AttendeeRole aParticipationRole,
           ParticipationStatus aStatus)
        : m_name(aName), m_email(aEmail), m_isOrganizer(aIsOrganizer), m_participationRole(aParticipationRole),
          m_participationStatus(aStatus)
    {
    }

    bool operator==(const Person &other) const
    {
        return m_name == other.m_name && m_email == other.m_email && m_isOrganizer == other.m_isOrganizer
            && m_participationRole == other.m_participationRole
            && m_participationStatus == other.m_participationStatus;
    }

    QString name() const { return m_name; }
    QString email() const { return m_email; }
    bool isOrganizer() const { return m_isOrganizer; }
    int participationRole() const { return m_participationRole; }
    int participationStatus() const { return m_participationStatus; }

private:
    QString m_name;
    QString m_email;
    bool m_isOrganizer;
    int m_participationRole;
    int m_participationStatus;
};

class CalendarEventQuery : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY instanceIdChanged)
    Q_PROPERTY(QDateTime startTime READ startTime WRITE setStartTime RESET resetStartTime NOTIFY startTimeChanged)
    Q_PROPERTY(QObject *event READ event NOTIFY eventChanged)
    Q_PROPERTY(QObject *occurrence READ occurrence NOTIFY occurrenceChanged)
    Q_PROPERTY(QList<QObject*> attendees READ attendees NOTIFY attendeesChanged)
    Q_PROPERTY(bool eventError READ eventError NOTIFY eventErrorChanged)

public:
    CalendarEventQuery();
    ~CalendarEventQuery();

    QString instanceId() const;
    void setInstanceId(const QString &);

    QDateTime startTime() const;
    void setStartTime(const QDateTime &);
    void resetStartTime();

    QObject *event() const;
    QObject *occurrence() const;

    QList<QObject*> attendees();

    bool eventError() const;

    virtual void classBegin();
    virtual void componentComplete();

    void doRefresh(CalendarData::Event event, bool eventError);

signals:
    void instanceIdChanged();
    void eventChanged();
    void occurrenceChanged();
    void attendeesChanged();
    void startTimeChanged();
    void eventErrorChanged();

private slots:
    void refresh();
    void onTimezoneChanged();
    void instanceIdNotified(QString oldId, QString newId, QString notebookUid);

private:
    bool m_isComplete;
    QString m_instanceId;
    QDateTime m_startTime;
    CalendarData::Event m_event;
    CalendarEventOccurrence *m_occurrence;
    bool m_attendeesCached;
    bool m_eventError;
    bool m_updateOccurrence;
    QList<CalendarData::Attendee> m_attendees;
};

#endif
