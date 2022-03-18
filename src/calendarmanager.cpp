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
    qRegisterMetaType<CalendarData::Incidence>("CalendarData::Incidence");
    qRegisterMetaType<QMultiHash<QString,CalendarData::Incidence> >("QMultiHash<QString,CalendarData::Incidence>");
    qRegisterMetaType<QHash<QDate,QStringList> >("QHash<QDate,QStringList>");
    qRegisterMetaType<CalendarData::Range>("CalendarData::Range");
    qRegisterMetaType<QList<CalendarData::Range > >("QList<CalendarData::Range>");
    qRegisterMetaType<QList<CalendarData::Notebook> >("QList<CalendarData::Notebook>");
    qRegisterMetaType<KCalendarCore::Person::List>("KCalendarCore::Person::List");
    qRegisterMetaType<KCalendarCore::Incidence::Ptr>("KCalendarCore::Incidence::Ptr");

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

CalendarStoredEvent* CalendarManager::findEventObject(const QString &eventUid, const QDateTime &recurrenceId)
{
    QMultiHash<QString, CalendarStoredEvent *>::iterator it = mEventObjects.find(eventUid);
    while (it != mEventObjects.end() && it.key() == eventUid) {
        if ((*it)->recurrenceId() == recurrenceId) {
            return *it;
        }
        ++it;
    }

    return nullptr;
}

CalendarStoredEvent* CalendarManager::eventObject(const QString &eventUid, const QDateTime &recurrenceId)
{
    CalendarStoredEvent *object = findEventObject(eventUid, recurrenceId);
    if (object)
        return object;

    CalendarData::Incidence event = getIncidence(eventUid, recurrenceId);
    if (event.incidence) {
        CalendarData::Notebook notebook = mNotebooks.value(event.calendarUid);
        object = new CalendarStoredEvent(this, KCalendarCore::Incidence::Ptr(event.incidence->clone()), notebook);
        mEventObjects.insert(eventUid, object);
        return object;
    }

    // TODO: maybe attempt to read event from DB? This situation should not happen.
    qWarning() << Q_FUNC_INFO << "No event with uid" << eventUid << recurrenceId << ", returning empty event";

    return new CalendarStoredEvent(this, {}, {});
}

void CalendarManager::saveModification(const KCalendarCore::Incidence::Ptr &incidence,
                                       const QString &calendarUid)
{
    if (!incidence)
        return;
    // The worker will work on a shared KCalendarCore::Incidence with the
    // manager. Detach this incidence to avoid concurrent access with the
    // worker. The updated version will be retrieved by the manager after
    // the storageModified() signal.
    detachIncidence(incidence->uid(), incidence->recurrenceId());
    QMetaObject::invokeMethod(mCalendarWorker, "saveEvent", Qt::QueuedConnection,
                              Q_ARG(KCalendarCore::Incidence::Ptr, incidence),
                              Q_ARG(QString, calendarUid));
}

