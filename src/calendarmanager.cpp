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
    : m_loadPending(false), m_resetPending(false)
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

    m_calendarWorker = new CalendarWorker();
    m_calendarWorker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_calendarWorker, &QObject::deleteLater);

    connect(m_calendarWorker, &CalendarWorker::storageModifiedSignal,
            this, &CalendarManager::storageModifiedSlot);

    connect(m_calendarWorker, &CalendarWorker::calendarTimezoneChanged,
            this, &CalendarManager::calendarTimezoneChangedSlot);

    connect(m_calendarWorker, &CalendarWorker::eventNotebookChanged,
            this, &CalendarManager::eventNotebookChanged);

    connect(m_calendarWorker, &CalendarWorker::excludedNotebooksChanged,
            this, &CalendarManager::excludedNotebooksChangedSlot);
    connect(m_calendarWorker, &CalendarWorker::notebooksChanged,
            this, &CalendarManager::notebooksChangedSlot);

    connect(m_calendarWorker, &CalendarWorker::dataLoaded,
            this, &CalendarManager::dataLoadedSlot);

    connect(m_calendarWorker, &CalendarWorker::searchResults,
            this, &CalendarManager::onSearchResults);

    connect(m_calendarWorker, &CalendarWorker::findMatchingEventFinished,
            this, &CalendarManager::findMatchingEventFinished);

    m_workerThread.setObjectName("calendarworker");
    m_workerThread.start();

    QMetaObject::invokeMethod(m_calendarWorker, "init", Qt::QueuedConnection);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(5);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
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
    m_workerThread.quit();
    m_workerThread.wait();
    if (managerInstance == this) {
        managerInstance = nullptr;
    }
}

QList<CalendarData::Notebook> CalendarManager::notebooks()
{
    return m_notebooks.values();
}

QString CalendarManager::defaultNotebook() const
{
    foreach (const CalendarData::Notebook &notebook, m_notebooks) {
        if (notebook.isDefault)
            return notebook.uid;
    }
    return QString();
}

