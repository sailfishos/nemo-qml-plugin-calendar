/*
 * Copyright (c) 2014-2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
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

#include "calendarworker.h"
#include "calendarutils.h"

#include <QDebug>
#include <QSettings>

// mkcal
#include <notebook.h>
#include <servicehandler.h>

// kCalCore
#include <attendee.h>
#include <event.h>
#include <calformat.h>
#include <vcalformat.h>
#include <recurrence.h>
#include <recurrencerule.h>
#include <memorycalendar.h>

#include <libical/vobject.h>
#include <libical/vcaltmp.h>

// libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Provider>
#include <Accounts/Account>

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

void CalendarWorker::deleteEvent(const QString &uid, const KDateTime &recurrenceId, const QDateTime &dateTime)
{
    KCalCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);

    if (!event)
        return;

    if (event->recurs() && dateTime.isValid()) {
        event->recurrence()->addExDateTime(KDateTime(dateTime, KDateTime::Spec(KDateTime::LocalZone)));
    } else {
        mCalendar->deleteEvent(event);
    }
    mExceptionEvents.append(QPair<QString, QDateTime>(uid, dateTime));
}

void CalendarWorker::deleteAll(const QString &uid)
{
    KCalCore::Event::Ptr event = mCalendar->event(uid);
    if (!event) {
        qWarning() << "Failed to delete event, not found" << uid;
        return;
    }

    mCalendar->deleteEventInstances(event);
    mCalendar->deleteEvent(event);
    mDeletedEvents.append(uid);
}

bool CalendarWorker::sendResponse(const CalendarData::Event &eventData, const CalendarEvent::Response response)
{
    KCalCore::Event::Ptr event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);
    if (!event) {
        qWarning() << "Failed to send response, event not found. UID = " << eventData.uniqueId;
        return false;
    }
    const QString &notebookUid = mCalendar->notebook(event);
    const QString &ownerEmail = mNotebooks.contains(notebookUid) ? mNotebooks.value(notebookUid).emailAddress
                                                                 : QString();

    // TODO: should we save this change in DB?
    KCalCore::Attendee::Ptr attender = event->attendeeByMail(ownerEmail);
    attender->setStatus(CalendarUtils::convertResponse(response));
    return mKCal::ServiceHandler::instance().sendResponse(event, eventData.description, mCalendar, mStorage);
}

// eventToVEvent() is protected
class CalendarVCalFormat : public KCalCore::VCalFormat
{
public:
    QString convertEventToVCalendar(const KCalCore::Event::Ptr &event, const QString &prodId)
    {
        VObject *vCalObj = vcsCreateVCal(
                    QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1().data(),
                    NULL,
                    prodId.toLatin1().data(),
                    NULL,
                    (char *) "1.0");
        VObject *vEventObj = eventToVEvent(event);
        addVObjectProp(vCalObj, vEventObj);
        char *memVObject = writeMemVObject(0, 0, vCalObj);
        QString retn = QLatin1String(memVObject);
        free(memVObject);
        cleanVObject(vCalObj);
        return retn;
    }
};

QString CalendarWorker::convertEventToVCalendar(const QString &uid, const QString &prodId) const
{
    // NOTE: not fetching eventInstances() with different recurrenceId for VCalendar.
    KCalCore::Event::Ptr event = mCalendar->event(uid);
    if (event.isNull()) {
        qWarning() << "No event with uid " << uid << ", unable to create VCalendar";
        return QString();
    }

    CalendarVCalFormat fmt;
    return fmt.convertEventToVCalendar(event,
                                       prodId.isEmpty() ? QLatin1String("-//NemoMobile.org/Nemo//NONSGML v1.0//EN")
                                                        : prodId);
}

void CalendarWorker::save()
{
    mStorage->save();
    // FIXME: should send response update if deleting an even we have responded to.
    // FIXME: should send cancel only if we own the event
    if (!mDeletedEvents.isEmpty()) {
        for (const QString &uid: mDeletedEvents) {
            KCalCore::Event::Ptr event = mCalendar->deletedEvent(uid);
            if (!needSendCancellation(event)) {
                continue;
            }
            event->setStatus(KCalCore::Incidence::StatusCanceled);
            mKCal::ServiceHandler::instance().sendUpdate(event, QString(), mCalendar, mStorage);
        }
        mDeletedEvents.clear();
    }
    if (!mExceptionEvents.isEmpty()) {
        for (const QPair<QString, QDateTime> &exceptionEvent: mExceptionEvents) {
            KCalCore::Event::Ptr event = mCalendar->deletedEvent(exceptionEvent.first,
                                                                 KDateTime(exceptionEvent.second, KDateTime::Spec(KDateTime::LocalZone)));
            if (!needSendCancellation(event)) {
                continue;
            }
            event->setStatus(KCalCore::Incidence::StatusCanceled);
            mKCal::ServiceHandler::instance().sendUpdate(event, QString(), mCalendar, mStorage);
        }
        mExceptionEvents.clear();
    }
}

void CalendarWorker::saveEvent(const CalendarData::Event &eventData, bool updateAttendees,
                               const QList<CalendarData::EmailContact> &required,
                               const QList<CalendarData::EmailContact> &optional)
{
    QString notebookUid = eventData.calendarUid;

    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid))
        return;

    KCalCore::Event::Ptr event;
    bool createNew = eventData.uniqueId.isEmpty();

    if (createNew) {
        event = KCalCore::Event::Ptr(new KCalCore::Event);

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
            KCalCore::Event::Ptr newEvent = KCalCore::Event::Ptr(event->clone());
            newEvent->setUid(KCalCore::CalFormat::createUniqueId().toUpper());
            emit eventNotebookChanged(event->uid(), newEvent->uid(), notebookUid);
            mCalendar->deleteEvent(event);
            mCalendar->addEvent(newEvent, notebookUid);
            event = newEvent;
        } else {
            event->setRevision(event->revision() + 1);
        }
    }

    setEventData(event, eventData);

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

void CalendarWorker::setEventData(KCalCore::Event::Ptr &event, const CalendarData::Event &eventData)
{
    event->setDescription(eventData.description);
    event->setSummary(eventData.displayLabel);
    event->setDtStart(eventData.startTime);
    event->setDtEnd(eventData.endTime);
    // setDtStart() overwrites allDay status based on KDateTime::isDateOnly(), avoid by setting that later
    event->setAllDay(eventData.allDay);
    event->setLocation(eventData.location);
    setReminder(event, eventData.reminder);
    setRecurrence(event, eventData.recur);

    if (eventData.recur != CalendarEvent::RecurOnce) {
        event->recurrence()->setEndDate(eventData.recurEndDate);
        if (!eventData.recurEndDate.isValid()) {
            // Recurrence/RecurrenceRule don't have separate method to clear the end date, and currently
            // setting invalid date doesn't make the duration() indicate recurring infinitely.
            event->recurrence()->setDuration(-1);
        }
    }
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

    KCalCore::Event::Ptr event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);
    if (!event) {
        qWarning("Event to create occurrence replacement for not found");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    // Note: occurrence given in local time. Thus if timezone changes between fetching and changing, this won't work
    KDateTime occurrenceTime = KDateTime(startTime, KDateTime::LocalZone);

    KCalCore::Incidence::Ptr replacementIncidence = mCalendar->dissociateSingleOccurrence(event,
                                                                                          occurrenceTime,
                                                                                          KDateTime::LocalZone);
    KCalCore::Event::Ptr replacement = replacementIncidence.staticCast<KCalCore::Event>();
    if (!replacement) {
        qWarning("Didn't find event occurrence to replace");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    setEventData(replacement, eventData);

    if (updateAttendees) {
        updateEventAttendees(replacement, false, required, optional, notebookUid);
    }

    mCalendar->addEvent(replacement, notebookUid);
    emit occurrenceExceptionCreated(eventData, startTime, replacement->recurrenceId());
    save();
}

void CalendarWorker::init()
{
    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mStorage = mCalendar->defaultStorage(mCalendar);
    mStorage->open();
    mStorage->registerObserver(this);
    loadNotebooks();
}

bool CalendarWorker::setRecurrence(KCalCore::Event::Ptr &event, CalendarEvent::Recur recur)
{
    if (!event)
        return false;

    CalendarEvent::Recur oldRecur = CalendarUtils::convertRecurrence(event);

    if (recur == CalendarEvent::RecurCustom) {
        qWarning() << "Cannot assign RecurCustom, will assing RecurOnce";
        recur = CalendarEvent::RecurOnce;
    }

    if (recur == CalendarEvent::RecurOnce)
        event->recurrence()->clear();

    if (oldRecur != recur) {
        switch (recur) {
        case CalendarEvent::RecurOnce:
            break;
        case CalendarEvent::RecurDaily:
            event->recurrence()->setDaily(1);
            break;
        case CalendarEvent::RecurWeekly:
            event->recurrence()->setWeekly(1);
            break;
        case CalendarEvent::RecurBiweekly:
            event->recurrence()->setWeekly(2);
            break;
        case CalendarEvent::RecurMonthly:
            event->recurrence()->setMonthly(1);
            break;
        case CalendarEvent::RecurYearly:
            event->recurrence()->setYearly(1);
            break;
        case CalendarEvent::RecurCustom:
            break;
        }
        return true;
    }

    return false;
}

bool CalendarWorker::setReminder(KCalCore::Event::Ptr &event, int seconds)
{
    if (!event)
        return false;

    if (CalendarUtils::getReminder(event) == seconds)
        return false;

    KCalCore::Alarm::List alarms = event->alarms();
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure)
            continue;
        event->removeAlarm(alarms.at(ii));
    }

    // negative reminder seconds means "no reminder", so only
    // deal with positive (or zero = at time of event) reminders.
    if (seconds >= 0) {
        KCalCore::Alarm::Ptr alarm = event->newAlarm();
        alarm->setEnabled(true);
        // backend stores as "offset to dtStart", i.e negative if reminder before event.
        alarm->setStartOffset(-1 * seconds);
        alarm->setType(KCalCore::Alarm::Display);
    }

    return true;
}

bool CalendarWorker::needSendCancellation(KCalCore::Event::Ptr &event) const
{
    if (!event) {
        qWarning() << Q_FUNC_INFO << "event is NULL";
        return false;
    }
    KCalCore::Attendee::List attendees = event->attendees();
    if (attendees.size() == 0) {
        return false;
    }
    KCalCore::Person::Ptr calOrganizer = event->organizer();
    if (calOrganizer.isNull() || calOrganizer->isEmpty()) {
        return false;
    }
    // we shouldn't send a response if we are not an organizer
    if (calOrganizer->email() != getNotebookAddress(event)) {
        return false;
    }
    return true;
}

// use explicit notebook uid so we don't need to assume the events involved being added there.
// the related notebook is just needed to associate updates to some plugin/account
void CalendarWorker::updateEventAttendees(KCalCore::Event::Ptr event, bool newEvent,
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

    if (!newEvent) {
        // if existing attendees are removed, those should get a cancel update
        KCalCore::Event::Ptr cancelEvent = KCalCore::Event::Ptr(event->clone());

        // first remove everyone still listed as included
        for (int i = 0; i < required.length(); ++i) {
            KCalCore::Attendee::Ptr toRemove = cancelEvent->attendeeByMail(required.at(i).email);
            if (toRemove) {
                cancelEvent->deleteAttendee(toRemove);
            }
        }
        for (int i = 0; i < optional.length(); ++i) {
            KCalCore::Attendee::Ptr toRemove = cancelEvent->attendeeByMail(optional.at(i).email);
            if (toRemove) {
                cancelEvent->deleteAttendee(toRemove);
            }
        }

        QString organizer = cancelEvent->organizer()->email();
        if (!organizer.isEmpty()) {
            KCalCore::Attendee::Ptr toRemove = cancelEvent->attendeeByMail(organizer);
            if (toRemove) {
                cancelEvent->deleteAttendee(toRemove);
            }
        }

        KCalCore::Attendee::List remainingAttendees = cancelEvent->attendees();

        for (int i = remainingAttendees.length() - 1; i >= 0; --i) {
            KCalCore::Attendee::Ptr attendee = remainingAttendees.at(i);

            // if there are non-participants getting update as FYI, or chair for any reason,
            // avoid sending them the cancel
            if (attendee->role() != KCalCore::Attendee::ReqParticipant
                    && attendee->role() != KCalCore::Attendee::OptParticipant) {
                cancelEvent->deleteAttendee(attendee);
                continue;
            }

            // this one really gets cancel so remove from update event side
            KCalCore::Attendee::Ptr toRemove = event->attendeeByMail(attendee->email());
            if (toRemove) {
                event->deleteAttendee(toRemove);
            }
        }
        if (cancelEvent->attendees().length() > 0) {
            cancelEvent->setStatus(KCalCore::Incidence::StatusCanceled);
            mKCal::ServiceHandler::instance().sendUpdate(cancelEvent, QString(), mCalendar, mStorage, notebook);
        }
    }

    if (required.length() > 0 || optional.length() > 0) {
        for (int i = 0; i < required.length(); ++i) {
            KCalCore::Attendee::Ptr existing = event->attendeeByMail(required.at(i).email);
            if (existing) {
                existing->setRole(KCalCore::Attendee::ReqParticipant);
            } else {
                auto attendee = new KCalCore::Attendee(required.at(i).name, required.at(i).email, true /* rsvp */,
                                                       KCalCore::Attendee::NeedsAction,
                                                       KCalCore::Attendee::ReqParticipant);
                event->addAttendee(KCalCore::Attendee::Ptr(attendee));
            }
        }
        for (int i = 0; i < optional.length(); ++i) {
            KCalCore::Attendee::Ptr existing = event->attendeeByMail(optional.at(i).email);
            if (existing) {
                existing->setRole(KCalCore::Attendee::OptParticipant);
            } else {
                auto attendee = new KCalCore::Attendee(optional.at(i).name, optional.at(i).email, true,
                                                       KCalCore::Attendee::NeedsAction,
                                                       KCalCore::Attendee::OptParticipant);
                event->addAttendee(KCalCore::Attendee::Ptr(attendee));
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

QString CalendarWorker::getNotebookAddress(const KCalCore::Event::Ptr &event) const
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
    if (!mNotebooks.contains(notebookUid))
        return false;

    if (mNotebooks.value(notebookUid).excluded == exclude)
        return false;

    CalendarData::Notebook notebook = mNotebooks.value(notebookUid);
    QSettings settings("nemo", "nemo-qml-plugin-calendar");
    notebook.excluded = exclude;
    if (exclude)
        settings.setValue("exclude/" + notebook.uid, true);
    else
        settings.remove("exclude/" + notebook.uid);

    mNotebooks.insert(notebook.uid, notebook);
    return true;
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
        CalendarData::Notebook notebook = mNotebooks.value(notebookUid);
        notebook.color = color;
        mNotebooks.insert(notebook.uid, notebook);

        QSettings settings("nemo", "nemo-qml-plugin-calendar");
        settings.setValue("colors/" + notebook.uid, notebook.color);

        emit notebooksChanged(mNotebooks.values());
    }
}