KCalendarCore::Incidence::Ptr CalendarManager::dissociateSingleOccurrence(const QString &eventUid, const QDateTime &recurrenceId) const
{
    KCalendarCore::Incidence::Ptr event;
    // Worker method is not calling any storage method that could block.
    // The only blocking possibility here would be to obtain the worker thread
    // availability.
    QMetaObject::invokeMethod(mCalendarWorker, "dissociateSingleOccurrence",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(KCalendarCore::Incidence::Ptr, event),
                              Q_ARG(QString, eventUid),
                              Q_ARG(QDateTime, recurrenceId));
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

QString CalendarManager::getNotebookEmail(const QString &notebookUid) const
{
    return mNotebooks.value(notebookUid).emailAddress;
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

            const QDateTime startDt(model->startDate()); // To be replaced later by start.startOfDay()
            const QDateTime endDt(model->endDate()); // To be replaced later by start.startOfDay()
            // on all day events the end time is inclusive, otherwise not
            if ((eo.eventAllDay && eo.startTime.date() <= model->endDate()
                 && eo.endTime.date() >= model->startDate())
                || (!eo.eventAllDay && eo.startTime < endDt.addDays(1)
                    && eo.endTime >= startDt)) {
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

    QStringList missingInstanceList;

    QList<CalendarEventQuery *> queryList = mQueryRefreshList;
    mQueryRefreshList.clear();
    foreach (CalendarEventQuery *query, queryList) {
        const QString eventUid = query->uniqueId();
        if (eventUid.isEmpty())
            continue;

        const QDateTime recurrenceId = query->recurrenceId();
        KCalendarCore::Event missing;
        missing.setUid(eventUid);
        missing.setRecurrenceId(recurrenceId);
        const QString id = missing.instanceIdentifier();
        bool loaded = mLoadedQueries.contains(id);
        CalendarData::Incidence event = getIncidence(eventUid, recurrenceId);
        if (((!event.incidence && !loaded) || mResetPending)
                && !missingInstanceList.contains(id)) {
            missingInstanceList << id;
        }
        query->doRefresh(event.incidence, !event.incidence && loaded);
    }

    const QList<CalendarEventListModel *> eventListModels = mEventListRefreshList;
    mEventListRefreshList.clear();
    for (CalendarEventListModel *model : eventListModels) {
        for (const QString &id : model->identifiers()) {
            if (id.isEmpty())
                continue;

            bool loaded;
            CalendarData::Incidence event = getIncidence(id, &loaded);
            if (((!event.incidence && !loaded) || mResetPending)
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

void CalendarManager::deleteEvent(const QString &uid, const QDateTime &recurrenceId, const QDateTime &time)
{
    // In case of adding an exdate, the worker will work on a shared
    // KCalendarCore::Incidence with the manager. Detach this incidence
    // to avoid concurrent access with the worker.
    detachIncidence(uid, recurrenceId);
    QMetaObject::invokeMethod(mCalendarWorker, "deleteEvent", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, recurrenceId),
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

void CalendarManager::detachIncidence(const QString &uid,
                                      const QDateTime &recurrenceId)
{
    QMultiHash<QString, CalendarData::Incidence>::iterator it = mEvents.find(uid);
    while (it != mEvents.end() && it.key() == uid) {
        if (it->incidence->recurrenceId() == recurrenceId) {
            it->incidence.reset(it->incidence->clone());
            const QString identifier = it->incidence->instanceIdentifier();
            if (identifier != uid) {
                const KCalendarCore::Incidence::Ptr clone = it->incidence;
                it = mEvents.find(identifier);
                if (it != mEvents.end()) {
                    it->incidence = clone;
                }
            }
            break;
        }
        ++it;
    }
}

CalendarData::Incidence CalendarManager::getIncidence(const QString &uid, const QDateTime &recurrenceId) const
{
    QMultiHash<QString, CalendarData::Incidence>::ConstIterator it = mEvents.find(uid);
    while (it != mEvents.end() && it.key() == uid) {
        if (it.value().incidence->recurrenceId() == recurrenceId) {
            return it.value();
        }
        ++it;
    }

    return CalendarData::Incidence();
}

CalendarData::Incidence CalendarManager::getIncidence(const QString &instanceIdentifier, bool *loaded) const
{
    if (loaded) {
        *loaded = mLoadedQueries.contains(instanceIdentifier);
    }
    // See CalendarWorker::loadData(), in case where instanceIdentifier is not the
    // UID, the event structure is duplicated with the key as the instanceIdentifier.
    QList<CalendarData::Incidence> events = mEvents.values(instanceIdentifier);
    if (events.count() == 1) {
        // Either the event is not recurring or it's an exception.
        return events[0];
    } else if (events.count() > 1) {
        // The event is recurring with exception, we look for the parent.
        QList<CalendarData::Incidence>::ConstIterator it = events.constBegin();
        while (it != events.constEnd()) {
            if (!it->incidence->hasRecurrenceId()) {
                return *it;
            }
            ++it;
        }
    }

    return CalendarData::Incidence();
}

bool CalendarManager::sendResponse(const QString &uid, const QDateTime &recurrenceId, CalendarEvent::Response response)
{
    bool result;
    QMetaObject::invokeMethod(mCalendarWorker, "sendResponse", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, recurrenceId),
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

void CalendarManager::findMatchingEventFinished(const QString &invitationFile,
                                                const CalendarData::Incidence &event)
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

CalendarEventOccurrence* CalendarManager::getOccurrence(const QString &instanceIdentifier, bool *loaded)
{
    CalendarData::Incidence event = getIncidence(instanceIdentifier, loaded);
    if (event.incidence)
        return new CalendarEventOccurrence(event.incidence->uid(), event.incidence->recurrenceId(), event.incidence->dtStart(), event.incidence->dateTime(KCalendarCore::Incidence::RoleEnd));
    else
        return nullptr;
}

CalendarEventOccurrence* CalendarManager::getNextOccurrence(const QString &uid, const QDateTime &recurrenceId,
                                                            const QDateTime &start)
{
    CalendarData::EventOccurrence eo;
    QMetaObject::invokeMethod(mCalendarWorker, "getNextOccurrence", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(CalendarData::EventOccurrence, eo),
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, recurrenceId),
                              Q_ARG(QDateTime, start));

    if (!eo.startTime.isValid()) {
        qWarning() << Q_FUNC_INFO << "Unable to find occurrence for event" << uid << recurrenceId;
        return new CalendarEventOccurrence(QString(), QDateTime(), QDateTime(), QDateTime());
    }

    return new CalendarEventOccurrence(eo.eventUid, eo.recurrenceId, eo.startTime, eo.endTime);
}

void CalendarManager::dataLoadedSlot(const QList<CalendarData::Range> &ranges,
                                     const QStringList &instanceList,
                                     const QMultiHash<QString, CalendarData::Incidence> &events,
                                     const QHash<QString, CalendarData::EventOccurrence> &occurrences,
                                     const QHash<QDate, QStringList> &dailyOccurrences,
                                     bool reset)
{
    QList<CalendarData::Incidence> oldEvents;
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

    foreach (const CalendarData::Incidence &oldEvent, oldEvents) {
        CalendarStoredEvent *object = findEventObject(oldEvent.incidence->uid(),
                                                      oldEvent.incidence->recurrenceId());
        CalendarData::Incidence newEvent = getIncidence(oldEvent.incidence->uid(),
                                                        oldEvent.incidence->recurrenceId());
        if (object && newEvent.incidence) {
            object->setEvent(newEvent.incidence,
                             mNotebooks.value(newEvent.calendarUid));
        }
    }

    emit dataUpdated();
    mTimer->start();
}
