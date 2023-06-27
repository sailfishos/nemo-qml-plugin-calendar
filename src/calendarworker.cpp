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

// To get notified about timezone changes
#include <timed-qt5/interface>

namespace {
    void updateAttendee(KCalendarCore::Incidence::Ptr event,
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
    : QObject(0), mAccountManager(0)
{
}

CalendarWorker::~CalendarWorker()
{
}

void CalendarWorker::storageModified(mKCal::MultiCalendarStorage *storage)
{
    Q_UNUSED(storage);

    // External touch of the database. We have no clue what changed.
    // The mCalendar content has been wiped out already.
    loadNotebooks();
    emit storageModifiedSignal();
}

void CalendarWorker::storageUpdated(mKCal::MultiCalendarStorage *storage,
                                    const QString &notebookUid,
                                    const KCalendarCore::Incidence::List &added,
                                    const KCalendarCore::Incidence::List &modified,
                                    const KCalendarCore::Incidence::List &deleted)
{
    mKCal::Notebook::Ptr notebook = storage->notebook(notebookUid);
    if (!notebook) {
        qWarning() << "Failed to load notebook" << notebookUid;
        return;
    }

    // The separation between sendInvitation and sendUpdate it not really good,
    // when modifying an existing event and adding attendees, should it be which?
    // Probably those should be combined into a single function on the API, but
    // until that is done, let's just handle new events as invitations and rest as updates.
    for (const KCalendarCore::Incidence::Ptr &event: added) {
        if (event->attendeeCount() > 0) {
            mKCal::ServiceHandler::instance().sendInvitation(notebook, event, QString());
        }
    }
    for (const KCalendarCore::Incidence::Ptr &event: modified) {
        if (event->attendeeCount() > 0 && isOrganizer(notebookUid, event)) {
            mKCal::ServiceHandler::instance().sendUpdate(notebook, event, QString());
        }
    }
    for (const KCalendarCore::Incidence::Ptr &event: deleted) {
        // FIXME: should send response update if deleting an event we have responded to.
        if (event->attendeeCount() > 0 && isOrganizer(notebookUid, event)) {
            event->setStatus(KCalendarCore::Incidence::StatusCanceled);
            mKCal::ServiceHandler::instance().sendUpdate(notebook, event, QString());
        }
    }
}

KCalendarCore::Incidence::Ptr CalendarWorker::getInstance(const QString &instanceId) const
{
    KCalendarCore::Incidence::Ptr event = mStorage->instance(instanceId);
    if (!event && mStorage->loadIncidenceInstance(instanceId)) {
        event = mStorage->instance(instanceId);
    }
    return event;
}

void CalendarWorker::deleteEvent(const QString &instanceId, const QDateTime &dateTime)
{
    KCalendarCore::Incidence::Ptr event = getInstance(instanceId);
    if (!event) {
        qDebug() << instanceId << "event already deleted from DB";
        return;
    }
    KCalendarCore::MemoryCalendar::Ptr calendar = mStorage->calendarOfInstance(instanceId);

    if (event->recurs() && dateTime.isValid()) {
        // We're deleting an occurrence from a recurring event.
        // No incidence is deleted from the database in that case,
        // only the base incidence is modified by adding an exDate.
        if (dateTime.timeSpec() == Qt::LocalTime && event->dtStart().timeSpec() != Qt::LocalTime)
            event->recurrence()->addExDateTime(dateTime.toTimeZone(event->dtStart().timeZone()));
        else
            event->recurrence()->addExDateTime(dateTime);
        event->setRevision(event->revision() + 1);
    } else if (event->hasRecurrenceId()) {
        // We consider that deleting an exception implies to create an exdate for the parent.
        KCalendarCore::Incidence::Ptr parent = calendar->incidence(event->uid());
        if (parent) {
            parent->recurrence()->addExDateTime(event->recurrenceId());
            parent->setRevision(parent->revision() + 1);
        }
        calendar->deleteIncidence(event);
    } else {
        calendar->deleteIncidence(event);
    }
}

void CalendarWorker::deleteAll(const QString &instanceId)
{
    KCalendarCore::Incidence::Ptr event = getInstance(instanceId);
    if (!event) {
        qDebug() << instanceId << "event already deleted from DB";
        return;
    }
    KCalendarCore::MemoryCalendar::Ptr calendar = mStorage->calendarOfInstance(instanceId);
    if (event->hasRecurrenceId()) {
        KCalendarCore::Incidence::Ptr parent = calendar->incidence(event->uid());
        if (parent)
            event = parent;
    }
    calendar->deleteIncidence(event);
}

bool CalendarWorker::sendResponse(const QString &instanceId,
                                  const CalendarEvent::Response response)
{
    KCalendarCore::Incidence::Ptr event = mStorage->instance(instanceId);
    if (!event) {
        qWarning() << "Failed to send response, event not found. UID = " << instanceId;
        return false;
    }
    mKCal::Notebook::Ptr notebook = mStorage->notebookOfInstance(instanceId);
    const QString ownerEmail = getNotebookAddress(notebook->uid());
    const KCalendarCore::Attendee origAttendee = event->attendeeByMail(ownerEmail);
    KCalendarCore::Attendee updated = origAttendee;
    switch (response) {
    case CalendarEvent::ResponseAccept:
        updated.setStatus(KCalendarCore::Attendee::Accepted);
        break;
    case CalendarEvent::ResponseTentative:
        updated.setStatus(KCalendarCore::Attendee::Tentative);
        break;
    case CalendarEvent::ResponseDecline:
        updated.setStatus(KCalendarCore::Attendee::Declined);
        break;
    default:
        updated.setStatus(KCalendarCore::Attendee::NeedsAction);
    }
    updateAttendee(event, origAttendee, updated);

    bool sent = mKCal::ServiceHandler::instance().sendResponse(notebook, event, event->description());
    if (!sent)
        updateAttendee(event, updated, origAttendee);

    return sent;
}

QString CalendarWorker::convertEventToICalendar(const QString &instanceId, const QString &prodId) const
{
    // NOTE: exporting only the matching occurrence with instanceId,
    // for recurring parent, it will not append the exceptions,
    // for exceptions, it will not append the parent.
    KCalendarCore::Incidence::Ptr event = mStorage->instance(instanceId);
    if (event.isNull()) {
        qWarning() << "No event with uid " << instanceId << ", unable to create iCalendar";
        return QString();
    }

    KCalendarCore::ICalFormat fmt;
    fmt.setApplication(fmt.application(),
                       prodId.isEmpty() ? QLatin1String("-//sailfishos.org/Sailfish//NONSGML v1.0//EN") : prodId);
    return fmt.toICalString(event);
}

void CalendarWorker::save()
{
    mStorage->save(QString(), mKCal::MultiCalendarStorage::PurgeOnLocal);

    // No smart handling of known updates at the moment.
    emit storageModifiedSignal();
}

void CalendarWorker::saveEvent(const CalendarData::Event &eventData, bool updateAttendees,
                               const QList<CalendarData::EmailContact> &required,
                               const QList<CalendarData::EmailContact> &optional)
{
    QString notebookUid = eventData.calendarUid;
    if (notebookUid.isEmpty()) {
        qWarning() << "Empty notebook uid";
        return;
    }

    KCalendarCore::Event::Ptr event;
    if (!eventData.instanceId.isEmpty()) {
        event = mStorage->instance(eventData.instanceId).staticCast<KCalendarCore::Event>();
        if (!event && eventData.recurrenceId.isNull()) {
            // possibility that event was removed while changes were edited. options to either skip, as done now,
            // or resurrect the event
            qWarning("Event to be saved not found");
            return;
        }
    }
    bool createNew = event.isNull();

    if (createNew) {
        event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);

        // For exchange it is better to use upper case UIDs, because for some reason when
        // UID is generated out of Global object id of the email message we are getting a lowercase
        // UIDs, but original UIDs for invitations/events sent from Outlook Web interface are in
        // upper case. To workaround such behaviour it is easier for us to generate an upper case UIDs
        // for new events than trying to implement some complex logic in basesailfish-eas.
        if (eventData.instanceId.isEmpty()) {
            event->setUid(event->uid().toUpper());
        } else {
            event->setUid(eventData.incidenceUid);
            event->setRecurrenceId(eventData.recurrenceId);
        }
    } else {
        mKCal::Notebook::Ptr notebook = mStorage->notebookOfInstance(eventData.instanceId);
        if (notebook->uid() != notebookUid) {
            KCalendarCore::MemoryCalendar::Ptr origCalendar = mStorage->calendarOfInstance(eventData.instanceId);
            origCalendar->deleteEvent(event);
            createNew = true;
        } else {
            event->setRevision(event->revision() + 1);
        }
    }

    eventData.toKCalendarCore(event);

    if (updateAttendees) {
        updateEventAttendees(event, createNew, required, optional, notebookUid);
    }

    if (createNew) {
        KCalendarCore::MemoryCalendar::Ptr calendar = mStorage->calendar(notebookUid);
        if (!calendar) {
            qWarning() << "No such notebook" << notebookUid;
            return;
        }
        if (!calendar->addEvent(event)) {
            qWarning() << "Cannot add event" << event->uid() << ", notebookUid:" << notebookUid;
            return;
        }
        const QString instanceId = mKCal::MultiCalendarStorage::multiCalendarIdentifier(notebookUid, *event);
        if (instanceId != eventData.instanceId) {
            emit eventNotebookChanged(eventData.instanceId, instanceId, notebookUid);
        }
    }

    save();
}

CalendarData::Event CalendarWorker::dissociateSingleOccurrence(const QString &instanceId, const QDateTime &datetime)
{
    KCalendarCore::Incidence::Ptr event = mStorage->instance(instanceId);
    if (!event || event->hasRecurrenceId()) {
        qWarning("Event to create occurrence replacement for not found or already an exception");
        return CalendarData::Event();
    }

    // Don't save milliseconds
    QDateTime recId(datetime);
    recId.setTime(QTime(recId.time().hour(),
                        recId.time().minute(),
                        recId.time().second()));
    // Note: for all day events, to guarantee that exception set in a given time
    // zone is also an exception when travelling to another time, we use the
    // LocalTime spec.
    const QDateTime occurrence = event->allDay()
            ? QDateTime(recId.date(), recId.time(), Qt::LocalTime)
            : recId;
    if (!event->allDay() && !event->recursAt(occurrence)) {
        qWarning() << "Event don't recurs at" << occurrence;
        return CalendarData::Event();
    } else if (event->allDay() && !event->recursOn(occurrence.date(), occurrence.timeZone())) {
        qWarning() << "Event don't recurs on" << occurrence.date();
        return CalendarData::Event();
    }
    KCalendarCore::MemoryCalendar::Ptr calendar = mStorage->calendarOfInstance(instanceId);
    const KCalendarCore::Incidence::List exceptions = calendar->instances(event);
    for (const KCalendarCore::Incidence::Ptr &exception : exceptions) {
        if (exception->recurrenceId() == occurrence) {
            qWarning() << "Exception already exists, cannot dissociate.";
            return CalendarData::Event();
        }
    }
    KCalendarCore::Incidence::Ptr replacement = calendar->createException(event, occurrence);
    if (!replacement) {
        qWarning() << "Unable to create the replacing occurrence";
        return CalendarData::Event();
    }
    replacement->setSchedulingID(QString());
    event->setLastModified(replacement->created());

    mKCal::Notebook::Ptr notebook = mStorage->notebookOfInstance(instanceId);
    return createEventStruct(replacement.staticCast<KCalendarCore::Event>(), *notebook);
}

void CalendarWorker::init()
{
    mStorage = mKCal::MultiCalendarStorage::systemStorage();
    if (!mStorage->open()) {
        qWarning() << "Unable to open system storage.";
        return;
    }
    mStorage->registerObserver(this);
    loadNotebooks();

    Maemo::Timed::Interface *timed = new Maemo::Timed::Interface(this);
    if (!timed->settings_changed_connect(this, SLOT(onTimedSignal(const Maemo::Timed::WallClock::Info &, bool)))) {
        qWarning() << "Connection to timed signal failed:" << Maemo::Timed::bus().lastError().message();
    }
}

void CalendarWorker::onTimedSignal(const Maemo::Timed::WallClock::Info &info,
                                   bool time_changed)
{
    Q_UNUSED(time_changed);

    QTimeZone newTimezone(info.humanReadableTz().toUtf8());
    if (newTimezone.isValid() && mStorage->setTimeZone(newTimezone)) {
        emit storageModifiedSignal();
        emit calendarTimezoneChanged();
    }
}

bool CalendarWorker::isOrganizer(const QString &notebookUid,
                                 const KCalendarCore::Incidence::Ptr &event) const
{
    if (event) {
        const KCalendarCore::Person calOrganizer = event->organizer();
        return (!calOrganizer.isEmpty() && calOrganizer.email() == getNotebookAddress(notebookUid));
    } else {
        qWarning() << Q_FUNC_INFO << "event is NULL";
        return false;
    }
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

    event->startUpdates();
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
            mKCal::ServiceHandler::instance().sendUpdate(notebook, cancelEvent, QString());
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
    }
    event->endUpdates();
}

