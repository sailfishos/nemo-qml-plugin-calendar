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

#include "calendarmanager.h"

#include <QDebug>

#include "calendarworker.h"
#include "calendarevent.h"
#include "calendaragendamodel.h"
#include "calendareventoccurrence.h"
#include "calendareventquery.h"
#include "calendarinvitationquery.h"
#include "calendarchangeinformation.h"

// kCalCore
#include <calformat.h>

CalendarManager::CalendarManager()
    : mLoadPending(false), mResetPending(false)
{
    qRegisterMetaType<KDateTime>("KDateTime");
    qRegisterMetaType<QList<KDateTime> >("QList<KDateTime>");
    qRegisterMetaType<CalendarEvent::Recur>("CalendarEvent::Recur");
    qRegisterMetaType<QHash<QString,CalendarData::EventOccurrence> >("QHash<QString,CalendarData::EventOccurrence>");
    qRegisterMetaType<CalendarData::Event>("CalendarData::Event");
    qRegisterMetaType<QMultiHash<QString,CalendarData::Event> >("QMultiHash<QString,CalendarData::Event>");
    qRegisterMetaType<QHash<QDate,QStringList> >("QHash<QDate,QStringList>");
    qRegisterMetaType<CalendarData::Range>("CalendarData::Range");
    qRegisterMetaType<QList<CalendarData::Range > >("QList<CalendarData::Range>");
    qRegisterMetaType<QList<CalendarData::Notebook> >("QList<CalendarData::Notebook>");
    qRegisterMetaType<QList<CalendarData::EmailContact> >("QList<CalendarData::EmailContact>");

    mCalendarWorker = new CalendarWorker();
    mCalendarWorker->moveToThread(&mWorkerThread);

    connect(&mWorkerThread, &QThread::finished, mCalendarWorker, &QObject::deleteLater);

    connect(mCalendarWorker, &CalendarWorker::storageModifiedSignal,
            this, &CalendarManager::storageModifiedSlot);

    connect(mCalendarWorker, &CalendarWorker::eventNotebookChanged,
            this, &CalendarManager::eventNotebookChanged);

    connect(mCalendarWorker, &CalendarWorker::excludedNotebooksChanged,
            this, &CalendarManager::excludedNotebooksChangedSlot);
    connect(mCalendarWorker, &CalendarWorker::notebooksChanged,
            this, &CalendarManager::notebooksChangedSlot);

    connect(mCalendarWorker, &CalendarWorker::dataLoaded,
            this, &CalendarManager::dataLoadedSlot);

    connect(mCalendarWorker, &CalendarWorker::occurrenceExceptionFailed,
            this, &CalendarManager::occurrenceExceptionFailedSlot);
    connect(mCalendarWorker, &CalendarWorker::occurrenceExceptionCreated,
            this, &CalendarManager::occurrenceExceptionCreatedSlot);

    connect(mCalendarWorker, &CalendarWorker::findMatchingEventFinished,
            this, &CalendarManager::findMatchingEventFinished);

    mWorkerThread.setObjectName("calendarworker");
    mWorkerThread.start();

    QMetaObject::invokeMethod(mCalendarWorker, "init", Qt::QueuedConnection);

    mTimer = new QTimer(this);
    mTimer->setSingleShot(true);
    mTimer->setInterval(5);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(timeout()));
}

CalendarManager *CalendarManager::instance(bool createIfNeeded)
{
    static CalendarManager *managerInstance;
    if (!managerInstance && createIfNeeded)
        managerInstance = new CalendarManager;

    return managerInstance;
}

CalendarManager::~CalendarManager()
{
    mWorkerThread.quit();
    mWorkerThread.wait();
}

QList<CalendarData::Notebook> CalendarManager::notebooks()
{
    return mNotebooks.values();
}

QString CalendarManager::defaultNotebook() const
{
    foreach (const CalendarData::Notebook &notebook, mNotebooks) {
        if (notebook.isDefault)
            return notebook.uid;
    }
    return QString();
}