void CalendarManager::setDefaultNotebook(const QString &notebookUid)
{
    QMetaObject::invokeMethod(m_calendarWorker, "setDefaultNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid));
}

CalendarStoredEvent* CalendarManager::eventObject(const QString &instanceId)
{
    QHash<QString, CalendarStoredEvent *>::ConstIterator it = m_eventObjects.find(instanceId);
    if (it != m_eventObjects.end()) {
        return *it;
    }

    const QHash<QString, CalendarData::Event>::ConstIterator event = m_events.find(instanceId);
    if (event != m_events.constEnd()) {
        CalendarStoredEvent *calendarEvent = new CalendarStoredEvent(this, &(*event));
        m_eventObjects.insert(instanceId, calendarEvent);
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
    QMetaObject::invokeMethod(m_calendarWorker, "saveEvent", Qt::QueuedConnection,
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
    QMetaObject::invokeMethod(m_calendarWorker, "dissociateSingleOccurrence",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(CalendarData::Event, event),
                              Q_ARG(QString, instanceId),
                              Q_ARG(QDateTime, datetime));
    return event;
}

QStringList CalendarManager::excludedNotebooks()
{
    return m_excludedNotebooks;
}

void CalendarManager::setExcludedNotebooks(const QStringList &list)
{
    QStringList sorted;
    sorted.append(list);
    sorted.sort();
    if (m_excludedNotebooks == sorted)
        return;

    QMetaObject::invokeMethod(m_calendarWorker, "setExcludedNotebooks", Qt::QueuedConnection,
                              Q_ARG(QStringList, sorted));
}

void CalendarManager::excludeNotebook(const QString &notebookUid, bool exclude)
{
    QMetaObject::invokeMethod(m_calendarWorker, "excludeNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid),
                              Q_ARG(bool, exclude));
}

void CalendarManager::setNotebookColor(const QString &notebookUid, const QString &color)
{
    QMetaObject::invokeMethod(m_calendarWorker, "setNotebookColor", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid),
                              Q_ARG(QString, color));
}

QString CalendarManager::getNotebookColor(const QString &notebookUid) const
{
    if (m_notebooks.contains(notebookUid))
        return m_notebooks.value(notebookUid, CalendarData::Notebook()).color;
    else
        return QString();
}

void CalendarManager::cancelSearch(CalendarSearchModel *model)
{
    m_searchList.removeOne(model);
}

void CalendarManager::search(CalendarSearchModel *model)
{
    if (m_searchList.contains(model))
        return;

    m_searchList.append(model);
    QList<CalendarSearchModel*>::ConstIterator it;
    for (it = m_searchList.constBegin(); it != m_searchList.constEnd(); it++) {
        if (model != *it && model->searchString() == (*it)->searchString())
            return;
    }
    QMetaObject::invokeMethod(m_calendarWorker, "search", Qt::QueuedConnection,
                              Q_ARG(QString, model->searchString()),
                              Q_ARG(int, model->limit()));
}

void CalendarManager::onSearchResults(const QString &searchString,
                                      const QStringList &identifiers)
{
    QList<CalendarSearchModel*>::Iterator it = m_searchList.begin();
    while (it != m_searchList.end()) {
        CalendarSearchModel *model = *it;
        if (model->searchString() == searchString) {
            it = m_searchList.erase(it);
            model->setIdentifiers(identifiers);
        } else {
            it++;
        }
    }
}

bool CalendarManager::isSearching(const CalendarSearchModel *model) const
{
    return m_searchList.contains(const_cast<CalendarSearchModel*>(model));
}

void CalendarManager::cancelAgendaRefresh(CalendarAgendaModel *model)
{
    m_agendaRefreshList.removeOne(model);
}

void CalendarManager::scheduleAgendaRefresh(CalendarAgendaModel *model)
{
    if (m_agendaRefreshList.contains(model))
        return;

    m_agendaRefreshList.append(model);

    m_timer->start();
}

void CalendarManager::cancelEventListRefresh(CalendarEventListModel *model)
{
    m_eventListRefreshList.removeOne(model);
}

void CalendarManager::scheduleEventListRefresh(CalendarEventListModel *model)
{
    if (m_eventListRefreshList.contains(model))
        return;

    m_eventListRefreshList.append(model);

    m_timer->start();
}

void CalendarManager::scheduleEventQueryRefresh(CalendarEventQuery *query)
{
    if (m_queryRefreshList.contains(query))
        return;

    m_queryRefreshList.append(query);

    m_timer->start();
}

void CalendarManager::cancelEventQueryRefresh(CalendarEventQuery *query)
{
    m_queryRefreshList.removeOne(query);
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
    if (m_loadedRanges.isEmpty()) {
        missingRanges->append(CalendarData::Range());
        missingRanges->last().first = r.first;
        missingRanges->last().second = r.second;
        return false;
    }

    QDate start(r.first);
    foreach (const CalendarData::Range range, m_loadedRanges) {
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
        foreach (const QString &id, m_eventOccurrenceForDates.value(model->startDate())) {
            if (m_eventOccurrences.contains(id)) {
                filtered.append(new CalendarEventOccurrence(m_eventOccurrences.value(id)));
            } else {
                qWarning() << "no occurrence with id" << id;
            }
        }
    } else {
        foreach (const CalendarData::EventOccurrence &eo, m_eventOccurrences.values()) {
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
                filtered.append(new CalendarEventOccurrence(eo));
            }
        }
    }

    model->doRefresh(filtered);
}

void CalendarManager::doAgendaAndQueryRefresh()
{
    QList<CalendarAgendaModel *> agendaModels = m_agendaRefreshList;
    m_agendaRefreshList.clear();
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
    if (m_resetPending) {
        missingRanges = addRanges(missingRanges, m_loadedRanges);
    }

    QStringList missingInstanceList;

    QList<CalendarEventQuery *> queryList = m_queryRefreshList;
    m_queryRefreshList.clear();
    foreach (CalendarEventQuery *query, queryList) {
        const QString instanceId = query->instanceId();
        if (instanceId.isEmpty())
            continue;

        bool loaded = m_loadedQueries.contains(instanceId);
        CalendarData::Event event = m_events.value(instanceId);
        if (((!event.isValid() && !loaded) || m_resetPending)
                && !missingInstanceList.contains(instanceId)) {
            missingInstanceList << instanceId;
        }
        query->doRefresh(event, !event.isValid() && loaded);
    }

    const QList<CalendarEventListModel *> eventListModels = m_eventListRefreshList;
    m_eventListRefreshList.clear();
    for (CalendarEventListModel *model : eventListModels) {
        for (const QString &id : model->identifiers()) {
            if (id.isEmpty())
                continue;

            bool loaded = m_loadedQueries.contains(id);
            CalendarData::Event event = m_events.value(id);
            if (((!event.isValid() && !loaded) || m_resetPending)
                && !missingInstanceList.contains(id)) {
                missingInstanceList << id;
            }
        }
    }

    if ((!missingRanges.isEmpty() || !missingInstanceList.isEmpty())
        && !m_loadPending) {
        m_loadPending = true;
        QMetaObject::invokeMethod(m_calendarWorker, "loadData", Qt::QueuedConnection,
                                  Q_ARG(QList<CalendarData::Range>, missingRanges),
                                  Q_ARG(QStringList, missingInstanceList),
                                  Q_ARG(bool, m_resetPending));
        m_resetPending = false;
    }
}

void CalendarManager::timeout()
{
    if (!m_agendaRefreshList.isEmpty()
        || !m_queryRefreshList.isEmpty()
        || !m_eventListRefreshList.isEmpty() || m_resetPending)
        doAgendaAndQueryRefresh();
}

void CalendarManager::deleteEvent(const QString &instanceId, const QDateTime &time)
{
    QMetaObject::invokeMethod(m_calendarWorker, "deleteEvent", Qt::QueuedConnection,
                              Q_ARG(QString, instanceId),
                              Q_ARG(QDateTime, time));
}

void CalendarManager::deleteAll(const QString &instanceId)
{
    QMetaObject::invokeMethod(m_calendarWorker, "deleteAll", Qt::QueuedConnection,
                              Q_ARG(QString, instanceId));
}

void CalendarManager::save()
{
    QMetaObject::invokeMethod(m_calendarWorker, "save", Qt::QueuedConnection);
}

QString CalendarManager::convertEventToICalendarSync(const QString &instanceId, const QString &prodId)
{
    QString vEvent;
    QMetaObject::invokeMethod(m_calendarWorker, "convertEventToICalendar", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, vEvent),
                              Q_ARG(QString, instanceId),
                              Q_ARG(QString, prodId));
    return vEvent;
}