QString CalendarWorker::getNotebookAddress(const QString &notebookUid) const
{
    return mNotebooks.value(notebookUid).emailAddress;
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

    if (!mStorage->setDefaultNotebook(notebookUid)) {
        qWarning() << "unable to set default notebook";
    }

    loadNotebooks();
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
        save();
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
            save();
        }

        CalendarData::Notebook notebook = mNotebooks.value(notebookUid);
        notebook.color = color;
        mNotebooks.insert(notebook.uid, notebook);

        emit notebooksChanged(mNotebooks.values());
    }
}

void CalendarWorker::eventOccurrences(QHash<QString, CalendarData::EventOccurrence> *occurrences,
                                      const QList<CalendarData::Range> &ranges,
                                      const KCalendarCore::Calendar &calendar) const
{
    for (const CalendarData::Range range : ranges) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        KCalendarCore::OccurrenceIterator it(calendar, range.first.addDays(-1).startOfDay(),
                                             range.second.endOfDay());
#else
        KCalendarCore::OccurrenceIterator it(calendar, QDateTime(range.first.addDays(-1)),
                                             QDateTime(range.second.addDays(1)).addSecs(-1));
#endif
        while (it.hasNext()) {
            it.next();
            if (it.incidence()->type() == KCalendarCore::IncidenceBase::TypeEvent) {
                const QDateTime sdt = it.occurrenceStartDate();
                const KCalendarCore::Duration elapsed
                    (it.incidence()->dateTime(KCalendarCore::Incidence::RoleDisplayStart),
                     it.incidence()->dateTime(KCalendarCore::Incidence::RoleDisplayEnd),
                     KCalendarCore::Duration::Seconds);
                CalendarData::EventOccurrence occurrence;
                occurrence.instanceId = it.incidence()->instanceIdentifier();
                occurrence.startTime = sdt;
                occurrence.endTime = elapsed.end(sdt);
                occurrence.eventAllDay = it.incidence()->allDay();
                occurrences->insert(occurrence.getId(), occurrence);
            }
        }
    }
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
    for (const CalendarData::Range &range : ranges) {
        mStorage->load(range.first, range.second.addDays(1)); // end date is not inclusive
    }

    foreach (const QString &id, instanceList) {
        mStorage->loadIncidenceInstance(id);
    }

    if (reset)
        mSentEvents.clear();

    QHash<QString, CalendarData::Event> events;
    QHash<QString, CalendarData::EventOccurrence> occurrences;
    for (const mKCal::Notebook::Ptr &notebook : mStorage->notebooks()) {
        if (!notebook->isVisible() || !notebook->eventsAllowed()) {
            continue;
        }
        KCalendarCore::MemoryCalendar::Ptr calendar = mStorage->calendar(notebook->uid());
        for (const KCalendarCore::Event::Ptr &event : calendar->rawEvents()) {
            const QString id = mStorage->multiCalendarIdentifier(notebook->uid(), *event);
            if (!mSentEvents.contains(id)) {
                mSentEvents.insert(id);
                events.insert(id, createEventStruct(event, *notebook));
            }
        }
        eventOccurrences(&occurrences, ranges, *calendar);
    }
    QHash<QDate, QStringList> dailyOccurrences = dailyEventOccurrences(ranges, occurrences.values());

    emit dataLoaded(ranges, instanceList, events, occurrences, dailyOccurrences, reset);
}