void CalendarManager::setDefaultNotebook(const QString &notebookUid)
{
    QMetaObject::invokeMethod(mCalendarWorker, "setDefaultNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid));
}

CalendarEvent* CalendarManager::eventObject(const QString &eventUid, const KDateTime &recurrenceId)
{
    CalendarData::Event event = getEvent(eventUid, recurrenceId);
    if (event.isValid()) {
        QMultiHash<QString, CalendarEvent *>::iterator it = mEventObjects.find(eventUid);
        while (it != mEventObjects.end() && it.key() == eventUid) {
            if ((*it)->recurrenceId() == recurrenceId) {
                return *it;
            }
            ++it;
        }

        CalendarEvent *calendarEvent = new CalendarEvent(this, eventUid, recurrenceId);
        mEventObjects.insert(eventUid, calendarEvent);
        return calendarEvent;
    }

    // TODO: maybe attempt to read event from DB? This situation should not happen.
    qWarning() << Q_FUNC_INFO << "No event with uid" << eventUid << recurrenceId.toString() << ", returning empty event";

    return new CalendarEvent(this, QString(), KDateTime());
}

void CalendarManager::saveModification(CalendarData::Event eventData, bool updateAttendees,
                                       const QList<CalendarData::EmailContact> &required,
                                       const QList<CalendarData::EmailContact> &optional)
{
    QMetaObject::invokeMethod(mCalendarWorker, "saveEvent", Qt::QueuedConnection,
                              Q_ARG(CalendarData::Event, eventData),
                              Q_ARG(bool, updateAttendees),
                              Q_ARG(QList<CalendarData::EmailContact>, required),
                              Q_ARG(QList<CalendarData::EmailContact>, optional));
}

// caller owns returned object
CalendarChangeInformation *
CalendarManager::replaceOccurrence(CalendarData::Event eventData, CalendarEventOccurrence *occurrence,
                                   bool updateAttendees,
                                   const QList<CalendarData::EmailContact> &required,
                                   const QList<CalendarData::EmailContact> &optional)
{
    if (!occurrence) {
        qWarning() << Q_FUNC_INFO << "no occurrence given";
        return nullptr;
    }

    if (eventData.uniqueId.isEmpty()) {
        qWarning("NemocalendarManager::replaceOccurrence() - empty uid given");
        return nullptr;
    }

    // save request information for signal handling
    CalendarChangeInformation *changes = new CalendarChangeInformation;
    OccurrenceData changeData = { eventData, occurrence->startTime(), changes };
    mPendingOccurrenceExceptions.append(changeData);

    QMetaObject::invokeMethod(mCalendarWorker, "replaceOccurrence", Qt::QueuedConnection,
                              Q_ARG(CalendarData::Event, eventData),
                              Q_ARG(QDateTime, occurrence->startTime()),
                              Q_ARG(bool, updateAttendees),
                              Q_ARG(QList<CalendarData::EmailContact>, required),
                              Q_ARG(QList<CalendarData::EmailContact>, optional));
    return changes;
}

QStringList CalendarManager::excludedNotebooks()
{
    return mExcludedNotebooks;
}

void CalendarManager::setExcludedNotebooks(const QStringList &list)
{
    QStringList sorted;
    sorted.append(list);
    sorted.sort();
    if (mExcludedNotebooks == sorted)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setExcludedNotebooks", Qt::QueuedConnection,
                              Q_ARG(QStringList, sorted));
}

void CalendarManager::excludeNotebook(const QString &notebookUid, bool exclude)
{
    QMetaObject::invokeMethod(mCalendarWorker, "excludeNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid),
                              Q_ARG(bool, exclude));
}

void CalendarManager::setNotebookColor(const QString &notebookUid, const QString &color)
{
    QMetaObject::invokeMethod(mCalendarWorker, "setNotebookColor", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid),
                              Q_ARG(QString, color));
}

