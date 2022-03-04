/*
 * Copyright (c) 2014 - 2019 Jolla Ltd.
 * Copyright (c) 2019 - 2021 Open Mobile Platform LLC.
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

#include "calendarworker.h"
#include "calendarutils.h"

#include <QDebug>
#include <QSettings>

// mkcal
#include <notebook.h>
#include <servicehandler.h>

// KCalendarCore
#include <KCalendarCore/Attendee>
#include <KCalendarCore/Event>
#include <KCalendarCore/CalFormat>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/Recurrence>
#include <KCalendarCore/RecurrenceRule>
#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/OccurrenceIterator>

// libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Provider>
#include <Accounts/Account>

namespace {
    void updateAttendee(KCalendarCore::Event::Ptr event,
                        const KCalendarCore::Attendee &attendee,
                        const KCalendarCore::Attendee &updated)
    {
        KCalendarCore::Attendee::List allAttendees = event->attendees();
        for (int i = 0; i < allAttendees.size(); ++i) {
            if (allAttendees[i] == attendee) {
                allAttendees.replace(i, updated);
                break;
            }
        }
        event->setAttendees(allAttendees);
    }
}

CalendarWorker::CalendarWorker()
    : QObject(0), mAccountManager(0), mHasRecurringEvents(false)
{
}

CalendarWorker::~CalendarWorker()
{
    if (mStorage.data())
        mStorage->close();

    mCalendar.clear();
    mStorage.clear();
}

void CalendarWorker::storageModified(mKCal::ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage)
    Q_UNUSED(info)

    // 'info' is either a path to the database (in which case we're screwed, we
    // have no idea what changed, so tell all interested models to reload) or a
    // space-seperated list of event UIDs.
    //
    // unfortunately we don't know *what* about these events changed with the
    // current mkcal API, so we'll have to try our best to guess when the time
    // comes.
    mSentEvents.clear();
    loadNotebooks();
    emit storageModifiedSignal(info);
}

void CalendarWorker::storageProgress(mKCal::ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage)
    Q_UNUSED(info)
}

void CalendarWorker::storageFinished(mKCal::ExtendedStorage *storage, bool error, const QString &info)
{
    Q_UNUSED(storage)
    Q_UNUSED(error)
    Q_UNUSED(info)
}

void CalendarWorker::deleteEvent(const QString &uid, const QDateTime &recurrenceId, const QDateTime &dateTime)
{
    KCalendarCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);
    if (!event && mStorage->load(uid, recurrenceId)) {
        event = mCalendar->event(uid, recurrenceId);
    }
    if (!event) {
        qDebug() << uid << "event already deleted from DB";
        return;
    }

    if (event->recurs() && dateTime.isValid()) {
        // We're deleting an occurrence from a recurring event.
        // No incidence is deleted from the database in that case,
        // only the base incidence is modified by adding an exDate.
        if (dateTime.timeSpec() == Qt::LocalTime && event->dtStart().timeSpec() != Qt::LocalTime)
            event->recurrence()->addExDateTime(dateTime.toTimeZone(event->dtStart().timeZone()));
        else
            event->recurrence()->addExDateTime(dateTime);
        event->setRevision(event->revision() + 1);
    } else {
        mCalendar->deleteEvent(event);
        mDeletedEvents.append(QPair<QString, QDateTime>(uid, recurrenceId));
    }
}

void CalendarWorker::deleteAll(const QString &uid)
{
    KCalendarCore::Event::Ptr event = mCalendar->event(uid);
    if (!event && mStorage->loadSeries(uid)) {
        event = mCalendar->event(uid);
    }
    if (!event) {
        qDebug() << uid << "event already deleted from DB";
        return;
    }

    mCalendar->deleteEventInstances(event);
    mCalendar->deleteEvent(event);
    mDeletedEvents.append(QPair<QString, QDateTime>(uid, QDateTime()));
}

bool CalendarWorker::sendResponse(const CalendarData::Event &eventData, const CalendarEvent::Response response)
{
    KCalendarCore::Event::Ptr event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);
    if (!event) {
        qWarning() << "Failed to send response, event not found. UID = " << eventData.uniqueId;
        return false;
    }
    const QString &notebookUid = mCalendar->notebook(event);
    const QString &ownerEmail = mNotebooks.contains(notebookUid) ? mNotebooks.value(notebookUid).emailAddress
                                                                 : QString();

    const KCalendarCore::Attendee origAttendee = event->attendeeByMail(ownerEmail);
    KCalendarCore::Attendee updated = origAttendee;
    updated.setStatus(CalendarUtils::convertResponse(response));
    updateAttendee(event, origAttendee, updated);

    bool sent = mKCal::ServiceHandler::instance().sendResponse(event, eventData.description, mCalendar, mStorage);

    if (!sent)
        updateAttendee(event, updated, origAttendee);

    save();
    return sent;
}

QString CalendarWorker::convertEventToICalendar(const QString &uid, const QString &prodId) const
{
    // NOTE: not fetching eventInstances() with different recurrenceId
    KCalendarCore::Event::Ptr event = mCalendar->event(uid);
    if (event.isNull()) {
        qWarning() << "No event with uid " << uid << ", unable to create iCalendar";
        return QString();
    }

    KCalendarCore::ICalFormat fmt;
    fmt.setApplication(fmt.application(),
                       prodId.isEmpty() ? QLatin1String("-//sailfishos.org/Sailfish//NONSGML v1.0//EN") : prodId);
    return fmt.toICalString(event);
}

void CalendarWorker::save()
{
    mStorage->save();
    // FIXME: should send response update if deleting an even we have responded to.
    // FIXME: should send cancel only if we own the event
    if (!mDeletedEvents.isEmpty()) {
        for (const QPair<QString, QDateTime> &pair: mDeletedEvents) {
            KCalendarCore::Event::Ptr event = mCalendar->deletedEvent(pair.first, pair.second);
            if (needSendCancellation(event)) {
                event->setStatus(KCalendarCore::Incidence::StatusCanceled);
                mKCal::ServiceHandler::instance().sendUpdate(event, QString(), mCalendar, mStorage);
            }
            // if the event was stored in a local (non-synced) notebook, purge it.
            const QString notebookUid = mCalendar->notebook(event);
            const mKCal::Notebook::Ptr notebook = mStorage->notebook(notebookUid);
            if (!notebook.isNull() && notebook->pluginName().isEmpty() && notebook->account().isEmpty()
                    && !mStorage->purgeDeletedIncidences(KCalendarCore::Incidence::List() << event)) {
                qWarning() << "Failed to purge deleted event " << event->uid() << " from local calendar " << notebookUid;
            }
        }
        mDeletedEvents.clear();
    }
}

void CalendarWorker::saveEvent(const CalendarData::Event &eventData, bool updateAttendees,
                               const QList<CalendarData::EmailContact> &required,
                               const QList<CalendarData::EmailContact> &optional)
{
    QString notebookUid = eventData.calendarUid;

    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid)) {
        qWarning() << "Invalid notebook uid:" << notebookUid;
        return;
    }

    KCalendarCore::Event::Ptr event;
    bool createNew = eventData.uniqueId.isEmpty();

    if (createNew) {
        event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);

        // For exchange it is better to use upper case UIDs, because for some reason when
        // UID is generated out of Global object id of the email message we are getting a lowercase
        // UIDs, but original UIDs for invitations/events sent from Outlook Web interface are in
        // upper case. To workaround such behaviour it is easier for us to generate an upper case UIDs
        // for new events than trying to implement some complex logic in basesailfish-eas.
        event->setUid(event->uid().toUpper());
    } else {
        event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);

        if (!event) {
            // possibility that event was removed while changes were edited. options to either skip, as done now,
            // or resurrect the event
            qWarning("Event to be saved not found");
            return;
        }

        if (!notebookUid.isEmpty() && mCalendar->notebook(event) != notebookUid) {
            // mkcal does funny things when moving event between notebooks, work around by changing uid
            KCalendarCore::Event::Ptr newEvent = KCalendarCore::Event::Ptr(event->clone());
            newEvent->setUid(KCalendarCore::CalFormat::createUniqueId().toUpper());
            emit eventNotebookChanged(event->uid(), newEvent->uid(), notebookUid);
            mCalendar->deleteEvent(event);
            mCalendar->addEvent(newEvent, notebookUid);
            event = newEvent;
        } else {
            event->setRevision(event->revision() + 1);
        }
    }

    eventData.toKCalendarCore(event);

    if (updateAttendees) {
        updateEventAttendees(event, createNew, required, optional, notebookUid);
    }

    if (createNew) {
        bool eventAdded;
        if (notebookUid.isEmpty())
            eventAdded = mCalendar->addEvent(event);
        else
            eventAdded = mCalendar->addEvent(event, notebookUid);
        if (!eventAdded) {
            qWarning() << "Cannot add event" << event->uid() << ", notebookUid:" << notebookUid;
            return;
        }
    }

    save();
}

void CalendarWorker::replaceOccurrence(const CalendarData::Event &eventData, const QDateTime &startTime,
                                       bool updateAttendees,
                                       const QList<CalendarData::EmailContact> &required,
                                       const QList<CalendarData::EmailContact> &optional)
{
    QString notebookUid = eventData.calendarUid;
    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid)) {
        qWarning("replaceOccurrence() - invalid notebook given");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    KCalendarCore::Event::Ptr event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);
    if (!event) {
        qWarning("Event to create occurrence replacement for not found");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    // Note: for all day events, to guarantee that exception set in a given time
    // zone is also an exception when travelling to another time, we use the
    // LocalTime spec.
    QDateTime occurrence = event->allDay()
            ? QDateTime(startTime.date(), startTime.time(), Qt::LocalTime)
            : startTime;

    KCalendarCore::Incidence::Ptr replacementIncidence = mCalendar->dissociateSingleOccurrence(
            event, occurrence);
    KCalendarCore::Event::Ptr replacement = replacementIncidence.staticCast<KCalendarCore::Event>();
    if (!replacement) {
        qWarning("Didn't find event occurrence to replace");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    eventData.toKCalendarCore(replacement);

    if (updateAttendees) {
        updateEventAttendees(replacement, false, required, optional, notebookUid);
    }

    mCalendar->addEvent(replacement, notebookUid);

    emit occurrenceExceptionCreated(eventData, startTime, replacement->recurrenceId());
    save();
}

void CalendarWorker::init()
{
    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mStorage = mCalendar->defaultStorage(mCalendar);
    mStorage->open();
    mStorage->registerObserver(this);
    loadNotebooks();
}

bool CalendarWorker::needSendCancellation(KCalendarCore::Event::Ptr &event) const
{
    if (!event) {
        qWarning() << Q_FUNC_INFO << "event is NULL";
        return false;
    }
    KCalendarCore::Attendee::List attendees = event->attendees();
    if (attendees.size() == 0) {
        return false;
    }
    KCalendarCore::Person calOrganizer = event->organizer();
    if (calOrganizer.isEmpty()) {
        return false;
    }
    // we shouldn't send a response if we are not an organizer
    if (calOrganizer.email() != getNotebookAddress(event)) {
        return false;
    }
    return true;
}

// use explicit notebook uid so we don't need to assume the events involved being added there.
// the related notebook is just needed to associate updates to some plugin/account
void CalendarWorker::updateEventAttendees(KCalendarCore::Event::Ptr event, bool newEvent,
                                          const QList<CalendarData::EmailContact> &required,
                                          const QList<CalendarData::EmailContact> &optional,
                                          const QString &notebookUid)
{
    if (notebookUid.isEmpty()) {
        qWarning() << "No notebook passed, refusing to send event updates from random source";
        return;
    }

    mKCal::Notebook::Ptr notebook = mStorage->notebook(notebookUid);
    if (notebook.isNull()) {
        qWarning() << "No notebook found with UID" << notebookUid;
        return;
    }

    // set the notebook email address as the organizer email address
    // if no explicit organizer is set (i.e. assume we are the organizer).
    const QString notebookOwnerEmail = getNotebookAddress(notebookUid);
    if (event->organizer().email().isEmpty() && !notebookOwnerEmail.isEmpty()) {
        KCalendarCore::Person organizer = event->organizer();
        organizer.setEmail(notebookOwnerEmail);
        event->setOrganizer(organizer);
    }

    if (!newEvent) {
        // if existing attendees are removed, those should get a cancel update
        KCalendarCore::Event::Ptr cancelEvent = KCalendarCore::Event::Ptr(event->clone());
        KCalendarCore::Attendee::List cancelAttendees = cancelEvent->attendees();
        KCalendarCore::Attendee::List attendees = event->attendees();

        // first remove everyone still listed as included
        for (int i = 0; i < required.length(); ++i) {
            const KCalendarCore::Attendee toRemove = cancelEvent->attendeeByMail(required.at(i).email);
            if (!toRemove.email().isEmpty()) {
                cancelAttendees.removeOne(toRemove);
            }
        }
        for (int i = 0; i < optional.length(); ++i) {
            const KCalendarCore::Attendee toRemove = cancelEvent->attendeeByMail(optional.at(i).email);
            if (!toRemove.email().isEmpty()) {
                cancelAttendees.removeOne(toRemove);
            }
        }

        const QString organizer = cancelEvent->organizer().email();
        if (!organizer.isEmpty()) {
            const KCalendarCore::Attendee toRemove = cancelEvent->attendeeByMail(organizer);
            if (!toRemove.email().isEmpty()) {
                cancelAttendees.removeOne(toRemove);
            }
        }

        bool attendeesChanged = false;
        for (int i = cancelAttendees.length() - 1; i >= 0; --i) {
            const KCalendarCore::Attendee attendee = cancelAttendees.at(i);

            // if there are non-participants getting update as FYI, or chair for any reason,
            // avoid sending them the cancel
            if (attendee.role() != KCalendarCore::Attendee::ReqParticipant
                    && attendee.role() != KCalendarCore::Attendee::OptParticipant) {
                cancelAttendees.removeAt(i);
                continue;
            }

            // this one really gets cancel so remove from update event side
            KCalendarCore::Attendee toRemove = event->attendeeByMail(attendee.email());
            if (!toRemove.email().isEmpty()) {
                attendeesChanged = true;
                attendees.removeOne(toRemove);
            }
        }

        if (attendeesChanged) {
            event->setAttendees(attendees);
        }

        if (cancelAttendees.size()) {
            cancelEvent->setAttendees(cancelAttendees);
            cancelEvent->setStatus(KCalendarCore::Incidence::StatusCanceled);
            mKCal::ServiceHandler::instance().sendUpdate(cancelEvent, QString(), mCalendar, mStorage, notebook);
        }
    }

    if (required.length() > 0 || optional.length() > 0) {
        for (int i = 0; i < required.length(); ++i) {
            const KCalendarCore::Attendee existing = event->attendeeByMail(required.at(i).email);
            if (!existing.email().isEmpty()) {
                KCalendarCore::Attendee updated = existing;
                updated.setRole(KCalendarCore::Attendee::ReqParticipant);
                updateAttendee(event, existing, updated);
            } else {
                event->addAttendee(KCalendarCore::Attendee(
                        required.at(i).name, required.at(i).email, true /* rsvp */,
                        KCalendarCore::Attendee::NeedsAction,
                        KCalendarCore::Attendee::ReqParticipant));
            }
        }
        for (int i = 0; i < optional.length(); ++i) {
            const KCalendarCore::Attendee existing = event->attendeeByMail(optional.at(i).email);
            if (!existing.email().isEmpty()) {
                KCalendarCore::Attendee updated = existing;
                updated.setRole(KCalendarCore::Attendee::OptParticipant);
                updateAttendee(event, existing, updated);
            } else {
                event->addAttendee(KCalendarCore::Attendee(
                        optional.at(i).name, optional.at(i).email, true,
                        KCalendarCore::Attendee::NeedsAction,
                        KCalendarCore::Attendee::OptParticipant));
            }
        }

        // The separation between sendInvitation and sendUpdate it not really good,
        // when modifying an existing event and adding attendees, should it be which?
        // Probably those should be combined into a single function on the API, but
        // until that is done, let's just handle new events as invitations and rest as updates.
        if (newEvent) {
            mKCal::ServiceHandler::instance().sendInvitation(event, QString(), mCalendar, mStorage, notebook);
        } else {
            mKCal::ServiceHandler::instance().sendUpdate(event, QString(), mCalendar, mStorage, notebook);
        }
    }
}

