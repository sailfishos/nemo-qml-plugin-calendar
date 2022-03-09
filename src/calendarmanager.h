/*
 * Copyright (C) 2014 Jolla Ltd.
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

#ifndef CALENDARMANAGER_H
#define CALENDARMANAGER_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <QDateTime>

#include "calendardata.h"
#include "calendarevent.h"

class CalendarWorker;
class CalendarAgendaModel;
class CalendarEventListModel;
class CalendarEventOccurrence;
class CalendarEventQuery;
class CalendarInvitationQuery;

class CalendarManager : public QObject
{
    Q_OBJECT
private:
    CalendarManager();

public:
    static CalendarManager *instance(bool createIfNeeded = true);
    ~CalendarManager();

    CalendarStoredEvent* eventObject(const QString &eventUid, const QDateTime &recurrenceId);

    void saveModification(CalendarData::Event eventData, bool updateAttendees,
                          const QList<CalendarData::EmailContact> &required,
                          const QList<CalendarData::EmailContact> &optional);
    void deleteEvent(const QString &uid, const QDateTime &recurrenceId, const QDateTime &dateTime);
    void deleteAll(const QString &uid);
    void save();

    // Synchronous DB thread access
    QString convertEventToICalendarSync(const QString &uid, const QString &prodId);

    // Event
    CalendarData::Event getEvent(const QString& instanceIdentifier, bool *loaded = nullptr) const;
    CalendarData::Event getEvent(const QString& uid, const QDateTime &recurrenceId);
    CalendarData::Event dissociateSingleOccurrence(const QString &eventUid, const QDateTime &recurrenceId) const;
    bool sendResponse(const CalendarData::Event &eventData, CalendarEvent::Response response);

    // Notebooks
    QList<CalendarData::Notebook> notebooks();
    QString defaultNotebook() const;
    void setDefaultNotebook(const QString &notebookUid);
    QStringList excludedNotebooks();
    void setExcludedNotebooks(const QStringList &list);
    void excludeNotebook(const QString &notebookUid, bool exclude);
    void setNotebookColor(const QString &notebookUid, const QString &color);
    QString getNotebookColor(const QString &notebookUid) const;

    // AgendaModel
    void cancelAgendaRefresh(CalendarAgendaModel *model);
    void scheduleAgendaRefresh(CalendarAgendaModel *model);

    // EventListModel
    void cancelEventListRefresh(CalendarEventListModel *model);
    void scheduleEventListRefresh(CalendarEventListModel *model);

    // EventQuery
    void scheduleEventQueryRefresh(CalendarEventQuery *query);
    void cancelEventQueryRefresh(CalendarEventQuery *query);

    // Invitation event search
    void scheduleInvitationQuery(CalendarInvitationQuery *query, const QString &invitationFile);
    void unRegisterInvitationQuery(CalendarInvitationQuery *query);

    // Caller gets ownership of returned CalendarEventOccurrence object
    // Does synchronous DB thread access - no DB operations, though, fast when no ongoing DB ops
    CalendarEventOccurrence* getNextOccurrence(const QString &uid, const QDateTime &recurrenceId,
                                               const QDateTime &start);
    // return attendees for given event, synchronous call
    QList<CalendarData::Attendee> getEventAttendees(const QString &uid, const QDateTime &recurrenceId, bool *resultValid);

private slots:
    void storageModifiedSlot(const QString &info);
    void eventNotebookChanged(const QString &oldEventUid, const QString &newEventUid, const QString &notebookUid);
    void excludedNotebooksChangedSlot(const QStringList &excludedNotebooks);
    void notebooksChangedSlot(const QList<CalendarData::Notebook> &notebooks);
    void dataLoadedSlot(const QList<CalendarData::Range> &ranges,
                        const QStringList &instanceList,
                        const QMultiHash<QString, CalendarData::Event> &events,
                        const QHash<QString, CalendarData::EventOccurrence> &occurrences,
                        const QHash<QDate, QStringList> &dailyOccurrences,
                        bool reset);
    void timeout();
    void findMatchingEventFinished(const QString &invitationFile,
                                   const CalendarData::Event &event);

signals:
    void excludedNotebooksChanged(QStringList excludedNotebooks);
    void notebooksAboutToChange();
    void notebooksChanged(QList<CalendarData::Notebook> notebooks);
    void notebookColorChanged(QString notebookUid);
    void defaultNotebookChanged(QString notebookUid);
    void storageModified();
    void dataUpdated();
    void eventUidChanged(QString oldUid, QString newUid);

private:
    friend class tst_CalendarManager;

    void doAgendaAndQueryRefresh();
    bool isRangeLoaded(const QPair<QDate, QDate> &r, QList<CalendarData::Range> *newRanges);
    QList<CalendarData::Range> addRanges(const QList<CalendarData::Range> &oldRanges,
                                         const QList<CalendarData::Range> &newRanges);
    void updateAgendaModel(CalendarAgendaModel *model);
    void sendEventChangeSignals(const CalendarData::Event &newEvent);

    QThread mWorkerThread;
    CalendarWorker *mCalendarWorker;
    QMultiHash<QString, CalendarData::Event> mEvents;
    QMultiHash<QString, CalendarStoredEvent *> mEventObjects;
    QHash<QString, CalendarData::EventOccurrence> mEventOccurrences;
    QHash<QDate, QStringList> mEventOccurrenceForDates;
    QList<CalendarAgendaModel *> mAgendaRefreshList;
    QList<CalendarEventListModel *> mEventListRefreshList;
    QList<CalendarEventQuery *> mQueryRefreshList;
    QHash<CalendarInvitationQuery *, QString> mInvitationQueryHash; // value is the invitationFile.
    QStringList mExcludedNotebooks;
    QHash<QString, CalendarData::Notebook> mNotebooks;

    QTimer *mTimer;

    // If true indicates that CalendarWorker::loadRanges(...) has been called, and the response
    // has not been received in slot CalendarManager::rangesLoaded(...)
    bool mLoadPending;

    // If true the next call to doAgendaRefresh() will cause a complete reload of calendar data
    bool mResetPending;

    // A list of non-overlapping loaded ranges sorted by range start date
    QList<CalendarData::Range > mLoadedRanges;

    // A list of event instance identifiers that have been processed by CalendarWorker
    QStringList mLoadedQueries;
};

#endif // CALENDARMANAGER_H