QHash<QString, CalendarData::EventOccurrence>
CalendarWorker::eventOccurrences(const QList<CalendarData::Range> &ranges) const
{
    mKCal::ExtendedCalendar::ExpandedIncidenceList events;
    foreach (CalendarData::Range range, ranges) {
        // mkcal fails to consider all day event end time inclusivity on this, add -1 days to start date
        mKCal::ExtendedCalendar::ExpandedIncidenceList newEvents =
                mCalendar->rawExpandedEvents(range.first.addDays(-1), range.second,
                                             false, false, KDateTime::Spec(KDateTime::LocalZone));
        events = events << newEvents;
    }

    QStringList excluded = excludedNotebooks();
    QHash<QString, CalendarData::EventOccurrence> filtered;

    for (int kk = 0; kk < events.count(); ++kk) {
        // Filter out excluded notebooks
        if (excluded.contains(mCalendar->notebook(events.at(kk).second)))
            continue;

        mKCal::ExtendedCalendar::ExpandedIncidence exp = events.at(kk);
        CalendarData::EventOccurrence occurrence;
        occurrence.eventUid = exp.second->uid();
        occurrence.recurrenceId = exp.second->recurrenceId();
        occurrence.startTime = exp.first.dtStart;
        occurrence.endTime = exp.first.dtEnd;
        filtered.insert(occurrence.getId(), occurrence);
    }

    return filtered;
}

