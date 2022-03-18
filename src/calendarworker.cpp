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
    void updateAttendee(const KCalendarCore::Incidence::Ptr &event,
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
    if (mStorage.data())
        mStorage->close();

    mCalendar.clear();
    mStorage.clear();
}

void CalendarWorker::storageModified(mKCal::ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);

    // External touch of the database. We have no clue what changed.
    // The mCalendar content has been wiped out already.
    loadNotebooks();
    emit storageModifiedSignal();
}

void CalendarWorker::storageUpdated(mKCal::ExtendedStorage *storage,
                                    const KCalendarCore::Incidence::List &added,
                                    const KCalendarCore::Incidence::List &modified,
                                    const KCalendarCore::Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);

    // No smart handling of known updates at the moment.
    emit storageModifiedSignal();
}

void CalendarWorker::deleteEvent(const QString &uid, const QDateTime &recurrenceId, const QDateTime &dateTime)
{
    KCalendarCore::Incidence::Ptr event = mCalendar->incidence(uid, recurrenceId);
    if (!event && mStorage->load(uid, recurrenceId)) {
        event = mCalendar->incidence(uid, recurrenceId);
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
        mCalendar->deleteIncidence(event);
        mDeletedEvents.append(QPair<QString, QDateTime>(uid, recurrenceId));
    }
}

void CalendarWorker::deleteAll(const QString &uid)
{
    KCalendarCore::Incidence::Ptr event = mCalendar->incidence(uid);
    if (!event && mStorage->loadSeries(uid)) {
        event = mCalendar->incidence(uid);
    }
    if (!event) {
        qDebug() << uid << "event already deleted from DB";
        return;
    }

    mCalendar->deleteIncidence(event);
    mDeletedEvents.append(QPair<QString, QDateTime>(uid, QDateTime()));
}

bool CalendarWorker::sendResponse(const QString &uid, const QDateTime &recurrenceId,
                                  const CalendarEvent::Response response)
{
    KCalendarCore::Incidence::Ptr event = mCalendar->incidence(uid, recurrenceId);
    if (!event) {
        qWarning() << "Failed to send response, event not found. UID = " << uid;
        return false;
    }
    const QString ownerEmail = mNotebooks.value(mCalendar->notebook(event)).emailAddress;
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

    bool sent = mKCal::ServiceHandler::instance().sendResponse(event, event->description(), mCalendar, mStorage);

    if (!sent)
        updateAttendee(event, updated, origAttendee);

    return sent;
}