QString CalendarManager::getNotebookColor(const QString &notebookUid) const
{
    if (mNotebooks.contains(notebookUid))
        return mNotebooks.value(notebookUid, CalendarData::Notebook()).color;
    else
        return QString();
}

void CalendarManager::cancelAgendaRefresh(CalendarAgendaModel *model)
{
    mAgendaRefreshList.removeOne(model);
}

void CalendarManager::scheduleAgendaRefresh(CalendarAgendaModel *model)
{
    if (mAgendaRefreshList.contains(model))
        return;

    mAgendaRefreshList.append(model);

    if (!mLoadPending)
        mTimer->start();
}

void CalendarManager::registerEventQuery(CalendarEventQuery *query)
{
    mQueryList.append(query);
}

void CalendarManager::unRegisterEventQuery(CalendarEventQuery *query)
{
    mQueryList.removeOne(query);
}

void CalendarManager::scheduleEventQueryRefresh(CalendarEventQuery *query)
{
    if (mQueryRefreshList.contains(query))
        return;

    mQueryRefreshList.append(query);

    if (!mLoadPending)
        mTimer->start();
}

static QDate agenda_endDate(const CalendarAgendaModel *model)
{
    QDate endDate = model->endDate();
    return endDate.isValid() ? endDate: model->startDate();
}

bool CalendarManager::isRangeLoaded(const CalendarData::Range &r, QList<CalendarData::Range> *missingRanges)
{
    missingRanges->clear();
    // Range not loaded, no stored data
    if (mLoadedRanges.isEmpty()) {
        missingRanges->append(CalendarData::Range());
        missingRanges->last().first = r.first;
        missingRanges->last().second = r.second;
        return false;
    }

    QDate start(r.first);
    foreach (const CalendarData::Range range, mLoadedRanges) {
        // Range already loaded
        if (start >= range.first && r.second <= range.second)
            return missingRanges->isEmpty();

        // Legend:
        // * |------|: time scale
        // * x: existing loaded period
        // * n: new period
        // * l: to be loaded
        //
        // Period partially loaded: end available
        //    nnnnnn
        // |------xxxxxxx------------|
        //    llll
        //
        // or beginning and end available, middle missing
        //             nnnnnn
        // |------xxxxxxx---xxxxx----|
        //               lll
        if (start < range.first && r.second <= range.second) {
            missingRanges->append(CalendarData::Range());
            missingRanges->last().first = start;
            if (r.second < range.first)
                missingRanges->last().second = r.second;
            else
                missingRanges->last().second = range.first.addDays(-1);

            return false;
        }

        // Period partially loaded: beginning available, end missing
        //           nnnnnn
        // |------xxxxxxx------------|
        //               lll
        if ((start >= range.first && start <= range.second) && r.second > range.second)
            start = range.second.addDays(1);

        // Period partially loaded, period will be splitted into two or more subperiods
        //              nnnnnnnnnnn
        // |------xxxxxxx---xxxxx----|
        //               lll     ll
        if (start < range.first && range.second < r.second) {
            missingRanges->append(CalendarData::Range());
            missingRanges->last().first = start;
            missingRanges->last().second = range.first.addDays(-1);
            start = range.second.addDays(1);
        }
    }

    missingRanges->append(CalendarData::Range());
    missingRanges->last().first = start;
    missingRanges->last().second = r.second;

    return false;
}

static bool range_lessThan(CalendarData::Range lhs, CalendarData::Range rhs)
{
    return lhs.first < rhs.first;
}

QList<CalendarData::Range> CalendarManager::addRanges(const QList<CalendarData::Range> &oldRanges,
                                                      const QList<CalendarData::Range> &newRanges)
{
    if (newRanges.isEmpty() && oldRanges.isEmpty())
        return oldRanges;

    // sort
    QList<CalendarData::Range> sortedRanges;
    sortedRanges.append(oldRanges);
    sortedRanges.append(newRanges);
    qSort(sortedRanges.begin(), sortedRanges.end(), range_lessThan);

    // combine
    QList<CalendarData::Range> combinedRanges;
    combinedRanges.append(sortedRanges.first());

    for (int i = 1; i < sortedRanges.count(); ++i) {
        CalendarData::Range r = sortedRanges.at(i);
        if (combinedRanges.last().second.addDays(1) >= r.first)
            combinedRanges.last().second = qMax(combinedRanges.last().second, r.second);
        else
            combinedRanges.append(r);
    }

    return combinedRanges;
}