CalendarData::Event CalendarManager::getEvent(const QString &instanceId, bool *loaded) const
{
    if (loaded) {
        *loaded = m_loadedQueries.contains(instanceId);
    }
    return m_events.value(instanceId);
}

bool CalendarManager::sendResponse(const QString &instanceId, CalendarEvent::Response response)
{
    bool result;
    QMetaObject::invokeMethod(m_calendarWorker, "sendResponse", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(QString, instanceId),
                              Q_ARG(CalendarEvent::Response, response));
    return result;
}

void CalendarManager::scheduleInvitationQuery(CalendarInvitationQuery *query, const QString &invitationFile)
{
    m_invitationQueryHash.insert(query, invitationFile);
    QMetaObject::invokeMethod(m_calendarWorker, "findMatchingEvent", Qt::QueuedConnection,
                              Q_ARG(QString, invitationFile));
}

void CalendarManager::unRegisterInvitationQuery(CalendarInvitationQuery *query)
{
    m_invitationQueryHash.remove(query);
}

void CalendarManager::findMatchingEventFinished(const QString &invitationFile, const CalendarData::Event &event)
{
    QHash<CalendarInvitationQuery*, QString>::iterator it = m_invitationQueryHash.begin();
    while (it != m_invitationQueryHash.end()) {
        if (it.value() == invitationFile) {
            it.key()->queryResult(event);
            it = m_invitationQueryHash.erase(it);
        } else {
            it++;
        }
    }
}

void CalendarManager::storageModifiedSlot()
{
    m_resetPending = true;
    emit storageModified();
}