QHash<QDate, QStringList>
CalendarWorker::dailyEventOccurrences(const QList<CalendarData::Range> &ranges,
                                      const QMultiHash<QString, KDateTime> &allDay,
                                      const QList<CalendarData::EventOccurrence> &occurrences)
{
    QHash<QDate, QStringList> occurrenceHash;
    foreach (const CalendarData::Range &range, ranges) {
        QDate start = range.first;
        while (start <= range.second) {
            foreach (const CalendarData::EventOccurrence &eo, occurrences) {
                // On all day events the end time is inclusive, otherwise not
                if ((eo.startTime.date() < start
                     && (eo.endTime.date() > start
                         || (eo.endTime.date() == start && (allDay.contains(eo.eventUid, eo.recurrenceId)
                                                            || eo.endTime.time() > QTime(0, 0)))))
                        || (eo.startTime.date() >= start && eo.startTime.date() <= start)) {
                    occurrenceHash[start].append(eo.getId());
                }
            }
            start = start.addDays(1);
        }
    }
    return occurrenceHash;
}

void CalendarWorker::loadData(const QList<CalendarData::Range> &ranges,
                              const QStringList &uidList,
                              bool reset)
{
    foreach (const CalendarData::Range &range, ranges)
        mStorage->load(range.first, range.second.addDays(1)); // end date is not inclusive

    // Note: omitting recurrence ids since loadRecurringIncidences() loads them anyway
    foreach (const QString &uid, uidList)
        mStorage->load(uid);

    // Load all recurring incidences, we have no other way to detect if they occur within a range
    mStorage->loadRecurringIncidences();

    KCalCore::Event::List list = mCalendar->rawEvents();

    if (reset)
        mSentEvents.clear();

    QMultiHash<QString, CalendarData::Event> events;
    QMultiHash<QString, KDateTime> allDay;
    bool orphansDeleted = false;

    foreach (const KCalCore::Event::Ptr e, list) {
        // The database may have changed after loading the events, make sure that the notebook
        // of the event still exists.
        mKCal::Notebook::Ptr notebook = mStorage->notebook(mCalendar->notebook(e));
        if (notebook.isNull()) {
            // This may be a symptom of a deeper bug: if a sync adapter (or mkcal)
            // doesn't delete events which belong to a deleted notebook, then the
            // events will be "orphan" and need to be deleted.
            if (mStorage->load(e->uid())) {
                KCalCore::Incidence::Ptr orphan = mCalendar->incidence(e->uid(), KDateTime());
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

        CalendarData::Event event = createEventStruct(e, notebook);

        if (!mSentEvents.contains(event.uniqueId, event.recurrenceId)) {
            mSentEvents.insert(event.uniqueId, event.recurrenceId);
            events.insert(event.uniqueId, event);
            if (event.allDay)
                allDay.insert(event.uniqueId, event.recurrenceId);
        }
    }

    if (orphansDeleted) {
        save(); // save the orphan deletions to storage.
    }

    QHash<QString, CalendarData::EventOccurrence> occurrences = eventOccurrences(ranges);
    QHash<QDate, QStringList> dailyOccurrences = dailyEventOccurrences(ranges, allDay, occurrences.values());

    emit dataLoaded(ranges, uidList, events, occurrences, dailyOccurrences, reset);
}

CalendarData::Event CalendarWorker::createEventStruct(const KCalCore::Event::Ptr &e,
                                                      mKCal::Notebook::Ptr notebook) const
{
    CalendarData::Event event;
    event.uniqueId = e->uid();
    event.recurrenceId = e->recurrenceId();
    event.allDay = e->allDay();
    event.calendarUid = mCalendar->notebook(e);
    event.description = e->description();
    event.displayLabel = e->summary();
    event.endTime = e->dtEnd();
    event.location = e->location();
    event.secrecy = CalendarUtils::convertSecrecy(e);
    event.readOnly = mStorage->notebook(event.calendarUid)->isReadOnly();
    event.recur = CalendarUtils::convertRecurrence(e);
    bool externalInvitation = false;
    const QString &calendarOwnerEmail = getNotebookAddress(e);

    KCalCore::Person::Ptr organizer = e->organizer();
    if (!organizer.isNull()) {
        QString organizerEmail = organizer->email();

        if (!organizerEmail.isEmpty() && organizerEmail != calendarOwnerEmail
                && (notebook.isNull() || !notebook->sharedWith().contains(organizer->email()))) {
            externalInvitation = true;
        }
    }
    event.externalInvitation = externalInvitation;

    // It would be good to set the attendance status directly in the event within the plugin,
    // however in some cases the account email and owner attendee email won't necessarily match
    // (e.g. in the case where server-side aliases are defined but unknown to the plugin).
    // So we handle this here to avoid "missing" some status changes due to owner email mismatch.
    // This defaults to QString() -> ResponseUnspecified in case the property is undefined
    event.ownerStatus = CalendarUtils::convertResponseType(e->nonKDECustomProperty("X-EAS-RESPONSE-TYPE"));

    KCalCore::Attendee::List attendees = e->attendees();

    foreach (KCalCore::Attendee::Ptr calAttendee, attendees) {
        if (calAttendee->email() == calendarOwnerEmail) {
            if (CalendarUtils::convertPartStat(calAttendee->status()) != CalendarEvent::ResponseUnspecified) {
                // Override the ResponseType
                event.ownerStatus = CalendarUtils::convertPartStat(calAttendee->status());
            }
            //TODO: KCalCore::Attendee::RSVP() returns false even if response was requested for some accounts like Google.
            // We can use attendee role until the problem is not fixed (probably in Google plugin).
            // To be updated later when google account support for responses is added.
            event.rsvp = calAttendee->RSVP();// || calAttendee->role() != KCalCore::Attendee::Chair;
        }
    }

    KCalCore::RecurrenceRule *defaultRule = e->recurrence()->defaultRRule();
    if (defaultRule) {
        event.recurEndDate = defaultRule->endDt().date();
    }
    event.reminder = CalendarUtils::getReminder(e);
    event.startTime = e->dtStart();
    return event;
}

void CalendarWorker::loadNotebooks()
{
    QStringList defaultNotebookColors = QStringList() << "#00aeef" << "red" << "blue" << "green" << "pink" << "yellow";
    int nextDefaultNotebookColor = 0;

    mKCal::Notebook::List notebooks = mStorage->notebooks();
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

        notebook.excluded = settings.value("exclude/" + notebook.uid, false).toBool();

        notebook.color = settings.value("colors/" + notebook.uid, QString()).toString();
        if (notebook.color.isEmpty())
            notebook.color = mkNotebook->color();
        if (notebook.color.isEmpty())
            notebook.color = defaultNotebookColors.at((nextDefaultNotebookColor++) % defaultNotebookColors.count());

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

        if (mkNotebook->isVisible()) {
            newNotebooks.insert(notebook.uid, notebook);
        }
    }

    if (changed || mNotebooks.count() != newNotebooks.count()) {
        mNotebooks = newNotebooks;
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}


CalendarData::EventOccurrence CalendarWorker::getNextOccurrence(const QString &uid,
                                                                const KDateTime &recurrenceId,
                                                                const QDateTime &start) const
{
    KCalCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);
    return CalendarUtils::getNextOccurrence(event, start);
}

QList<CalendarData::Attendee> CalendarWorker::getEventAttendees(const QString &uid, const KDateTime &recurrenceId)
{
    QList<CalendarData::Attendee> result;

    KCalCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);

    if (event.isNull()) {
        return result;
    }

    return CalendarUtils::getEventAttendees(event);
}