QString CalendarWorker::getNotebookAddress(const QString &notebookUid) const
{
    return mNotebooks.contains(notebookUid) ? mNotebooks.value(notebookUid).emailAddress
                                            : QString();
}

QString CalendarWorker::getNotebookAddress(const KCalendarCore::Event::Ptr &event) const
{
    const QString &notebookUid = mCalendar->notebook(event);
    return mNotebooks.contains(notebookUid) ? mNotebooks.value(notebookUid).emailAddress
                                            : QString();
}

QList<CalendarData::Notebook> CalendarWorker::notebooks() const
{
    return mNotebooks.values();
}

void CalendarWorker::excludeNotebook(const QString &notebookUid, bool exclude)
{
    if (saveExcludeNotebook(notebookUid, exclude)) {
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}

void CalendarWorker::setDefaultNotebook(const QString &notebookUid)
{
    if (mStorage->defaultNotebook() && mStorage->defaultNotebook()->uid() == notebookUid)
        return;

    mStorage->setDefaultNotebook(mStorage->notebook(notebookUid));
    mStorage->save();
}

QStringList CalendarWorker::excludedNotebooks() const
{
    QStringList excluded;
    foreach (const CalendarData::Notebook &notebook, mNotebooks.values()) {
        if (notebook.excluded)
            excluded << notebook.uid;
    }
    return excluded;
}

bool CalendarWorker::saveExcludeNotebook(const QString &notebookUid, bool exclude)
{
    QHash<QString, CalendarData::Notebook>::Iterator notebook = mNotebooks.find(notebookUid);
    if (notebook == mNotebooks.end())
        return false;
    bool changed = (notebook->excluded != exclude);
    notebook->excluded = exclude;

    // Ensure, mKCal backend is up-to-date on notebook visibility.
    const mKCal::Notebook::Ptr mkNotebook = mStorage->notebook(notebookUid);
    if (mkNotebook && mkNotebook->isVisible() != !exclude) {
        mkNotebook->setIsVisible(!exclude);
        mStorage->updateNotebook(mkNotebook);
    }

    return changed;
}

void CalendarWorker::setExcludedNotebooks(const QStringList &list)
{
    bool changed = false;

    QStringList excluded = excludedNotebooks();

    foreach (const QString &notebookUid, list) {
        if (!excluded.contains(notebookUid)) {
            if (saveExcludeNotebook(notebookUid, true))
                changed = true;
        }
    }

    foreach (const QString &notebookUid, excluded) {
        if (!list.contains(notebookUid)) {
            if (saveExcludeNotebook(notebookUid, false))
                changed = true;
        }
    }

    if (changed) {
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}

void CalendarWorker::setNotebookColor(const QString &notebookUid, const QString &color)
{
    if (!mNotebooks.contains(notebookUid))
        return;

    if (mNotebooks.value(notebookUid).color != color) {
        if (mKCal::Notebook::Ptr mkNotebook = mStorage->notebook(notebookUid)) {
            mkNotebook->setColor(color);
            mStorage->updateNotebook(mkNotebook);
        }

        CalendarData::Notebook notebook = mNotebooks.value(notebookUid);
        notebook.color = color;
        mNotebooks.insert(notebook.uid, notebook);

        emit notebooksChanged(mNotebooks.values());
    }
}

QHash<QString, CalendarData::EventOccurrence>
CalendarWorker::eventOccurrences(const QList<CalendarData::Range> &ranges) const
{
    const QStringList excluded = excludedNotebooks();
    QHash<QString, CalendarData::EventOccurrence> filtered;
    for (const CalendarData::Range range : ranges) {
        KCalendarCore::OccurrenceIterator it(*mCalendar, QDateTime(range.first.addDays(-1)),
                                             QDateTime(range.second.addDays(1)).addSecs(-1));
        while (it.hasNext()) {
            it.next();
            if (mCalendar->isVisible(it.incidence())
                && it.incidence()->type() == KCalendarCore::IncidenceBase::TypeEvent
                && !excluded.contains(mCalendar->notebook(it.incidence()))) {
                const QDateTime sdt = it.occurrenceStartDate();
                const KCalendarCore::Duration elapsed
                    (it.incidence()->dateTime(KCalendarCore::Incidence::RoleDisplayStart),
                     it.incidence()->dateTime(KCalendarCore::Incidence::RoleDisplayEnd),
                     KCalendarCore::Duration::Seconds);
                CalendarData::EventOccurrence occurrence;
                occurrence.eventUid = it.incidence()->uid();
                occurrence.recurrenceId = it.incidence()->recurrenceId();
                occurrence.startTime = sdt;
                occurrence.endTime = elapsed.end(sdt);
                occurrence.eventAllDay = it.incidence()->allDay();
                filtered.insert(occurrence.getId(), occurrence);
            }
        }
    }

    return filtered;
}

QHash<QDate, QStringList>
CalendarWorker::dailyEventOccurrences(const QList<CalendarData::Range> &ranges,
                                      const QList<CalendarData::EventOccurrence> &occurrences) const
{
    QHash<QDate, QStringList> occurrenceHash;
    for (const CalendarData::EventOccurrence &eo : occurrences) {
        // On all day events the end time is inclusive, otherwise not
        const QDate st = eo.eventAllDay ? eo.startTime.date() : eo.startTime.toLocalTime().date();
        const QDate ed = eo.eventAllDay ? eo.endTime.date() : eo.endTime.toLocalTime().addSecs(-1).date();

        for (const CalendarData::Range &range: ranges) {
            const QDate s = st < range.first ? range.first : st;
            const QDate e = ed > range.second ? range.second : ed;
            for (QDate date = s; date <= e; date = date.addDays(1)) {
                occurrenceHash[date].append(eo.getId());
            }
        }
    }
    return occurrenceHash;
}

void CalendarWorker::loadData(const QList<CalendarData::Range> &ranges,
                              const QStringList &instanceList,
                              bool reset)
{
    if (reset)
        mHasRecurringEvents = false;

    foreach (const CalendarData::Range &range, ranges)
        mStorage->load(range.first, range.second.addDays(1)); // end date is not inclusive

    foreach (const QString &id, instanceList)
        mStorage->loadIncidenceInstance(id);

    if (!ranges.isEmpty() && !mHasRecurringEvents) {
        // Load all recurring incidences,
        // we have no other way to detect if they occur within a range
        mStorage->loadRecurringIncidences();
        mHasRecurringEvents = true;
    }

    if (reset)
        mSentEvents.clear();

    QMultiHash<QString, CalendarData::Event> events;
    bool orphansDeleted = false;

    const KCalendarCore::Event::List list = mCalendar->rawEvents();
    for (const KCalendarCore::Event::Ptr e : list) {
        if (!mCalendar->isVisible(e)) {
            continue;
        }
        // The database may have changed after loading the events, make sure that the notebook
        // of the event still exists.
        mKCal::Notebook::Ptr notebook = mStorage->notebook(mCalendar->notebook(e));
        if (notebook.isNull()) {
            // This may be a symptom of a deeper bug: if a sync adapter (or mkcal)
            // doesn't delete events which belong to a deleted notebook, then the
            // events will be "orphan" and need to be deleted.
            if (mStorage->load(e->uid())) {
                KCalendarCore::Incidence::Ptr orphan = mCalendar->incidence(e->uid(), QDateTime());
                if (!orphan.isNull()) {
                    bool deletedOrphanOccurrences = mCalendar->deleteIncidenceInstances(orphan);
                    bool deletedOrphanSeries = mCalendar->deleteIncidence(orphan);
                    if (deletedOrphanOccurrences || deletedOrphanSeries) {
                        qWarning() << "Deleted orphan calendar event:" << orphan->uid()
                                   << orphan->summary() << orphan->description() << orphan->location();
                        orphansDeleted = true;
                    } else {
                        qWarning() << "Failed to delete orphan calendar event:" << orphan->uid()
                                   << orphan->summary() << orphan->description() << orphan->location();
                    }
                }
            }
            continue;
        }

        if (!mSentEvents.contains(e->uid(), e->recurrenceId())) {
            CalendarData::Event event = createEventStruct(e, notebook);
            mSentEvents.insert(event.uniqueId, event.recurrenceId);
            events.insert(event.uniqueId, event);
            const QString id = e->instanceIdentifier();
            if (id != event.uniqueId) {
                // Ensures that events can also be retrieved by instanceIdentifier
                events.insert(id, event);
            }
        }
    }

    if (orphansDeleted) {
        save(); // save the orphan deletions to storage.
    }

    QHash<QString, CalendarData::EventOccurrence> occurrences = eventOccurrences(ranges);
    QHash<QDate, QStringList> dailyOccurrences = dailyEventOccurrences(ranges, occurrences.values());

    emit dataLoaded(ranges, instanceList, events, occurrences, dailyOccurrences, reset);
}

CalendarData::Event CalendarWorker::createEventStruct(const KCalendarCore::Event::Ptr &e,
                                                      mKCal::Notebook::Ptr notebook) const
{
    CalendarData::Event event(*e);
    event.calendarUid = mCalendar->notebook(e);
    event.readOnly = mStorage->notebook(event.calendarUid)->isReadOnly();
    bool externalInvitation = false;
    const QString &calendarOwnerEmail = getNotebookAddress(e);

    KCalendarCore::Person organizer = e->organizer();
    const QString organizerEmail = organizer.email();
    if (!organizerEmail.isEmpty() && organizerEmail != calendarOwnerEmail
            && (notebook.isNull() || !notebook->sharedWith().contains(organizerEmail))) {
        externalInvitation = true;
    }
    event.externalInvitation = externalInvitation;

    // It would be good to set the attendance status directly in the event within the plugin,
    // however in some cases the account email and owner attendee email won't necessarily match
    // (e.g. in the case where server-side aliases are defined but unknown to the plugin).
    // So we handle this here to avoid "missing" some status changes due to owner email mismatch.
    // This defaults to QString() -> ResponseUnspecified in case the property is undefined
    event.ownerStatus = CalendarUtils::convertResponseType(e->nonKDECustomProperty("X-EAS-RESPONSE-TYPE"));

    const KCalendarCore::Attendee::List attendees = e->attendees();
    for (const KCalendarCore::Attendee &calAttendee : attendees) {
        if (calAttendee.email() == calendarOwnerEmail) {
            if (CalendarUtils::convertPartStat(calAttendee.status()) != CalendarEvent::ResponseUnspecified) {
                // Override the ResponseType
                event.ownerStatus = CalendarUtils::convertPartStat(calAttendee.status());
            }
            //TODO: KCalendarCore::Attendee::RSVP() returns false even if response was requested for some accounts like Google.
            // We can use attendee role until the problem is not fixed (probably in Google plugin).
            // To be updated later when google account support for responses is added.
            event.rsvp = calAttendee.RSVP();// || calAttendee->role() != KCalendarCore::Attendee::Chair;
        }
    }
    return event;
}

static bool serviceIsEnabled(Accounts::Account *account, const QString &syncProfile)
{
    account->selectService();
    if (account->enabled()) {
        for (const Accounts::Service &service : account->services()) {
            account->selectService(service);
            const QStringList allKeys = account->allKeys();
            for (const QString &key : allKeys) {
                if (key.endsWith(QLatin1String("/profile_id"))
                    && account->valueAsString(key) == syncProfile) {
                    bool ret = account->enabled();
                    account->selectService();
                    return ret;
                }
            }
        }
        account->selectService();
        return true;
    }
    return false;
}

void CalendarWorker::loadNotebooks()
{
    QStringList defaultNotebookColors = QStringList() << "#00aeef" << "red" << "blue" << "green" << "pink" << "yellow";
    int nextDefaultNotebookColor = 0;

    const mKCal::Notebook::List notebooks = mStorage->notebooks();
    QSettings settings("nemo", "nemo-qml-plugin-calendar");

    QHash<QString, CalendarData::Notebook> newNotebooks;

    bool changed = mNotebooks.isEmpty();
    for (int ii = 0; ii < notebooks.count(); ++ii) {
        mKCal::Notebook::Ptr mkNotebook = notebooks.at(ii);
        CalendarData::Notebook notebook = mNotebooks.value(mkNotebook->uid(), CalendarData::Notebook());

        notebook.name = mkNotebook->name();
        notebook.uid = mkNotebook->uid();
        notebook.description = mkNotebook->description();
        notebook.emailAddress = mKCal::ServiceHandler::instance().emailAddress(mkNotebook, mStorage);
        notebook.isDefault = mkNotebook->isDefault();
        notebook.readOnly = mkNotebook->isReadOnly();
        notebook.localCalendar = mkNotebook->isMaster()
                && !mkNotebook->isShared()
                && mkNotebook->pluginName().isEmpty();

        notebook.excluded = !mkNotebook->isVisible();
        // To keep backward compatibility:
        if (settings.value("exclude/" + notebook.uid, false).toBool()) {
            mkNotebook->setIsVisible(false);
            if (notebook.excluded || mStorage->updateNotebook(mkNotebook)) {
                settings.remove("exclude/" + notebook.uid);
            }
            notebook.excluded = true;
        }

        const QString &confColor = settings.value("colors/" + notebook.uid, QString()).toString();
        const QString &notebookColor = confColor.isEmpty() ? mkNotebook->color() : confColor;
        const bool confHasColor = !confColor.isEmpty();
        notebook.color = notebookColor.isEmpty()
                       ? defaultNotebookColors.at((nextDefaultNotebookColor++) % defaultNotebookColors.count())
                       : notebookColor;
        bool canRemoveConf = true;
        if (notebook.color != mkNotebook->color()) {
            mkNotebook->setColor(notebook.color);
            canRemoveConf = mStorage->updateNotebook(mkNotebook);
        }
        if (confHasColor && canRemoveConf) {
            settings.remove("colors/" + notebook.uid);
        }

        QString accountStr = mkNotebook->account();
        if (!accountStr.isEmpty()) {
            if (!mAccountManager) {
                mAccountManager = new Accounts::Manager(this);
            }
            bool ok = false;
            int accountId = accountStr.toInt(&ok);
            if (ok && accountId > 0) {
                Accounts::Account *account = Accounts::Account::fromId(mAccountManager, accountId, this);
                if (account) {
                    if (!serviceIsEnabled(account, mkNotebook->syncProfile())) {
                        continue;
                    }
                    notebook.accountId = accountId;
                    notebook.accountIcon = mAccountManager->provider(account->providerName()).iconName();
                    if (notebook.description.isEmpty()) {
                        // fill the description field with some account information
                        notebook.description = account->displayName();
                    }
                }
                delete account;
            }
        }

        if (mNotebooks.contains(notebook.uid) && mNotebooks.value(notebook.uid) != notebook)
            changed = true;

        newNotebooks.insert(notebook.uid, notebook);
    }

    if (changed || mNotebooks.count() != newNotebooks.count()) {
        mNotebooks = newNotebooks;
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}


CalendarData::EventOccurrence CalendarWorker::getNextOccurrence(const QString &uid,
                                                                const QDateTime &recurrenceId,
                                                                const QDateTime &start) const
{
    KCalendarCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);
    if (!event) {
        qWarning() << "Failed to get next occurrence, event not found. UID = " << uid << recurrenceId;
        return CalendarData::EventOccurrence();
    }
    if (event->recurs() && !mStorage->loadSeries(uid)) {
        qWarning() << "Failed to load series of event. UID = " << uid << recurrenceId;
        return CalendarData::EventOccurrence();
    }
    return CalendarUtils::getNextOccurrence(event, start, event->recurs() ? mCalendar->instances(event) : KCalendarCore::Incidence::List());
}

QList<CalendarData::Attendee> CalendarWorker::getEventAttendees(const QString &uid, const QDateTime &recurrenceId)
{
    QList<CalendarData::Attendee> result;

    KCalendarCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);

    if (event.isNull()) {
        return result;
    }

    return CalendarUtils::getEventAttendees(event);
}

void CalendarWorker::findMatchingEvent(const QString &invitationFile)
{
    KCalendarCore::MemoryCalendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    CalendarUtils::importFromFile(invitationFile, cal);
    KCalendarCore::Incidence::List incidenceList = cal->incidences();
    for (int i = 0; i < incidenceList.size(); i++) {
        KCalendarCore::Incidence::Ptr incidence = incidenceList.at(i);
        if (incidence->type() == KCalendarCore::IncidenceBase::TypeEvent) {
            // Search for this event in the database.
            loadData(QList<CalendarData::Range>() << qMakePair(incidence->dtStart().date().addDays(-1), incidence->dtStart().date().addDays(1)), QStringList(), false);
            KCalendarCore::Incidence::List dbIncidences = mCalendar->incidences();
            Q_FOREACH (KCalendarCore::Incidence::Ptr dbIncidence, dbIncidences) {
                const QString remoteUidValue(dbIncidence->nonKDECustomProperty("X-SAILFISHOS-REMOTE-UID"));
                if (dbIncidence->uid().compare(incidence->uid(), Qt::CaseInsensitive) == 0 ||
                        remoteUidValue.compare(incidence->uid(), Qt::CaseInsensitive) == 0) {
                    if ((!incidence->hasRecurrenceId() && !dbIncidence->hasRecurrenceId())
                            || (incidence->hasRecurrenceId() && dbIncidence->hasRecurrenceId()
                                && incidence->recurrenceId() == dbIncidence->recurrenceId())) {
                        emit findMatchingEventFinished(invitationFile, createEventStruct(dbIncidence.staticCast<KCalendarCore::Event>()));
                        return;
                    }
                }
            }
            break; // we only attempt to find the very first event, the invitation should only contain one.
        }
    }

    // not found.
    emit findMatchingEventFinished(invitationFile, CalendarData::Event());
}