void CalendarManager::calendarTimezoneChangedSlot()
{
    QHash<QString, CalendarStoredEvent *>::ConstIterator it;
    for (it = m_eventObjects.constBegin(); it != m_eventObjects.constEnd(); it++) {
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

void CalendarManager::eventNotebookChanged(const QString &oldInstanceId,
                                           const QString &newInstanceId,
                                           const QString &notebookUid)
{
    if (m_events.contains(oldInstanceId)) {
        m_events.insert(newInstanceId, m_events.value(oldInstanceId));
        m_events[newInstanceId].calendarUid = notebookUid;
        m_events.remove(oldInstanceId);
    }
    // newInstanceId points to the same object than oldInstanceId
    // to avoid CalendarEventQuery or CalendarEventOccurrence to
    // emit object changed.
    if (m_eventObjects.contains(oldInstanceId)) {
        m_eventObjects.insert(newInstanceId, m_eventObjects.value(oldInstanceId));
        m_eventObjects.remove(oldInstanceId);
    }
    emit instanceIdChanged(oldInstanceId, newInstanceId, notebookUid);
}

void CalendarManager::excludedNotebooksChangedSlot(const QStringList &excludedNotebooks)
{
    QStringList sortedExcluded = excludedNotebooks;
    sortedExcluded.sort();
    if (m_excludedNotebooks != sortedExcluded) {
        m_excludedNotebooks = sortedExcluded;
        emit excludedNotebooksChanged(m_excludedNotebooks);
        m_resetPending = true;
        m_timer->start();
    }
}

void CalendarManager::notebooksChangedSlot(const QList<CalendarData::Notebook> &notebooks)
{
    QHash<QString, CalendarData::Notebook> newNotebooks;
    QStringList colorChangers;
    QString newDefaultNotebookUid;
    bool changed = false;
    foreach (const CalendarData::Notebook &notebook, notebooks) {
        if (m_notebooks.contains(notebook.uid)) {
            if (m_notebooks.value(notebook.uid) != notebook) {
                changed = true;
                if (m_notebooks.value(notebook.uid).color != notebook.color)
                    colorChangers << notebook.uid;
            }
        }
        if (notebook.isDefault) {
            if (!m_notebooks.contains(notebook.uid) || !m_notebooks.value(notebook.uid).isDefault)
                newDefaultNotebookUid = notebook.uid;
        }

        newNotebooks.insert(notebook.uid, notebook);
    }

    if (changed || m_notebooks.count() != newNotebooks.count()) {
        emit notebooksAboutToChange();
        m_notebooks = newNotebooks;
        emit notebooksChanged(m_notebooks.values());
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
    const CalendarData::Event event = m_events.value(instanceId);
    if (event.recur == CalendarEvent::RecurOnce) {
        const QTimeZone systemTimeZone = QTimeZone::systemTimeZone();
        eo.instanceId = event.instanceId;
        eo.startTime = event.startTime.toTimeZone(systemTimeZone);
        eo.endTime = event.endTime.toTimeZone(systemTimeZone);
    } else {
        QMetaObject::invokeMethod(m_calendarWorker, "getNextOccurrence", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(CalendarData::EventOccurrence, eo),
                                  Q_ARG(QString, instanceId),
                                  Q_ARG(QDateTime, start));
    }

    if (!eo.startTime.isValid()) {
        qWarning() << Q_FUNC_INFO << "Unable to find occurrence for event" << instanceId;
        return new CalendarEventOccurrence();
    }

    return new CalendarEventOccurrence(eo);
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
    *resultValid = !(m_loadPending || m_resetPending);
    if (*resultValid) {
        QMetaObject::invokeMethod(m_calendarWorker, "getEventAttendees", Qt::BlockingQueuedConnection,
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
        m_events.clear();
        m_eventOccurrences.clear();
        m_eventOccurrenceForDates.clear();
        m_loadedRanges.clear();
        m_loadedQueries.clear();
    }

    m_loadedRanges = addRanges(m_loadedRanges, ranges);
    m_loadedQueries.append(instanceList);
    m_events = m_events.unite(events);
    // Use m_eventOccurrences.insert(occurrences) from Qt5.15,
    // .unite() is deprecated and broken, it is duplicating keys.
    for (const CalendarData::EventOccurrence &eo: occurrences)
        m_eventOccurrences.insert(eo.getId(), eo);
    for (QHash<QDate, QStringList>::ConstIterator it = dailyOccurrences.constBegin();
         it != dailyOccurrences.constEnd(); ++it)
        m_eventOccurrenceForDates.insert(it.key(), it.value());
    m_loadPending = false;

    for (QHash<QString, CalendarStoredEvent *>::ConstIterator it = m_eventObjects.constBegin();
         it != m_eventObjects.constEnd(); it++) {
        const QHash<QString, CalendarData::Event>::ConstIterator event = m_events.find(it.key());
        if (event != m_events.constEnd()) {
            it.value()->setEvent(&(*event));
        }
    }

    emit dataUpdated();
    m_timer->start();
}