void CalendarManager::updateAgendaModel(CalendarAgendaModel *model)
{
    QList<CalendarEventOccurrence*> filtered;
    if (model->startDate() == model->endDate() || !model->endDate().isValid()) {
        foreach (const QString &id, mEventOccurrenceForDates.value(model->startDate())) {
            if (mEventOccurrences.contains(id)) {
                CalendarData::EventOccurrence eo = mEventOccurrences.value(id);
                filtered.append(new CalendarEventOccurrence(eo.eventUid, eo.recurrenceId,
                                                            eo.startTime, eo.endTime));
            } else {
                qWarning() << "no occurrence with id" << id;
            }
        }
    } else {
        foreach (const CalendarData::EventOccurrence &eo, mEventOccurrences.values()) {
            CalendarEvent *event = eventObject(eo.eventUid, eo.recurrenceId);
            if (!event) {
                qWarning() << "no event for occurrence";
                continue;
            }
            QDate start = model->startDate();
            QDate end = model->endDate();

            // on all day events the end time is inclusive, otherwise not
            if ((eo.startTime.date() < start
                 && (eo.endTime.date() > start
                     || (eo.endTime.date() == start && (event->allDay()
                                                        || eo.endTime.time() > QTime(0, 0)))))
                    || (eo.startTime.date() >= start && eo.startTime.date() <= end)) {
                filtered.append(new CalendarEventOccurrence(eo.eventUid, eo.recurrenceId,
                                                            eo.startTime, eo.endTime));
            }
        }
    }

    model->doRefresh(filtered);
}

void CalendarManager::doAgendaAndQueryRefresh()
{
    QList<CalendarAgendaModel *> agendaModels = mAgendaRefreshList;
    mAgendaRefreshList.clear();
    QList<CalendarData::Range> missingRanges;
    foreach (CalendarAgendaModel *model, agendaModels) {
        CalendarData::Range range;
        range.first = model->startDate();
        range.second = agenda_endDate(model);

        if (!range.first.isValid()) {
            // need start date for fetching events, clear this model
            model->doRefresh(QList<CalendarEventOccurrence*>());
            continue;
        }

        QList<CalendarData::Range> newRanges;
        if (isRangeLoaded(range, &newRanges))
            updateAgendaModel(model);
        else
            missingRanges = addRanges(missingRanges, newRanges);
    }

    if (mResetPending) {
        missingRanges = addRanges(missingRanges, mLoadedRanges);
        mLoadedRanges.clear();
        mLoadedQueries.clear();
    }

    QList<CalendarEventQuery *> queryList = mQueryRefreshList;
    mQueryRefreshList.clear();
    QStringList missingUidList;
    foreach (CalendarEventQuery *query, queryList) {
        QString eventUid = query->uniqueId();
        if (eventUid.isEmpty())
            continue;

        KDateTime recurrenceId = query->recurrenceId();
        CalendarData::Event event = getEvent(eventUid, recurrenceId);
        if (event.uniqueId.isEmpty()
                && !mLoadedQueries.contains(eventUid)
                && !missingUidList.contains(eventUid)) {
            missingUidList << eventUid;
        }
        query->doRefresh(event);

        if (mResetPending && !missingUidList.contains(eventUid))
            missingUidList << eventUid;
    }

    if (!missingRanges.isEmpty() || !missingUidList.isEmpty()) {
        mLoadPending = true;
        QMetaObject::invokeMethod(mCalendarWorker, "loadData", Qt::QueuedConnection,
                                  Q_ARG(QList<CalendarData::Range>, missingRanges),
                                  Q_ARG(QStringList, missingUidList),
                                  Q_ARG(bool, mResetPending));
        mResetPending = false;
    }
}