CalendarData::Event CalendarWorker::createEventStruct(const KCalendarCore::Event::Ptr &e,
                                                      const mKCal::Notebook &notebook) const
{
    CalendarData::Event event(*e);
    event.instanceId = mKCal::MultiCalendarStorage::multiCalendarIdentifier(notebook.uid(), *e);
    event.calendarUid = notebook.uid();
    event.readOnly = notebook.isReadOnly();

    const QString &calendarOwnerEmail = getNotebookAddress(event.calendarUid);

    const QString organizerEmail = e->organizer().email();
    event.externalInvitation = (!organizerEmail.isEmpty()
                                && organizerEmail != calendarOwnerEmail
                                && !notebook.sharedWith().contains(organizerEmail));

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

void CalendarWorker::search(const QString &searchString, int limit)
{
    QStringList identifiers;
    QHash<QString, CalendarData::Event> events;

    if (mStorage->search(searchString, &identifiers, limit)) {
        emit searchResults(searchString, identifiers);
    }

    for (int i = 0; i < identifiers.length(); i++) {
        if (!mSentEvents.contains(identifiers[i])) {
            KCalendarCore::Incidence::Ptr incidence = mStorage->instance(identifiers[i]);
            mKCal::Notebook::Ptr notebook = mStorage->notebookOfInstance(identifiers[i]);
            if (incidence->type() == KCalendarCore::IncidenceBase::TypeEvent
                && notebook->isVisible()) {
                mSentEvents.insert(identifiers[i]);
                events.insert(identifiers[i], createEventStruct(incidence.staticCast<KCalendarCore::Event>(), *notebook));
            }
        }
    }
    if (!events.isEmpty()) {
        emit dataLoaded(QList<CalendarData::Range>(), identifiers, events,
                        QHash<QString, CalendarData::EventOccurrence>(),
                        QHash<QDate, QStringList>(), false);
    }
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

    QSettings settings("nemo", "nemo-qml-plugin-calendar");

    QHash<QString, CalendarData::Notebook> newNotebooks;

    bool changed = false;
    for (mKCal::Notebook::Ptr mkNotebook : mStorage->notebooks()) {
        if (!mkNotebook->eventsAllowed()) {
            continue;
        }
        CalendarData::Notebook notebook = mNotebooks.value(mkNotebook->uid(), CalendarData::Notebook());

        notebook.name = mkNotebook->name();
        notebook.uid = mkNotebook->uid();
        notebook.description = mkNotebook->description();
        notebook.emailAddress = mKCal::ServiceHandler::instance().emailAddress(mkNotebook);
        notebook.isDefault = mStorage->defaultNotebook()
                && (mkNotebook->uid() == mStorage->defaultNotebook()->uid());
        notebook.readOnly = mkNotebook->isReadOnly();
        notebook.localCalendar = mkNotebook->isMaster()
                && !mkNotebook->isShared()
                && mkNotebook->pluginName().isEmpty();

        notebook.excluded = !mkNotebook->isVisible();
        // To keep backward compatibility:
        if (settings.value("exclude/" + notebook.uid, false).toBool()) {
            mkNotebook->setIsVisible(false);
            if (notebook.excluded || mStorage->save()) {
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
            canRemoveConf = mStorage->save();
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


CalendarData::EventOccurrence CalendarWorker::getNextOccurrence(const QString &instanceId,
                                                                const QDateTime &start) const
{
    KCalendarCore::Event::Ptr event = getInstance(instanceId).staticCast<KCalendarCore::Event>();
    if (!event) {
        qWarning() << "Failed to get next occurrence, event not found. UID = " << instanceId;
        return CalendarData::EventOccurrence();
    }
    KCalendarCore::Incidence::List instances;
    if (event->recurs()) {
        instances = mStorage->calendarOfInstance(instanceId)->instances(event);
    }
    CalendarData::EventOccurrence occurrence =
        CalendarUtils::getNextOccurrence(event, start, instances);
    occurrence.instanceId = instanceId;
    return occurrence;
}

QList<CalendarData::Attendee> CalendarWorker::getEventAttendees(const QString &instanceId)
{
    QList<CalendarData::Attendee> result;

    KCalendarCore::Incidence::Ptr event = mStorage->instance(instanceId);

    if (event.isNull()) {
        return result;
    }

    return CalendarUtils::getEventAttendees(event);
}

void CalendarWorker::findMatchingEvent(const QString &invitationFile)
{
    KCalendarCore::MemoryCalendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    CalendarUtils::importFromFile(invitationFile, cal);
    for (const KCalendarCore::Event::Ptr &event : cal->events()) {
        // Search for this event in the database.
        const QDate at = event->dtStart().date();
        mStorage->load(at.addDays(-1), at.addDays(2));
        for (const mKCal::Notebook::Ptr &notebook : mStorage->notebooks()) {
            for (const KCalendarCore::Event::Ptr &dbEvent : mStorage->calendar(notebook->uid())->events(at.addDays(-1), at.addDays(2))) {
                const QString remoteUidValue(dbEvent->nonKDECustomProperty("X-SAILFISHOS-REMOTE-UID"));
                if (dbEvent->uid().compare(event->uid(), Qt::CaseInsensitive) == 0 ||
                    remoteUidValue.compare(event->uid(), Qt::CaseInsensitive) == 0) {
                    if ((!event->hasRecurrenceId() && !dbEvent->hasRecurrenceId())
                        || (event->hasRecurrenceId() && dbEvent->hasRecurrenceId()
                            && event->recurrenceId() == dbEvent->recurrenceId())) {
                        emit findMatchingEventFinished(invitationFile, createEventStruct(dbEvent, *notebook));
                        return;
                    }
                }
            }
        }
        break; // we only attempt to find the very first event, the invitation should only contain one.
    }

    // not found.
    emit findMatchingEventFinished(invitationFile, CalendarData::Event());
}
