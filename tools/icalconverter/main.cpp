/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Chris Adams <chris.adams@jollamobile.com>
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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QString>
#include <QFile>
#include <QBuffer>
#include <QDataStream>
#include <QTextStream>
#include <QtDebug>

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/VCalFormat>
#include <KCalendarCore/Incidence>
#include <KCalendarCore/Event>
#include <KCalendarCore/Todo>
#include <KCalendarCore/Journal>
#include <KCalendarCore/Attendee>

#include <extendedcalendar.h>
#include <extendedstorage.h>

#define LOG_DEBUG(msg) if (printDebug) qDebug() << msg

#define COPY_IF_NOT_EQUAL(dest, src, get, set) \
{ \
    if (dest->get != src->get) { \
        dest->set(src->get); \
    } \
}

#define RETURN_FALSE_IF_NOT_EQUAL(a, b, func, desc) {\
    if (a->func != b->func) {\
        LOG_DEBUG("Incidence" << desc << "" << "properties are not equal:" << a->func << b->func); \
        return false;\
    }\
}

#define RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(failureCheck, desc, debug) {\
    if (failureCheck) {\
        LOG_DEBUG("Incidence" << desc << "properties are not equal:" << desc << debug); \
        return false;\
    }\
}

namespace {
    mKCal::Notebook::Ptr defaultLocalCalendarNotebook(mKCal::ExtendedStorage::Ptr storage)
    {
        mKCal::Notebook::List notebooks = storage->notebooks();
        Q_FOREACH (const mKCal::Notebook::Ptr nb, notebooks) {
            if (nb->isMaster() && !nb->isShared() && nb->pluginName().isEmpty()) {
                // assume that this is the default local calendar notebook.
                return nb;
            }
        }
        qWarning() << "No default local calendar notebook found!";
        return mKCal::Notebook::Ptr();
    }
}

namespace CalendarImportExport {
    namespace IncidenceHandler {
        void normalizePersonEmail(KCalendarCore::Person *p)
        {
            QString email = p->email().replace(QStringLiteral("mailto:"), QString(), Qt::CaseInsensitive);
            if (email != p->email()) {
                p->setEmail(email);
            }
        }

        template <typename T>
        bool pointerDataEqual(const QVector<QSharedPointer<T> > &vectorA, const QVector<QSharedPointer<T> > &vectorB)
        {
            if (vectorA.count() != vectorB.count()) {
                return false;
            }
            for (int i=0; i<vectorA.count(); i++) {
                if (vectorA[i].data() != vectorB[i].data()) {
                    return false;
                }
            }
            return true;
        }

        bool eventsEqual(const KCalendarCore::Event::Ptr &a, const KCalendarCore::Event::Ptr &b, bool printDebug)
        {
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dateEnd() != b->dateEnd(), "dateEnd", (a->dateEnd().toString() + " != " + b->dateEnd().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, transparency(), "transparency");

            // some special handling for dtEnd() depending on whether it's an all-day event or not.
            if (a->allDay() && b->allDay()) {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtEnd().date() != b->dtEnd().date(), "dtEnd", (a->dtEnd().toString() + " != " + b->dtEnd().toString()));
            } else {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtEnd() != b->dtEnd(), "dtEnd", (a->dtEnd().toString() + " != " + b->dtEnd().toString()));
            }

            // some special handling for isMultiday() depending on whether it's an all-day event or not.
            if (a->allDay() && b->allDay()) {
                // here we assume that both events are in "export form" (that is, exclusive DTEND)
                if (a->dtEnd().date() != b->dtEnd().date()) {
                    LOG_DEBUG("have a->dtStart()" << a->dtStart().toString() << ", a->dtEnd()" << a->dtEnd().toString());
                    LOG_DEBUG("have b->dtStart()" << b->dtStart().toString() << ", b->dtEnd()" << b->dtEnd().toString());
                    LOG_DEBUG("have a->isMultiDay()" << a->isMultiDay() << ", b->isMultiDay()" << b->isMultiDay());
                    return false;
                }
            } else {
                RETURN_FALSE_IF_NOT_EQUAL(a, b, isMultiDay(), "multiday");
            }

            // Don't compare hasEndDate() as Event(Event*) does not initialize it based on the validity of
            // dtEnd(), so it could be false when dtEnd() is valid. The dtEnd comparison above is sufficient.