void CalendarManager::timeout()
{
    if (mLoadPending)
        return;

    if (!mAgendaRefreshList.isEmpty() || !mQueryRefreshList.isEmpty() || mResetPending)
        doAgendaAndQueryRefresh();
}

void CalendarManager::occurrenceExceptionFailedSlot(const CalendarData::Event &data, const QDateTime &occurrence)
{
    for (int i = 0; i < mPendingOccurrenceExceptions.length(); ++i) {
        const OccurrenceData &item = mPendingOccurrenceExceptions.at(i);
        if (item.event == data && item.occurrenceTime == occurrence) {
            if (item.changeObject) {
                item.changeObject->setInformation(QString(), KDateTime());
            }
            mPendingOccurrenceExceptions.removeAt(i);
            break;
        }
    }
}

void CalendarManager::occurrenceExceptionCreatedSlot(const CalendarData::Event &data, const QDateTime &occurrence,
                                                     const KDateTime &newRecurrenceId)
{
    for (int i = 0; i < mPendingOccurrenceExceptions.length(); ++i) {
        const OccurrenceData &item = mPendingOccurrenceExceptions.at(i);
        if (item.event == data && item.occurrenceTime == occurrence) {
            if (item.changeObject) {
                item.changeObject->setInformation(data.uniqueId, newRecurrenceId);
            }

            mPendingOccurrenceExceptions.removeAt(i);
            break;
        }
    }

}

void CalendarManager::deleteEvent(const QString &uid, const KDateTime &recurrenceId, const QDateTime &time)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteEvent", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, recurrenceId),
                              Q_ARG(QDateTime, time));
}

void CalendarManager::deleteAll(const QString &uid)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteAll", Qt::QueuedConnection,
                              Q_ARG(QString, uid));
}

void CalendarManager::save()
{
    QMetaObject::invokeMethod(mCalendarWorker, "save", Qt::QueuedConnection);
}

QString CalendarManager::convertEventToICalendarSync(const QString &uid, const QString &prodId)
{
    QString vEvent;
    QMetaObject::invokeMethod(mCalendarWorker, "convertEventToICalendar", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, vEvent),
                              Q_ARG(QString, uid),
                              Q_ARG(QString, prodId));
    return vEvent;
}

CalendarData::Event CalendarManager::getEvent(const QString &uid, const KDateTime &recurrenceId)
{
    QMultiHash<QString, CalendarData::Event>::iterator it = mEvents.find(uid);
    while (it != mEvents.end() && it.key() == uid) {
        if (it.value().recurrenceId == recurrenceId) {
            return it.value();
        }
        ++it;
    }

    return CalendarData::Event();
}

bool CalendarManager::sendResponse(const CalendarData::Event &eventData, CalendarEvent::Response response)
{
    bool result;
    QMetaObject::invokeMethod(mCalendarWorker, "sendResponse", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(CalendarData::Event, eventData),
                              Q_ARG(CalendarEvent::Response, response));
    return result;
}

void CalendarManager::scheduleInvitationQuery(CalendarInvitationQuery *query, const QString &invitationFile)
{
    mInvitationQueryHash.insert(query, invitationFile);
    QMetaObject::invokeMethod(mCalendarWorker, "findMatchingEvent", Qt::QueuedConnection,
                              Q_ARG(QString, invitationFile));
}

void CalendarManager::unRegisterInvitationQuery(CalendarInvitationQuery *query)
{
    mInvitationQueryHash.remove(query);
}

void CalendarManager::findMatchingEventFinished(const QString &invitationFile, const CalendarData::Event &event)
{
    QHash<CalendarInvitationQuery*, QString>::iterator it = mInvitationQueryHash.begin();
    while (it != mInvitationQueryHash.end()) {
        if (it.value() == invitationFile) {
            it.key()->queryResult(event);
            it = mInvitationQueryHash.erase(it);
        } else {
            it++;
        }
    }
}

