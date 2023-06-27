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

#ifndef CALENDARWORKER_H
#define CALENDARWORKER_H

#include "calendardata.h"

#include <QObject>
#include <QHash>

// mkcal
#include <multicalendarstorage.h>

// libaccounts-qt
namespace Accounts { class Manager; }

// To get notified about timezone changes
#include <timed-qt5/wall-declarations.h>

class CalendarInvitationQuery;

class CalendarWorker : public QObject, public mKCal::MultiCalendarStorage::Observer
{
    Q_OBJECT
    
public:
    CalendarWorker();
    ~CalendarWorker();

    /* mKCal::ExtendedStorageObserver */
    void storageModified(mKCal::MultiCalendarStorage *storage) override;
    void storageUpdated(mKCal::MultiCalendarStorage *storage,
                        const QString &notebookUid,
                        const KCalendarCore::Incidence::List &added,
                        const KCalendarCore::Incidence::List &modified,
                        const KCalendarCore::Incidence::List &deleted) override;

public slots:
    void init();
    void save();

    void saveEvent(const CalendarData::Event &eventData, bool updateAttendees,
                   const QList<CalendarData::EmailContact> &required,
                   const QList<CalendarData::EmailContact> &optional);
    CalendarData::Event dissociateSingleOccurrence(const QString &instanceId, const QDateTime &datetime);
    void deleteEvent(const QString &instanceId, const QDateTime &dateTime);
    void deleteAll(const QString &instanceId);
    bool sendResponse(const QString &instanceId, const CalendarEvent::Response response);
    QString convertEventToICalendar(const QString &instanceId, const QString &prodId) const;

    QList<CalendarData::Notebook> notebooks() const;
    void setNotebookColor(const QString &notebookUid, const QString &color);
    void setExcludedNotebooks(const QStringList &list);
    void excludeNotebook(const QString &notebookUid, bool exclude);
    void setDefaultNotebook(const QString &notebookUid);

    void loadData(const QList<CalendarData::Range> &ranges,
                  const QStringList &instanceList, bool reset);

    void search(const QString &searchString, int limit);

    CalendarData::EventOccurrence getNextOccurrence(const QString &instanceId,
                                                    const QDateTime &startTime) const;
    QList<CalendarData::Attendee> getEventAttendees(const QString &instanceId);

    void findMatchingEvent(const QString &invitationFile);
    void onTimedSignal(const Maemo::Timed::WallClock::Info &info, bool time_changed);

signals:
    void storageModifiedSignal();
    void calendarTimezoneChanged();

    void eventNotebookChanged(const QString &oldInstanceId, const QString &newInstanceId, const QString &notebookUid);

    void excludedNotebooksChanged(const QStringList &excludedNotebooks);
    void notebookColorChanged(const CalendarData::Notebook &notebook);
    void notebooksChanged(const QList<CalendarData::Notebook> &notebooks);

    void dataLoaded(const QList<CalendarData::Range> &ranges,
                    const QStringList &instanceList,
                    const QHash<QString, CalendarData::Event> &events,
                    const QHash<QString, CalendarData::EventOccurrence> &occurrences,
                    const QHash<QDate, QStringList> &dailyOccurrences,
                    bool reset);

    void searchResults(const QString &searchString, const QStringList &identifiers);

    void findMatchingEventFinished(const QString &invitationFile,
                                   const CalendarData::Event &eventData);

private:
    void loadNotebooks();
    QStringList excludedNotebooks() const;
    bool saveExcludeNotebook(const QString &notebookUid, bool exclude);

    bool isOrganizer(const QString &notebookUid,
                     const KCalendarCore::Incidence::Ptr &event) const;
    void updateEventAttendees(KCalendarCore::Event::Ptr event, bool newEvent,
                              const QList<CalendarData::EmailContact> &required,
                              const QList<CalendarData::EmailContact> &optional,
                              const QString &notebookUid);
    QString getNotebookAddress(const QString &notebookUid) const;
    KCalendarCore::Incidence::Ptr getInstance(const QString &instanceId) const;

    CalendarData::Event createEventStruct(const KCalendarCore::Event::Ptr &event,
                                          const mKCal::Notebook &notebook) const;
    void eventOccurrences(QHash<QString, CalendarData::EventOccurrence> *occurrences,
                          const QList<CalendarData::Range> &ranges,
                          const KCalendarCore::Calendar &calendar) const;
    QHash<QDate, QStringList> dailyEventOccurrences(const QList<CalendarData::Range> &ranges,
                                                    const QList<CalendarData::EventOccurrence> &occurrences) const;

    Accounts::Manager *mAccountManager;

    mKCal::MultiCalendarStorage::Ptr mStorage;

    QHash<QString, CalendarData::Notebook> mNotebooks;

    // Tracks which events have been already passed to manager, using instanceIdentifiers.
    QSet<QString> mSentEvents;
};

#endif // CALENDARWORKER_H