            return true;
        }

        bool todosEqual(const KCalendarCore::Todo::Ptr &a, const KCalendarCore::Todo::Ptr &b, bool printDebug)
        {
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasCompletedDate(), "hasCompletedDate");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtRecurrence() != b->dtRecurrence(), "dtRecurrence", (a->dtRecurrence().toString() + " != " + b->dtRecurrence().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasDueDate(), "hasDueDate");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtDue() != b->dtDue(), "dtDue", (a->dtDue().toString() + " != " + b->dtDue().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasStartDate(), "hasStartDate");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, isCompleted(), "isCompleted");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->completed() != b->completed(), "completed", (a->completed().toString() + " != " + b->completed().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, isOpenEnded(), "isOpenEnded");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, percentComplete(), "percentComplete");
            return true;
        }

        bool journalsEqual(const KCalendarCore::Journal::Ptr &, const KCalendarCore::Journal::Ptr &, bool)
        {
            // no journal-specific properties; it only uses the base incidence properties
            return true;
        }

        // Checks whether a specific set of properties are equal.
        bool copiedPropertiesAreEqual(const KCalendarCore::Incidence::Ptr &a, const KCalendarCore::Incidence::Ptr &b, bool printDebug)
        {
            if (!a || !b) {
                qWarning() << "Invalid paramters! a:" << a << "b:" << b;
                return false;
            }

            // Do not compare created() or lastModified() because we don't update these fields when
            // an incidence is updated by copyIncidenceProperties(), so they are guaranteed to be unequal.
            // TODO compare deref alarms and attachment lists to compare them also.
            // Don't compare resources() for now because KCalendarCore may insert QStringList("") as the resources
            // when in fact it should be QStringList(), which causes the comparison to fail.
            RETURN_FALSE_IF_NOT_EQUAL(a, b, type(), "type");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, duration(), "duration");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasDuration(), "hasDuration");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, isReadOnly(), "isReadOnly");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, comments(), "comments");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, contacts(), "contacts");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, altDescription(), "altDescription");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, categories(), "categories");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, customStatus(), "customStatus");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, description(), "description");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(!qFuzzyCompare(a->geoLatitude(), b->geoLatitude()), "geoLatitude", (QString("%1 != %2").arg(a->geoLatitude()).arg(b->geoLatitude())));
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(!qFuzzyCompare(a->geoLongitude(), b->geoLongitude()), "geoLongitude", (QString("%1 != %2").arg(a->geoLongitude()).arg(b->geoLongitude())));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasGeo(), "hasGeo");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, location(), "location");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, secrecy(), "secrecy");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, status(), "status");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, summary(), "summary");

            // check recurrence information. Note that we only need to check the recurrence rules for equality if they both recur.
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->recurs() != b->recurs(), "recurs", a->recurs() + " != " + b->recurs());
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->recurs() && *(a->recurrence()) != *(b->recurrence()), "recurrence", "...");

            // some special handling for dtStart() depending on whether it's an all-day event or not.
            if (a->allDay() && b->allDay()) {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtStart().date() != b->dtStart().date(), "dtStart", (a->dtStart().toString() + " != " + b->dtStart().toString()));
            } else {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtStart() != b->dtStart(), "dtStart", (a->dtStart().toString() + " != " + b->dtStart().toString()));
            }

            // Some servers insert a mailto: in the organizer email address, so ignore this when comparing organizers
            KCalendarCore::Person personA(a->organizer());
            KCalendarCore::Person personB(b->organizer());
            normalizePersonEmail(&personA);
            normalizePersonEmail(&personB);
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(personA != personB, "organizer", (personA.fullName() + " != " + personB.fullName()));

            switch (a->type()) {
            case KCalendarCore::IncidenceBase::TypeEvent:
                if (!eventsEqual(a.staticCast<KCalendarCore::Event>(), b.staticCast<KCalendarCore::Event>(), printDebug)) {
                    return false;
                }
                break;
            case KCalendarCore::IncidenceBase::TypeTodo:
                if (!todosEqual(a.staticCast<KCalendarCore::Todo>(), b.staticCast<KCalendarCore::Todo>(), printDebug)) {
                    return false;
                }
                break;
            case KCalendarCore::IncidenceBase::TypeJournal:
                if (!journalsEqual(a.staticCast<KCalendarCore::Journal>(), b.staticCast<KCalendarCore::Journal>(), printDebug)) {
                    return false;
                }
                break;
            case KCalendarCore::IncidenceBase::TypeFreeBusy:
            case KCalendarCore::IncidenceBase::TypeUnknown:
                LOG_DEBUG("Unable to compare FreeBusy or Unknown incidence, assuming equal");
                break;
            }
            return true;
        }

        void copyIncidenceProperties(KCalendarCore::Incidence::Ptr dest, const KCalendarCore::Incidence::Ptr &src)
        {
            if (!dest || !src) {
                qWarning() << "Invalid parameters!";
                return;
            }
            if (dest->type() != src->type()) {
                qWarning() << "incidences do not have same type!";
                return;
            }

            QDateTime origCreated = dest->created();
            QDateTime origLastModified = dest->lastModified();

            // Copy recurrence information if required.
            if (*(dest->recurrence()) != *(src->recurrence())) {
                dest->recurrence()->clear();

                KCalendarCore::Recurrence *dr = dest->recurrence();
                KCalendarCore::Recurrence *sr = src->recurrence();

                // recurrence rules and dates
                KCalendarCore::RecurrenceRule::List srRRules = sr->rRules();
                for (QList<KCalendarCore::RecurrenceRule*>::const_iterator it = srRRules.constBegin(), end = srRRules.constEnd(); it != end; ++it) {
                    KCalendarCore::RecurrenceRule *r = new KCalendarCore::RecurrenceRule(*(*it));
                    dr->addRRule(r);
                }
                dr->setRDates(sr->rDates());
                dr->setRDateTimes(sr->rDateTimes());

                // exception rules and dates
                KCalendarCore::RecurrenceRule::List srExRules = sr->exRules();
                for (QList<KCalendarCore::RecurrenceRule*>::const_iterator it = srExRules.constBegin(), end = srExRules.constEnd(); it != end; ++it) {
                    KCalendarCore::RecurrenceRule *r = new KCalendarCore::RecurrenceRule(*(*it));
                    dr->addExRule(r);
                }
                dr->setExDates(sr->exDates());
                dr->setExDateTimes(sr->exDateTimes());
            }

            // copy the duration before the dtEnd as calling setDuration() changes the dtEnd
            COPY_IF_NOT_EQUAL(dest, src, duration(), setDuration);

            if (dest->type() == KCalendarCore::IncidenceBase::TypeEvent && src->type() == KCalendarCore::IncidenceBase::TypeEvent) {
                KCalendarCore::Event::Ptr destEvent = dest.staticCast<KCalendarCore::Event>();
                KCalendarCore::Event::Ptr srcEvent = src.staticCast<KCalendarCore::Event>();
                COPY_IF_NOT_EQUAL(destEvent, srcEvent, dtEnd(), setDtEnd);
                COPY_IF_NOT_EQUAL(destEvent, srcEvent, transparency(), setTransparency);
            }

            if (dest->type() == KCalendarCore::IncidenceBase::TypeTodo && src->type() == KCalendarCore::IncidenceBase::TypeTodo) {
                KCalendarCore::Todo::Ptr destTodo = dest.staticCast<KCalendarCore::Todo>();
                KCalendarCore::Todo::Ptr srcTodo = src.staticCast<KCalendarCore::Todo>();
                COPY_IF_NOT_EQUAL(destTodo, srcTodo, completed(), setCompleted);
                COPY_IF_NOT_EQUAL(destTodo, srcTodo, dtRecurrence(), setDtRecurrence);
                COPY_IF_NOT_EQUAL(destTodo, srcTodo, percentComplete(), setPercentComplete);
            }

            // dtStart and dtEnd changes allDay value, so must set those before copying allDay value
            COPY_IF_NOT_EQUAL(dest, src, dtStart(), setDtStart);
            COPY_IF_NOT_EQUAL(dest, src, allDay(), setAllDay);

            COPY_IF_NOT_EQUAL(dest, src, hasDuration(), setHasDuration);
            COPY_IF_NOT_EQUAL(dest, src, organizer(), setOrganizer);
            COPY_IF_NOT_EQUAL(dest, src, isReadOnly(), setReadOnly);

            if (src->attendees() != dest->attendees()) {
                dest->clearAttendees();
                Q_FOREACH (const KCalendarCore::Attendee &attendee, src->attendees()) {
                    dest->addAttendee(attendee);
                }
            }

            if (src->comments() != dest->comments()) {
                dest->clearComments();
                Q_FOREACH (const QString &comment, src->comments()) {
                    dest->addComment(comment);
                }
            }
            if (src->contacts() != dest->contacts()) {
                dest->clearContacts();
                Q_FOREACH (const QString &contact, src->contacts()) {
                    dest->addContact(contact);
                }
            }

            COPY_IF_NOT_EQUAL(dest, src, altDescription(), setAltDescription);
            COPY_IF_NOT_EQUAL(dest, src, categories(), setCategories);
            COPY_IF_NOT_EQUAL(dest, src, customStatus(), setCustomStatus);
            COPY_IF_NOT_EQUAL(dest, src, description(), setDescription);
            COPY_IF_NOT_EQUAL(dest, src, geoLatitude(), setGeoLatitude);
            COPY_IF_NOT_EQUAL(dest, src, geoLongitude(), setGeoLongitude);
            COPY_IF_NOT_EQUAL(dest, src, location(), setLocation);
            COPY_IF_NOT_EQUAL(dest, src, resources(), setResources);
            COPY_IF_NOT_EQUAL(dest, src, secrecy(), setSecrecy);
            COPY_IF_NOT_EQUAL(dest, src, status(), setStatus);
            COPY_IF_NOT_EQUAL(dest, src, summary(), setSummary);
            COPY_IF_NOT_EQUAL(dest, src, revision(), setRevision);

            if (!pointerDataEqual(src->alarms(), dest->alarms())) {
                dest->clearAlarms();
                Q_FOREACH (const KCalendarCore::Alarm::Ptr &alarm, src->alarms()) {
                    dest->addAlarm(alarm);
                }
            }

            if (src->attachments() != dest->attachments()) {
                dest->clearAttachments();
                Q_FOREACH (const KCalendarCore::Attachment &attachment, src->attachments()) {
                    dest->addAttachment(attachment);
                }
            }

            // Don't change created and lastModified properties as that affects mkcal
            // calculations for when the incidence was added and modified in the db.
            if (origCreated != dest->created()) {
                dest->setCreated(origCreated);
            }
            if (origLastModified != dest->lastModified()) {
                dest->setLastModified(origLastModified);
            }
        }

        void prepareImportedIncidence(KCalendarCore::Incidence::Ptr incidence, bool printDebug)
        {
            if (incidence->type() != KCalendarCore::IncidenceBase::TypeEvent) {
                qWarning() << "unable to handle imported non-event incidence; skipping";
                return;
            }
            KCalendarCore::Event::Ptr event = incidence.staticCast<KCalendarCore::Event>();

            if (event->allDay()) {
                QDateTime dtStart = event->dtStart();
                QDateTime dtEnd = event->dtEnd();

                // calendar processing requires all-day events to have a dtEnd
                if (!dtEnd.isValid()) {
                    LOG_DEBUG("Adding DTEND to" << incidence->uid() << "as" << dtStart.toString());
                    event->setDtEnd(dtStart);
                }

                // setting dtStart/End changes the allDay value, so ensure it is still set to true
                event->setAllDay(true);
            }
        }

        KCalendarCore::Incidence::Ptr incidenceToExport(KCalendarCore::Incidence::Ptr sourceIncidence, bool printDebug)
        {
            if (sourceIncidence->type() != KCalendarCore::IncidenceBase::TypeEvent) {
                LOG_DEBUG("Incidence not an event; cannot create exportable version");
                return sourceIncidence;
            }

            KCalendarCore::Incidence::Ptr incidence = QSharedPointer<KCalendarCore::Incidence>(sourceIncidence->clone());
            KCalendarCore::Event::Ptr event = incidence.staticCast<KCalendarCore::Event>();
            bool eventIsAllDay = event->allDay();
            if (eventIsAllDay) {
                if (event->dtStart() == event->dtEnd()) {
                    // A single-day all-day event was received without a DTEND, and it is still a single-day
                    // all-day event, so remove the DTEND before upsyncing.
                    LOG_DEBUG("Removing DTEND from" << incidence->uid());
                    event->setDtEnd(QDateTime());
                }
            }

            // setting dtStart/End changes the allDay value, so ensure it is still set to true if needed.
            if (eventIsAllDay) {
                event->setAllDay(true);
            }

            // The default storage implementation applies the organizer as an attendee by default.
            // Undo this as it turns the incidence into a scheduled event requiring acceptance/rejection/etc.
            const KCalendarCore::Person organizer = event->organizer();
            if (!organizer.email().isEmpty()) {
                bool found = false;
                KCalendarCore::Attendee::List attendees = event->attendees();
                for (int i = attendees.size() - 1; i >= 0; --i) {
                    const KCalendarCore::Attendee &attendee(attendees[i]);
                    if (attendee.email() == organizer.email() && attendee.fullName() == organizer.fullName()) {
                        LOG_DEBUG("Discarding organizer as attendee" << attendee.fullName());
                        attendees.removeAt(i);
                        found = true;
                    } else {
                        LOG_DEBUG("Not discarding attendee:" << attendee.fullName() << attendee.email() << ": not organizer:" << organizer.fullName() << organizer.email());
                    }
                }

                if (found) {
                    event->setAttendees(attendees);
                }
            }

            return event;
        }
    }

    void listNotebooks()
    {
        mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::utc()));
        mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
        storage->open();
        storage->load();
        QTextStream qStdout(stdout);
        qStdout << "List of known notebooks on device:" << endl;
        Q_FOREACH (mKCal::Notebook::Ptr notebook, storage->notebooks()) {
            qStdout << "- " << notebook->uid() << ": " << notebook->name() << endl;
        }
        storage->close();
    }

    QString constructExportIcs(mKCal::ExtendedCalendar::Ptr calendar, KCalendarCore::Incidence::List incidencesToExport, bool printDebug)
    {
        // create an in-memory calendar
        // add to it the required incidences (ie, check if has recurrenceId -> load parent and all instances; etc)
        // for each of those, we need to do the IncidenceToExport() modifications first
        // then, export from that calendar to .ics file.
        KCalendarCore::MemoryCalendar::Ptr memoryCalendar(new KCalendarCore::MemoryCalendar(QTimeZone::utc()));
        Q_FOREACH (KCalendarCore::Incidence::Ptr toExport, incidencesToExport) {
          LOG_DEBUG("Exporting incidence:" << toExport->uid());
            if (toExport->hasRecurrenceId() || toExport->recurs()) {
                KCalendarCore::Incidence::Ptr recurringIncidence = toExport->hasRecurrenceId()
                                                        ? calendar->incidence(toExport->uid(), QDateTime())
                                                        : toExport;
                // Don't crash on null instances
                if (recurringIncidence.isNull()) continue;
                KCalendarCore::Incidence::List instances = calendar->instances(recurringIncidence);
                KCalendarCore::Incidence::Ptr exportableIncidence = IncidenceHandler::incidenceToExport(recurringIncidence, printDebug);

                // remove EXDATE values from the recurring incidence which correspond to the persistent occurrences (instances)
                Q_FOREACH (KCalendarCore::Incidence::Ptr instance, instances) {
                    QList<QDateTime> exDateTimes = exportableIncidence->recurrence()->exDateTimes();
                    exDateTimes.removeAll(instance->recurrenceId());
                    exportableIncidence->recurrence()->setExDateTimes(exDateTimes);
                }

                // store the base recurring event into the in-memory calendar
                memoryCalendar->addIncidence(exportableIncidence);

                // now create the persistent occurrences in the in-memory calendar
                Q_FOREACH (KCalendarCore::Incidence::Ptr instance, instances) {
                    // We cannot call dissociateSingleOccurrence() on the MemoryCalendar
                    // as that's an mKCal specific function.
                    // We cannot call dissociateOccurrence() because that function
                    // takes only a QDate instead of a QDateTime recurrenceId.
                    // Thus, we need to manually create an exception occurrence.
                    KCalendarCore::Incidence::Ptr exportableOccurrence(exportableIncidence->clone());
                    exportableOccurrence->setCreated(instance->created());
                    exportableOccurrence->setRevision(instance->revision());
                    exportableOccurrence->clearRecurrence();
                    exportableOccurrence->setRecurrenceId(instance->recurrenceId());
                    exportableOccurrence->setDtStart(instance->recurrenceId());

                    // add it, and then update it in-memory.
                    memoryCalendar->addIncidence(exportableOccurrence);
                    exportableOccurrence = memoryCalendar->incidence(instance->uid(), instance->recurrenceId());
                    exportableOccurrence->startUpdates();
                    IncidenceHandler::copyIncidenceProperties(exportableOccurrence, IncidenceHandler::incidenceToExport(instance, printDebug));
                    exportableOccurrence->endUpdates();
                }
            } else {
                KCalendarCore::Incidence::Ptr exportableIncidence = IncidenceHandler::incidenceToExport(toExport, printDebug);
                memoryCalendar->addIncidence(exportableIncidence);
            }
        }

        KCalendarCore::ICalFormat icalFormat;
        return icalFormat.toString(memoryCalendar, QString(), false);
    }

    QString constructExportIcs(const QString &notebookUid, const QString &incidenceUid, const QDateTime &recurrenceId, bool printDebug)
    {
        // if notebookUid empty, we fall back to the default notebook.
        // if incidenceUid is empty, we load all incidences from the notebook.
        mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::utc()));
        mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
        storage->open();
        storage->load();
        mKCal::Notebook::Ptr notebook = notebookUid.isEmpty() ? defaultLocalCalendarNotebook(storage) : storage->notebook(notebookUid);
        if (!notebook) {
            qWarning() << "No default notebook exists or invalid notebook uid specified:" << notebookUid;
            storage->close();
            return QString();
        }
        LOG_DEBUG("Exporting notebook:" << notebook->uid());

        KCalendarCore::Incidence::List incidencesToExport;
        if (incidenceUid.isEmpty()) {
            storage->loadNotebookIncidences(notebook->uid());
            storage->allIncidences(&incidencesToExport, notebook->uid());
        } else {
            storage->load(incidenceUid);
            incidencesToExport << calendar->incidence(incidenceUid, recurrenceId);
        }
        LOG_DEBUG("Found" << incidencesToExport.length() << "incidences to export.");

        QString retn = constructExportIcs(calendar, incidencesToExport, printDebug);
        storage->close();
        return retn;
    }

    bool updateIncidence(mKCal::ExtendedCalendar::Ptr calendar, mKCal::Notebook::Ptr notebook, KCalendarCore::Incidence::Ptr incidence, bool *criticalError, bool printDebug)
    {
        if (incidence.isNull()) {
            return false;
        }

        KCalendarCore::Incidence::Ptr storedIncidence;
        switch (incidence->type()) {
        case KCalendarCore::IncidenceBase::TypeEvent:
            storedIncidence = calendar->event(incidence->uid(), incidence->hasRecurrenceId() ? incidence->recurrenceId() : QDateTime());
            break;
        case KCalendarCore::IncidenceBase::TypeTodo:
            storedIncidence = calendar->todo(incidence->uid());
            break;
        case KCalendarCore::IncidenceBase::TypeJournal:
            storedIncidence = calendar->journal(incidence->uid());
            break;
        case KCalendarCore::IncidenceBase::TypeFreeBusy:
        case KCalendarCore::IncidenceBase::TypeUnknown:
            qWarning() << "Unsupported incidence type:" << incidence->type();
            return false;
        }
        if (storedIncidence) {
            if (incidence->status() == KCalendarCore::Incidence::StatusCanceled
                    || incidence->customStatus().compare(QStringLiteral("CANCELLED"), Qt::CaseInsensitive) == 0) {
                LOG_DEBUG("Deleting cancelled event:" << storedIncidence->uid() << storedIncidence->recurrenceId().toString());
                if (!calendar->deleteIncidence(storedIncidence)) {
                    qWarning() << "Error removing cancelled occurrence:" << storedIncidence->uid() << storedIncidence->recurrenceId().toString();
                    return false;
                }
            } else {
                LOG_DEBUG("Updating existing event:" << storedIncidence->uid() << storedIncidence->recurrenceId().toString());
                storedIncidence->startUpdates();
                IncidenceHandler::prepareImportedIncidence(incidence, printDebug);
                IncidenceHandler::copyIncidenceProperties(storedIncidence, incidence);

                // if this incidence is a recurring incidence, we should get all persistent occurrences
                // and add them back as EXDATEs.  This is because mkcal expects that dissociated
                // single instances will correspond to an EXDATE, but most sync servers do not (and
                // so will not include the RECURRENCE-ID values as EXDATEs of the parent).
                if (storedIncidence->recurs()) {
                    KCalendarCore::Incidence::List instances = calendar->instances(incidence);
                    Q_FOREACH (KCalendarCore::Incidence::Ptr instance, instances) {
                        if (instance->hasRecurrenceId()) {
                            storedIncidence->recurrence()->addExDateTime(instance->recurrenceId());
                        }
                    }
                }
                storedIncidence->endUpdates();
            }
        } else {
            // the new incidence will be either a new persistent occurrence, or a new base-series (or new non-recurring).
            LOG_DEBUG("Have new incidence:" << incidence->uid() << incidence->recurrenceId().toString());
            KCalendarCore::Incidence::Ptr occurrence;
            if (incidence->hasRecurrenceId()) {
                // no dissociated occurrence exists already (ie, it's not an update), so create a new one.
                // need to detach, and then copy the properties into the detached occurrence.
                KCalendarCore::Incidence::Ptr recurringIncidence = calendar->event(incidence->uid(), QDateTime());
                if (recurringIncidence.isNull()) {
                    qWarning() << "error: parent recurring incidence could not be retrieved:" << incidence->uid();
                    return false;
                }
                occurrence = calendar->dissociateSingleOccurrence(recurringIncidence, incidence->recurrenceId());
                if (occurrence.isNull()) {
                    qWarning() << "error: could not dissociate occurrence from recurring event:" << incidence->uid() << incidence->recurrenceId().toString();
                    return false;
                }

                IncidenceHandler::prepareImportedIncidence(incidence, printDebug);
                IncidenceHandler::copyIncidenceProperties(occurrence, incidence);
                if (!calendar->addEvent(occurrence.staticCast<KCalendarCore::Event>(), notebook->uid())) {
                    qWarning() << "error: could not add dissociated occurrence to calendar";
                    return false;
                }
                LOG_DEBUG("Added new occurrence incidence:" << occurrence->uid() << occurrence->recurrenceId().toString());
            } else {
                // just a new event without needing detach.
                IncidenceHandler::prepareImportedIncidence(incidence, printDebug);
                bool added = false;
                switch (incidence->type()) {
                case KCalendarCore::IncidenceBase::TypeEvent:
                    added = calendar->addEvent(incidence.staticCast<KCalendarCore::Event>(), notebook->uid());
                    break;
                case KCalendarCore::IncidenceBase::TypeTodo:
                    added = calendar->addTodo(incidence.staticCast<KCalendarCore::Todo>(), notebook->uid());
                    break;
                case KCalendarCore::IncidenceBase::TypeJournal:
                    added = calendar->addJournal(incidence.staticCast<KCalendarCore::Journal>(), notebook->uid());
                    break;
                case KCalendarCore::IncidenceBase::TypeFreeBusy:
                case KCalendarCore::IncidenceBase::TypeUnknown:
                    qWarning() << "Unsupported incidence type:" << incidence->type();
                    return false;
                }
                if (added) {
                    LOG_DEBUG("Added new incidence:" << incidence->uid() << incidence->recurrenceId().toString());
                } else {
                    qWarning() << "Unable to add incidence" << incidence->uid() << incidence->recurrenceId().toString() << "to notebook" << notebook->uid();
                    *criticalError = true;
                    return false;
                }
            }
        }
        return true;
    }

    bool importIcsData(const QString &icsData, const QString &notebookUid, bool destructiveImport, bool printDebug)
    {
        KCalendarCore::ICalFormat iCalFormat;
        KCalendarCore::MemoryCalendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::utc()));
        if (!iCalFormat.fromString(cal, icsData)) {
            qWarning() << "unable to parse iCal data, trying as vCal";
            KCalendarCore::VCalFormat vCalFormat;
            if (!vCalFormat.fromString(cal, icsData)) {
                qWarning() << "unable to parse vCal data";
                return false;
            }
        }

        // Reorganize the list of imported incidences into lists of incidences segregated by UID.
        QHash<QString, KCalendarCore::Incidence::List> uidIncidences;
        KCalendarCore::Incidence::List importedIncidences = cal->incidences();
        Q_FOREACH (KCalendarCore::Incidence::Ptr imported, importedIncidences) {
            IncidenceHandler::prepareImportedIncidence(imported, printDebug);
            uidIncidences[imported->uid()] << imported;
        }

        // Now save the imported incidences into the calendar.
        // Note that the import may specify updates to existing events, so
        // we will need to compare the imported incidences with the
        // existing incidences, by UID.
        mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::utc()));
        mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
        storage->open();
        storage->load();
        mKCal::Notebook::Ptr notebook = notebookUid.isEmpty() ? defaultLocalCalendarNotebook(storage) : storage->notebook(notebookUid);
        if (!notebook) {
            qWarning() << "No default notebook exists or invalid notebook uid specified:" << notebookUid;
            storage->close();
            return false;
        }
        KCalendarCore::Incidence::List notebookIncidences;
        storage->loadNotebookIncidences(notebook->uid());
        storage->allIncidences(&notebookIncidences, notebook->uid());

        if (destructiveImport) {
            // Any incidences which don't exist in the import list should be deleted.
            Q_FOREACH (KCalendarCore::Incidence::Ptr possiblyDoomed, notebookIncidences) {
                if (!uidIncidences.contains(possiblyDoomed->uid())) {
                    // no incidence or series with this UID exists in the import list.
                    LOG_DEBUG("Removing rolled-back incidence:" << possiblyDoomed->uid() << possiblyDoomed->recurrenceId().toString());
                    if (!calendar->deleteIncidence(possiblyDoomed)) {
                        qWarning() << "Error removing rolled-back incidence:" << possiblyDoomed->uid() << possiblyDoomed->recurrenceId().toString();
                        storage->close();
                        return false;
                    }
                } // no need to remove rolled-back persistent occurrences here; we do that later.
            }
        }

        Q_FOREACH (const QString &uid, uidIncidences.keys()) {
            // deal with every incidence or series from the import list.
            KCalendarCore::Incidence::List incidences(uidIncidences[uid]);
            // find the recurring incidence (parent) in the import list, and save it.
            // alternatively, it may be a non-recurring base incidence.
            bool criticalError = false;
            int parentIndex = -1;
            for (int i = 0; i < incidences.size(); ++i) {
                if (!incidences[i]->hasRecurrenceId()) {
                    parentIndex = i;
                    break;
                }
            }

            if (parentIndex == -1) {
                LOG_DEBUG("No parent or base incidence in incidence list, performing direct updates to persistent occurrences");
                for (int i = 0; i < incidences.size(); ++i) {
                    KCalendarCore::Incidence::Ptr importInstance = incidences[i];
                    updateIncidence(calendar, notebook, importInstance, &criticalError, printDebug);
                    if (criticalError) {
                        qWarning() << "Error saving updated persistent occurrence:" << importInstance->uid() << importInstance->recurrenceId().toString();
                        storage->close();
                        return false;
                    }
                }
            } else {
                // if there was a parent / base incidence, then we need to compare local/import lists.
                // load the local (persistent) occurrences of the series.  Later we will update or remove them as required.
                KCalendarCore::Incidence::Ptr localBaseIncidence = calendar->incidence(uid, QDateTime());
                KCalendarCore::Incidence::List localInstances;
                if (!localBaseIncidence.isNull() && localBaseIncidence->recurs()) {
                    localInstances = calendar->instances(localBaseIncidence);
                }

                // first save the added/updated base incidence
                LOG_DEBUG("Saving the added/updated base incidence before saving persistent exceptions:" << incidences[parentIndex]->uid());
                KCalendarCore::Incidence::Ptr updatedBaseIncidence = incidences[parentIndex];
                updateIncidence(calendar, notebook, updatedBaseIncidence, &criticalError, printDebug); // update the base incidence first.
                if (criticalError) {
                    qWarning() << "Error saving base incidence:" << updatedBaseIncidence->uid();
                    storage->close();
                    return false;
                }

                // update persistent exceptions which are in the import list.
                QList<QDateTime> importRecurrenceIds;
                for (int i = 0; i < incidences.size(); ++i) {
                    if (i == parentIndex) {
                        continue; // already handled this one.
                    }

                    LOG_DEBUG("Now saving a persistent exception:" << incidences[i]->recurrenceId().toString());
                    KCalendarCore::Incidence::Ptr importInstance = incidences[i];
                    importRecurrenceIds.append(importInstance->recurrenceId());
                    updateIncidence(calendar, notebook, importInstance, &criticalError, printDebug);
                    if (criticalError) {
                        qWarning() << "Error saving updated persistent occurrence:" << importInstance->uid() << importInstance->recurrenceId().toString();
                        storage->close();
                        return false;
                    }
                }

                if (destructiveImport) {
                    // remove persistent exceptions which are not in the import list.
                    for (int i = 0; i < localInstances.size(); ++i) {
                        KCalendarCore::Incidence::Ptr localInstance = localInstances[i];
                        if (!importRecurrenceIds.contains(localInstance->recurrenceId())) {
                            LOG_DEBUG("Removing rolled-back persistent occurrence:" << localInstance->uid() << localInstance->recurrenceId().toString());
                            if (!calendar->deleteIncidence(localInstance)) {
                                qWarning() << "Error removing rolled-back persistent occurrence:" << localInstance->uid() << localInstance->recurrenceId().toString();
                                storage->close();
                                return false;
                            }
                        }
                    }
                }
            }
        }

        storage->save();
        storage->close();
        return true;
    }
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("icalconverter");

    QCommandLineParser parser;
    parser.setApplicationDescription("Command line tool to import / export calendar data from / to ICS data.");
    parser.addHelpOption();

    parser.addPositionalArgument("action", "action to execute, 'import', 'export' or 'list'.");
    parser.addOption(QCommandLineOption(QStringList() << "v" << "verbose",
                                        "extra debugging will be printed."));
    parser.parse(QCoreApplication::arguments());

    const QString command = parser.positionalArguments().isEmpty()
        ? QString() : parser.positionalArguments().first();
    if (command == "import") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument("import", "import the ICS data found in backup.ics.");
        parser.addPositionalArgument("backup", "file to be read.", "backup.ics");
        parser.addOption(QCommandLineOption(QStringList() << "d" << "destructive",
                                            "local calendar data will be removed prior to import."));
    } else if (command == "export") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument("export", "export calendar entries as ICS data in backup.ics.");
        parser.addPositionalArgument("backup", "file to be written.", "backup.ics");
        parser.addOption(QCommandLineOption(QStringList() << "n" << "notebook",
                                            "uid of notebook to export.", "uid"));
    } else if (command == "list") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument("list", "list all notebooks known on device.");
    } else {
        parser.showHelp();
    }
    parser.process(app);

    // parse arguments
    bool verbose = parser.isSet("verbose");

    // perform required operation
    if (verbose) {
        qputenv("KCALDEBUG", "1");
    }
    if (command == QStringLiteral("import")) {
        if (parser.positionalArguments().length() != 2)
            parser.showHelp();
        const QString backupFile = parser.positionalArguments().at(1);
        if (!QFile::exists(backupFile)) {
            qWarning() << "no such file exists:" << backupFile << "; cannot import.";
        } else {
            QFile importFile(backupFile);
            if (importFile.open(QIODevice::ReadOnly)) {
                QString fileData = QString::fromUtf8(importFile.readAll());
                if (CalendarImportExport::importIcsData(fileData, QString(), parser.isSet("destructive"), verbose)) {
                    qDebug() << "Successfully imported:" << backupFile;
                    return 0;
                }
                qWarning() << "Failed to import:" << backupFile;
            } else {
                qWarning() << "Unable to open:" << backupFile << "for import.";
            }
        }
    } else if (command == QStringLiteral("export")) {
        if (parser.positionalArguments().length() != 2)
            parser.showHelp();
        const QString backupFile = parser.positionalArguments().at(1);
        QString exportIcsData = CalendarImportExport::constructExportIcs(parser.value("notebook"), QString(), QDateTime(), verbose);
        if (exportIcsData.isEmpty()) {
            qWarning() << "No data to export!";
            return 0;
        }
        QFile exportFile(backupFile);
        if (exportFile.open(QIODevice::WriteOnly)) {
            QByteArray ba = exportIcsData.toUtf8();
            qint64 bytesRemaining = ba.size();
            while (bytesRemaining) {
                qint64 count = exportFile.write(ba, bytesRemaining);
                if (count == -1) {
                    qWarning() << "Error while writing export data to:" << backupFile;
                    return 1;
                } else {
                    bytesRemaining -= count;
                }
            }
            qDebug() << "Successfully wrote:" << ba.size() << "bytes of export data to:" << backupFile;
            return 0;
        } else {
            qWarning() << "Unable to open:" << backupFile << "for export.";
        }
    } else if (command == QStringLiteral("list")) {
        CalendarImportExport::listNotebooks();
        return 0;
    }

    return 1;
}