void CalendarManager::storageModifiedSlot(const QString &info)
{
    Q_UNUSED(info)
    mResetPending = true;
    emit storageModified();
}

void CalendarManager::eventNotebookChanged(const QString &oldEventUid, const QString &newEventUid,
                                           const QString &notebookUid)
{
    // FIXME: adapt to multihash + recurrenceId.
#if 0
    if (mEvents.contains(oldEventUid)) {
        mEvents.insert(newEventUid, mEvents.value(oldEventUid));
        mEvents[newEventUid].calendarUid = notebookUid;
        mEvents.remove(oldEventUid);
    }
    if (mEventObjects.contains(oldEventUid)) {
        mEventObjects.insert(newEventUid, mEventObjects.value(oldEventUid));
        mEventObjects.remove(oldEventUid);
    }
    foreach (QString occurrenceUid, mEventOccurrences.keys()) {
        if (mEventOccurrences.value(occurrenceUid).eventUid == oldEventUid)
            mEventOccurrences[occurrenceUid].eventUid = newEventUid;
    }

    emit eventUidChanged(oldEventUid, newEventUid);

    // Event uid is changed when events are moved between notebooks, the notebook color
    // associated with this event has changed. Emit color changed after emitting eventUidChanged,
    // so that data models have the correct event uid to use when querying for CalendarEvent
    // instances, see CalendarEventOccurrence::eventObject(), used by CalendarAgendaModel.
    CalendarEvent *eventObject = mEventObjects.value(newEventUid);
    if (eventObject)
        emit eventObject->colorChanged();
#else
    Q_UNUSED(oldEventUid)
    Q_UNUSED(newEventUid)
    Q_UNUSED(notebookUid)
#endif
}

void CalendarManager::excludedNotebooksChangedSlot(const QStringList &excludedNotebooks)
{
    QStringList sortedExcluded = excludedNotebooks;
    sortedExcluded.sort();
    if (mExcludedNotebooks != sortedExcluded) {
        mExcludedNotebooks = sortedExcluded;
        emit excludedNotebooksChanged(mExcludedNotebooks);
        mResetPending = true;
        mTimer->start();
    }
}

void CalendarManager::notebooksChangedSlot(const QList<CalendarData::Notebook> &notebooks)
{
    QHash<QString, CalendarData::Notebook> newNotebooks;
    QStringList colorChangers;
    QString newDefaultNotebookUid;
    bool changed = false;
    foreach (const CalendarData::Notebook &notebook, notebooks) {
        if (mNotebooks.contains(notebook.uid)) {
            if (mNotebooks.value(notebook.uid) != notebook) {
                changed = true;
                if (mNotebooks.value(notebook.uid).color != notebook.color)
                    colorChangers << notebook.uid;
            }
        }
        if (notebook.isDefault) {
            if (!mNotebooks.contains(notebook.uid) || !mNotebooks.value(notebook.uid).isDefault)
                newDefaultNotebookUid = notebook.uid;
        }

        newNotebooks.insert(notebook.uid, notebook);
    }

    if (changed || mNotebooks.count() != newNotebooks.count()) {
        emit notebooksAboutToChange();
        mNotebooks = newNotebooks;
        emit notebooksChanged(mNotebooks.values());
        foreach (const QString &uid, colorChangers)
            emit notebookColorChanged(uid);

        if (!newDefaultNotebookUid.isEmpty())
            emit defaultNotebookChanged(newDefaultNotebookUid);
    }
}

CalendarEventOccurrence* CalendarManager::getNextOccurrence(const QString &uid, const KDateTime &recurrenceId,
                                                            const QDateTime &start)
{
    CalendarData::EventOccurrence eo;
    QMetaObject::invokeMethod(mCalendarWorker, "getNextOccurrence", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(CalendarData::EventOccurrence, eo),
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, recurrenceId),
                              Q_ARG(QDateTime, start));

    if (!eo.startTime.isValid()) {
        qWarning() << Q_FUNC_INFO << "Unable to find occurrence for event" << uid << recurrenceId.toString();
        return new CalendarEventOccurrence(QString(), KDateTime(), QDateTime(), QDateTime());
    }

    return new CalendarEventOccurrence(eo.eventUid, eo.recurrenceId, eo.startTime, eo.endTime);
}