void CalendarWorker::findMatchingEvent(const QString &invitationFile)
{
    KCalCore::MemoryCalendar::Ptr cal(new KCalCore::MemoryCalendar(KDateTime::Spec::LocalZone()));
    CalendarUtils::importFromFile(invitationFile, cal);
    KCalCore::Incidence::List incidenceList = cal->incidences();
    for (int i = 0; i < incidenceList.size(); i++) {
        KCalCore::Incidence::Ptr incidence = incidenceList.at(i);
        if (incidence->type() == KCalCore::IncidenceBase::TypeEvent) {
            // Search for this event in the database.
            loadData(QList<CalendarData::Range>() << qMakePair(incidence->dtStart().date().addDays(-1), incidence->dtStart().date().addDays(1)), QStringList(), false);
            KCalCore::Incidence::List dbIncidences = mCalendar->incidences();
            Q_FOREACH (KCalCore::Incidence::Ptr dbIncidence, dbIncidences) {
                const QString remoteUidValue(dbIncidence->nonKDECustomProperty("X-SAILFISHOS-REMOTE-UID"));
                if (dbIncidence->uid().compare(incidence->uid(), Qt::CaseInsensitive) == 0 ||
                        remoteUidValue.compare(incidence->uid(), Qt::CaseInsensitive) == 0) {
                    if ((!incidence->hasRecurrenceId() && !dbIncidence->hasRecurrenceId())
                            || (incidence->hasRecurrenceId() && dbIncidence->hasRecurrenceId()
                                && incidence->recurrenceId() == dbIncidence->recurrenceId())) {
                        emit findMatchingEventFinished(invitationFile, createEventStruct(dbIncidence.staticCast<KCalCore::Event>()));
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