QString CalendarWorker::convertEventToICalendar(const QString &uid, const QString &prodId) const
{
    // NOTE: not fetching eventInstances() with different recurrenceId
    KCalendarCore::Incidence::Ptr event = mCalendar->event(uid);
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

void CalendarWorker::saveEvent(const KCalendarCore::Incidence::Ptr &incidence,
                               const QString &notebookUid)
{
    if (!incidence) {
        return;
    }
    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid)) {
        qWarning() << "Invalid notebook uid:" << notebookUid;
        return;
    }

    KCalendarCore::Incidence::Ptr event = mCalendar->incidence(incidence->uid(),
                                                               incidence->recurrenceId());
    if (!event) {
        // For exchange it is better to use upper case UIDs, because for some reason when
        // UID is generated out of Global object id of the email message we are getting a lowercase
        // UIDs, but original UIDs for invitations/events sent from Outlook Web interface are in
        // upper case. To workaround such behaviour it is easier for us to generate an upper case UIDs
        // for new events than trying to implement some complex logic in basesailfish-eas.
        incidence->setUid(incidence->uid().toUpper());
        if (!mCalendar->addIncidence(incidence, notebookUid.isEmpty() ? mCalendar->defaultNotebook() : notebookUid)) {
            qWarning() << "Cannot add incidence" << incidence->uid() << ", notebookUid:" << notebookUid;
            return;
        }
        if (!incidence->attendees().isEmpty())
            mKCal::ServiceHandler::instance().sendInvitation(incidence, QString(), mCalendar, mStorage, mStorage->notebook(mCalendar->notebook(incidence)));
    } else {
        if (!notebookUid.isEmpty() && mCalendar->notebook(incidence) != notebookUid) {
            // mkcal does funny things when moving event between notebooks, work around by changing uid
            KCalendarCore::Incidence::Ptr newEvent(incidence->clone());
            newEvent->setUid(KCalendarCore::CalFormat::createUniqueId().toUpper());
            emit eventNotebookChanged(incidence->uid(), newEvent->uid(), notebookUid);
            mCalendar->deleteIncidence(event);
            mCalendar->addIncidence(newEvent, notebookUid);
        } else {
            incidence->setRevision(event->revision() + 1);
            if (!event->attendees().isEmpty()) {
                KCalendarCore::Attendee::List cancelAttendees;
                const KCalendarCore::Attendee::List oldAttendees = event->attendees();
                for (const KCalendarCore::Attendee &attendee : oldAttendees) {
                    if (incidence->attendeeByMail(attendee.email()).isNull()
                        && (attendee.role() == KCalendarCore::Attendee::ReqParticipant
                            || attendee.role() == KCalendarCore::Attendee::OptParticipant)) {
                        cancelAttendees.append(attendee);
                    }
                }
                if (!cancelAttendees.isEmpty()) {
                    KCalendarCore::Incidence::Ptr cancelled(incidence->clone());
                    cancelled->setAttendees(cancelAttendees);
                    cancelled->setStatus(KCalendarCore::Incidence::StatusCanceled);
                    mKCal::ServiceHandler::instance().sendUpdate(cancelled, QString(), mCalendar, mStorage, mStorage->notebook(mCalendar->notebook(event)));
                }
            }
            event->startUpdates();
            *event.staticCast<KCalendarCore::IncidenceBase>() = *incidence.staticCast<KCalendarCore::IncidenceBase>();
            event->endUpdates();
            if (!event->attendees().isEmpty())
                mKCal::ServiceHandler::instance().sendUpdate(event, QString(), mCalendar, mStorage, mStorage->notebook(mCalendar->notebook(event)));
        }
    }

    save();
}

KCalendarCore::Incidence::Ptr CalendarWorker::dissociateSingleOccurrence(const QString &uid, const QDateTime &recurrenceId)
{
    KCalendarCore::Incidence::Ptr event = mCalendar->incidence(uid);
    if (!event) {
        qWarning("Event to create occurrence replacement for not found");
        return KCalendarCore::Incidence::Ptr();
    }

    // Note: for all day events, to guarantee that exception set in a given time
    // zone is also an exception when travelling to another time, we use the
    // LocalTime spec.
    const QDateTime occurrence = event->allDay()
            ? QDateTime(recurrenceId.date(), recurrenceId.time(), Qt::LocalTime)
            : recurrenceId;
    return mCalendar->dissociateSingleOccurrence(event, occurrence);
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
    if (calOrganizer.email() != mNotebooks.value(mCalendar->notebook(event)).emailAddress) {
        return false;
    }
    return true;
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

    if (!mStorage->setDefaultNotebook(mStorage->notebook(notebookUid))) {
        qWarning() << "unable to set default notebook";
    }
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
    for (const CalendarData::Range &range : ranges) {
        mStorage->load(range.first, range.second.addDays(1)); // end date is not inclusive
    }

    foreach (const QString &id, instanceList) {
        if (mCalendar->instance(id).isNull()) {
            mStorage->loadIncidenceInstance(id);
        }
    }

    if (reset)
        mSentEvents.clear();

    QMultiHash<QString, CalendarData::Incidence> events;
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

        const QString id = e->instanceIdentifier();
        if (!mSentEvents.contains(id)) {
            mSentEvents.insert(id);
            const CalendarData::Incidence data{e, mCalendar->notebook(e)};
            events.insert(e->uid(), data);
            if (id != e->uid()) {
                // Ensures that events can also be retrieved by instanceIdentifier
                events.insert(id, data);
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
        notebook.sharedWith = mkNotebook->sharedWith();

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
    KCalendarCore::Incidence::Ptr event = mCalendar->incidence(uid, recurrenceId);
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
                        emit findMatchingEventFinished(invitationFile, {dbIncidence, mCalendar->notebook(dbIncidence)});
                        return;
                    }
                }
            }
            break; // we only attempt to find the very first event, the invitation should only contain one.
        }
    }

    // not found.
    emit findMatchingEventFinished(invitationFile, {});
}