QList<CalendarData::Attendee> CalendarManager::getEventAttendees(const QString &uid, const KDateTime &recurrenceId)
{
    QList<CalendarData::Attendee> attendees;
    QMetaObject::invokeMethod(mCalendarWorker, "getEventAttendees", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<CalendarData::Attendee>, attendees),
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, recurrenceId));
    return attendees;
}

void CalendarManager::dataLoadedSlot(const QList<CalendarData::Range> &ranges,
                                     const QStringList &uidList,
                                     const QMultiHash<QString, CalendarData::Event> &events,
                                     const QHash<QString, CalendarData::EventOccurrence> &occurrences,
                                     const QHash<QDate, QStringList> &dailyOccurrences,
                                     bool reset)
{
    QList<CalendarData::Event> oldEvents;
    foreach (const QString &uid, mEventObjects.keys()) {
        // just add all matching uid, change signal emission will match recurrence ids
        if (events.contains(uid))
            oldEvents.append(mEvents.values(uid));
    }

    if (reset) {
        mEvents.clear();
        mEventOccurrences.clear();
        mEventOccurrenceForDates.clear();
    }

    mLoadedRanges = addRanges(mLoadedRanges, ranges);
    mLoadedQueries.append(uidList);
    mEvents = mEvents.unite(events);
    mEventOccurrences = mEventOccurrences.unite(occurrences);
    mEventOccurrenceForDates = mEventOccurrenceForDates.unite(dailyOccurrences);
    mLoadPending = false;

    foreach (const CalendarData::Event &oldEvent, oldEvents) {
        CalendarData::Event event = getEvent(oldEvent.uniqueId, oldEvent.recurrenceId);
        if (event.isValid())
            sendEventChangeSignals(event, oldEvent);
    }

    emit dataUpdated();
    mTimer->start();
}

void CalendarManager::sendEventChangeSignals(const CalendarData::Event &newEvent,
                                             const CalendarData::Event &oldEvent)
{
    CalendarEvent *eventObject = 0;
    QMultiHash<QString, CalendarEvent *>::iterator it = mEventObjects.find(newEvent.uniqueId);
    while (it != mEventObjects.end() && it.key() == newEvent.uniqueId) {
        if (it.value()->recurrenceId() == newEvent.recurrenceId) {
            eventObject = it.value();
            break;
        }
        ++it;
    }

    if (!eventObject)
        return;

    if (newEvent.allDay != oldEvent.allDay)
        emit eventObject->allDayChanged();

    if (newEvent.displayLabel != oldEvent.displayLabel)
        emit eventObject->displayLabelChanged();

    if (newEvent.description != oldEvent.description)
        emit eventObject->descriptionChanged();

    if (newEvent.endTime != oldEvent.endTime)
        emit eventObject->endTimeChanged();

    if (newEvent.location != oldEvent.location)
        emit eventObject->locationChanged();

    if (newEvent.secrecy != oldEvent.secrecy)
        emit eventObject->secrecyChanged();

    if (newEvent.recur != oldEvent.recur)
        emit eventObject->recurChanged();

    if (newEvent.reminder != oldEvent.reminder)
        emit eventObject->reminderChanged();

    if (newEvent.startTime != oldEvent.startTime)
        emit eventObject->startTimeChanged();

    if (newEvent.rsvp != oldEvent.rsvp)
        emit eventObject->rsvpChanged();

    if (newEvent.externalInvitation != oldEvent.externalInvitation)
        emit eventObject->externalInvitationChanged();

    if (newEvent.ownerStatus != oldEvent.ownerStatus)
        emit eventObject->ownerStatusChanged();
}
