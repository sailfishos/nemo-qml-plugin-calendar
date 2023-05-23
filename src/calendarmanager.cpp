/*
 * Copyright (C) 2014 - 2019 Jolla Ltd.
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

#include "calendarmanager.h"

#include <QDebug>

#include "calendarworker.h"
#include "calendarevent.h"
#include "calendaragendamodel.h"
#include "calendareventlistmodel.h"
#include "calendarsearchmodel.h"
#include "calendareventoccurrence.h"
#include "calendareventquery.h"
#include "calendarinvitationquery.h"

// kcalendarcore
#include <KCalendarCore/CalFormat>

CalendarManager::CalendarManager()
    : mLoadPending(false), mResetPending(false)
{
    qRegisterMetaType<QList<QDateTime> >("QList<QDateTime>");
    qRegisterMetaType<CalendarEvent::Recur>("CalendarEvent::Recur");
    qRegisterMetaType<QHash<QString,CalendarData::EventOccurrence> >("QHash<QString,CalendarData::EventOccurrence>");
    qRegisterMetaType<CalendarData::Event>("CalendarData::Event");
    qRegisterMetaType<QHash<QString,CalendarData::Event> >("QHash<QString,CalendarData::Event>");
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

    connect(mCalendarWorker, &CalendarWorker::calendarTimezoneChanged,
            this, &CalendarManager::calendarTimezoneChangedSlot);

    connect(mCalendarWorker, &CalendarWorker::eventNotebookChanged,
            this, &CalendarManager::eventNotebookChanged);

    connect(mCalendarWorker, &CalendarWorker::excludedNotebooksChanged,
            this, &CalendarManager::excludedNotebooksChangedSlot);
    connect(mCalendarWorker, &CalendarWorker::notebooksChanged,
            this, &CalendarManager::notebooksChangedSlot);

    connect(mCalendarWorker, &CalendarWorker::dataLoaded,
            this, &CalendarManager::dataLoadedSlot);

    connect(mCalendarWorker, &CalendarWorker::searchResults,
            this, &CalendarManager::onSearchResults);

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

static CalendarManager *managerInstance = nullptr;

CalendarManager *CalendarManager::instance(bool createIfNeeded)
{
    if (!managerInstance && createIfNeeded)
        managerInstance = new CalendarManager;

    return managerInstance;
}

CalendarManager::~CalendarManager()
{
    mWorkerThread.quit();
    mWorkerThread.wait();
    if (managerInstance == this) {
        managerInstance = nullptr;
    }
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

CalendarStoredEvent* CalendarManager::eventObject(const QString &instanceId)
{
    QHash<QString, CalendarStoredEvent *>::ConstIterator it = mEventObjects.find(instanceId);
    if (it != mEventObjects.end()) {
        return *it;
    }

    const QHash<QString, CalendarData::Event>::ConstIterator event = mEvents.find(instanceId);
    if (event != mEvents.constEnd()) {
        CalendarStoredEvent *calendarEvent = new CalendarStoredEvent(this, &(*event));
        mEventObjects.insert(instanceId, calendarEvent);
        return calendarEvent;
    }

    // TODO: maybe attempt to read event from DB? This situation should not happen.
    qWarning() << Q_FUNC_INFO << "No event with uid" << instanceId << ", returning empty event";

    return new CalendarStoredEvent(this, nullptr);
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

CalendarData::Event CalendarManager::dissociateSingleOccurrence(const QString &instanceId, const QDateTime &datetime) const
{
    CalendarData::Event event;
    // Worker method is not calling any storage method that could block.
    // The only blocking possibility here would be to obtain the worker thread
    // availability.
    QMetaObject::invokeMethod(mCalendarWorker, "dissociateSingleOccurrence",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(CalendarData::Event, event),
                              Q_ARG(QString, instanceId),
                              Q_ARG(QDateTime, datetime));
    return event;
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

void CalendarManager::cancelSearch(CalendarSearchModel *model)
{
    mSearchList.removeOne(model);
}

void CalendarManager::search(CalendarSearchModel *model)
{
    if (mSearchList.contains(model))
        return;

    mSearchList.append(model);
    QList<CalendarSearchModel*>::ConstIterator it;
    for (it = mSearchList.constBegin(); it != mSearchList.constEnd(); it++) {
        if (model != *it && model->searchString() == (*it)->searchString())
            return;
    }
    QMetaObject::invokeMethod(mCalendarWorker, "search", Qt::QueuedConnection,
                              Q_ARG(QString, model->searchString()),
                              Q_ARG(int, model->limit()));
}

void CalendarManager::onSearchResults(const QString &searchString,
                                      const QStringList &identifiers)
{
    QList<CalendarSearchModel*>::Iterator it = mSearchList.begin();
    while (it != mSearchList.end()) {
        CalendarSearchModel *model = *it;
        if (model->searchString() == searchString) {
            it = mSearchList.erase(it);
            model->setIdentifiers(identifiers);
        } else {
            it++;
        }
    }
}

bool CalendarManager::isSearching(const CalendarSearchModel *model) const
{
    return mSearchList.contains(const_cast<CalendarSearchModel*>(model));
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

void CalendarManager::cancelEventListRefresh(CalendarEventListModel *model)
{
    mEventListRefreshList.removeOne(model);
}

void CalendarManager::scheduleEventListRefresh(CalendarEventListModel *model)
{
    if (mEventListRefreshList.contains(model))
        return;

    mEventListRefreshList.append(model);

    if (!mLoadPending)
        mTimer->start();
}

void CalendarManager::scheduleEventQueryRefresh(CalendarEventQuery *query)
{
    if (mQueryRefreshList.contains(query))
        return;

    mQueryRefreshList.append(query);

    if (!mLoadPending)
        mTimer->start();
}

void CalendarManager::cancelEventQueryRefresh(CalendarEventQuery *query)
{
    mQueryRefreshList.removeOne(query);
}

static QDate agenda_endDate(const CalendarAgendaModel *model)
{
    QDate endDate = model->endDate();
    return endDate.isValid() ? endDate : model->startDate();
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
    std::sort(sortedRanges.begin(), sortedRanges.end(), range_lessThan);

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
                filtered.append(new CalendarEventOccurrence(eo.uniqueId, eo.startTime, eo.endTime));
            } else {
                qWarning() << "no occurrence with id" << id;
            }
        }
    } else {
        foreach (const CalendarData::EventOccurrence &eo, mEventOccurrences.values()) {
            CalendarEvent *event = eventObject(eo.instanceId);
            if (!event) {
                qWarning() << "no event for occurrence";
                continue;
            }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
            const QDateTime startDt(model->startDate().startOfDay());
            const QDateTime endDt(model->endDate().endOfDay());
#else
            const QDateTime startDt(model->startDate());
            const QDateTime endDt(QDateTime(model->endDate()).addDays(1));
#endif
            // on all day events the end time is inclusive, otherwise not
            if ((eo.eventAllDay && eo.startTime.date() <= model->endDate()
                 && eo.endTime.date() >= model->startDate())
                || (!eo.eventAllDay && eo.startTime < endDt
                    && eo.endTime >= startDt)) {
                filtered.append(new CalendarEventOccurrence(eo.uniqueId, eo.startTime, eo.endTime));
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

    QStringList missingInstanceList;

    QList<CalendarEventQuery *> queryList = mQueryRefreshList;
    mQueryRefreshList.clear();
    foreach (CalendarEventQuery *query, queryList) {
        const QString instanceId = query->instanceId();
        if (instanceId.isEmpty())
            continue;

        bool loaded = mLoadedQueries.contains(instanceId);
        CalendarData::Event event = mEvents.value(instanceId);
        if (((!event.isValid() && !loaded) || mResetPending)
                && !missingInstanceList.contains(instanceId)) {
            missingInstanceList << instanceId;
        }
        query->doRefresh(event, !event.isValid() && loaded);
    }

    const QList<CalendarEventListModel *> eventListModels = mEventListRefreshList;
    mEventListRefreshList.clear();
    for (CalendarEventListModel *model : eventListModels) {
        for (const QString &id : model->identifiers()) {
            if (id.isEmpty())
                continue;

            bool loaded = mLoadedQueries.contains(id);
            CalendarData::Event event = mEvents.value(id);
            if (((!event.isValid() && !loaded) || mResetPending)
                && !missingInstanceList.contains(id)) {
                missingInstanceList << id;
            }
        }
    }

    if (!missingRanges.isEmpty() || !missingInstanceList.isEmpty()) {
        mLoadPending = true;
        QMetaObject::invokeMethod(mCalendarWorker, "loadData", Qt::QueuedConnection,
                                  Q_ARG(QList<CalendarData::Range>, missingRanges),
                                  Q_ARG(QStringList, missingInstanceList),
                                  Q_ARG(bool, mResetPending));
        mResetPending = false;
    }
}

void CalendarManager::timeout()
{
    if (mLoadPending)
        return;

    if (!mAgendaRefreshList.isEmpty()
        || !mQueryRefreshList.isEmpty()
        || !mEventListRefreshList.isEmpty() || mResetPending)
        doAgendaAndQueryRefresh();
}

void CalendarManager::deleteEvent(const QString &instanceId, const QDateTime &time)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteEvent", Qt::QueuedConnection,
                              Q_ARG(QString, instanceId),
                              Q_ARG(QDateTime, time));
}

void CalendarManager::deleteAll(const QString &instanceId)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteAll", Qt::QueuedConnection,
                              Q_ARG(QString, instanceId));
}

void CalendarManager::save()
{
    QMetaObject::invokeMethod(mCalendarWorker, "save", Qt::QueuedConnection);
}

QString CalendarManager::convertEventToICalendarSync(const QString &instanceId, const QString &prodId)
{
    QString vEvent;
    QMetaObject::invokeMethod(mCalendarWorker, "convertEventToICalendar", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, vEvent),
                              Q_ARG(QString, instanceId),
                              Q_ARG(QString, prodId));
    return vEvent;
}

CalendarData::Event CalendarManager::getEvent(const QString &instanceId, bool *loaded) const
{
    if (loaded) {
        *loaded = mLoadedQueries.contains(instanceId);
    }
    return mEvents.value(instanceId);
}

bool CalendarManager::sendResponse(const QString &instanceId, CalendarEvent::Response response)
{
    bool result;
    QMetaObject::invokeMethod(mCalendarWorker, "sendResponse", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(QString, instanceId),
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

void CalendarManager::storageModifiedSlot()
{
    mResetPending = true;
    emit storageModified();
}

void CalendarManager::calendarTimezoneChangedSlot()
{
    QHash<QString, CalendarStoredEvent *>::ConstIterator it;
    for (it = mEventObjects.constBegin(); it != mEventObjects.constEnd(); it++) {
        // Actually, the date times have not changed, but
        // their representation in local time (as used in QML)
        // have changed.
        if ((*it)->startTimeSpec() != Qt::LocalTime)
            (*it)->startTimeChanged();
        if ((*it)->endTimeSpec() != Qt::LocalTime)
            (*it)->endTimeChanged();
    }
    emit timezoneChanged();
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
        if (mEventOccurrences.value(occurrenceUid).instanceId == oldEventUid)
            mEventOccurrences[occurrenceUid].instanceId = newEventUid;
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

CalendarEventOccurrence* CalendarManager::getNextOccurrence(const QString &instanceId,
                                                            const QDateTime &start)
{
    CalendarData::EventOccurrence eo;
    const CalendarData::Event event = mEvents.value(instanceId);
    if (event.recur == CalendarEvent::RecurOnce) {
        const QTimeZone systemTimeZone = QTimeZone::systemTimeZone();
        eo.instanceId = event.instanceId;
        eo.startTime = event.startTime.toTimeZone(systemTimeZone);
        eo.endTime = event.endTime.toTimeZone(systemTimeZone);
    } else {
        QMetaObject::invokeMethod(mCalendarWorker, "getNextOccurrence", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(CalendarData::EventOccurrence, eo),
                                  Q_ARG(QString, instanceId),
                                  Q_ARG(QDateTime, start));
    }

    if (!eo.startTime.isValid()) {
        qWarning() << Q_FUNC_INFO << "Unable to find occurrence for event" << instanceId;
        return new CalendarEventOccurrence(QString(), QDateTime(), QDateTime());
    }

    return new CalendarEventOccurrence(eo.uniqueId, eo.startTime, eo.endTime);
}

QList<CalendarData::Attendee> CalendarManager::getEventAttendees(const QString &instanceId, bool *resultValid)
{
    QList<CalendarData::Attendee> attendees;

    // Not foolproof, since thread interleaving means we might
    // receive a storageModified() signal on the worker thread
    // while we're dispatching this call here.
    // But, this will at least ensure that if we _know_ that
    // the storage is not in loaded state, that we don't
    // attempt to read the invalid data.
    // The other alternative would be to cache all attendee
    // info in the event struct immediately within
    // CalendarWorker::createEventStruct(), however it was
    // decided that it would be better to avoid the memory usage.
    *resultValid = !(mLoadPending || mResetPending);
    if (*resultValid) {
        QMetaObject::invokeMethod(mCalendarWorker, "getEventAttendees", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(QList<CalendarData::Attendee>, attendees),
                                  Q_ARG(QString, instanceId));
    }

    return attendees;
}

void CalendarManager::dataLoadedSlot(const QList<CalendarData::Range> &ranges,
                                     const QStringList &instanceList,
                                     const QHash<QString, CalendarData::Event> &events,
                                     const QHash<QString, CalendarData::EventOccurrence> &occurrences,
                                     const QHash<QDate, QStringList> &dailyOccurrences,
                                     bool reset)
{
    if (reset) {
        mEvents.clear();
        mEventOccurrences.clear();
        mEventOccurrenceForDates.clear();
    }

    mLoadedRanges = addRanges(mLoadedRanges, ranges);
    mLoadedQueries.append(instanceList);
    mEvents = mEvents.unite(events);
    // Use mEventOccurrences.insert(occurrences) from Qt5.15,
    // .unite() is deprecated and broken, it is duplicating keys.
    for (const CalendarData::EventOccurrence &eo: occurrences)
        mEventOccurrences.insert(eo.getId(), eo);
    for (QHash<QDate, QStringList>::ConstIterator it = dailyOccurrences.constBegin();
         it != dailyOccurrences.constEnd(); ++it)
        mEventOccurrenceForDates.insert(it.key(), it.value());
    mLoadPending = false;

    for (QHash<QString, CalendarStoredEvent *>::ConstIterator it = mEventObjects.constBegin();
         it != mEventObjects.constEnd(); it++) {
        const QHash<QString, CalendarData::Event>::ConstIterator event = mEvents.find(it.key());
        if (event != mEvents.constEnd()) {
            it.value()->setEvent(&(*event));
        }
    }

    emit dataUpdated();
    mTimer->start();
}
