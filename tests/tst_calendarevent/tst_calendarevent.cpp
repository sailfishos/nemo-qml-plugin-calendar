/*
 * Copyright (c) 2014 - 2019 Jolla Ltd.
 * Copyright (c) 2020 Open Mobile Platform LLC.
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
#include <QObject>
#include <QtTest>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QSet>
#include <QDateTime>

#include "calendarapi.h"
#include "calendarevent.h"
#include "calendareventquery.h"
#include "calendaragendamodel.h"
#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "calendarworker.h"
#include "test_plugin/test_plugin.h"

#include <servicehandler.h>

#include "plugin.cpp"

class tst_CalendarEvent : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void modSetters();
    void testSave();
    void testModify();
    void testTimeZone_data();
    void testTimeZone();
    void testRecurrenceException();
    void testDate_data();
    void testDate();
    void testRecurrence_data();
    void testRecurrence();
    void testRecurWeeklyDays();
    void testAttendees();

private:
    bool saveEvent(CalendarEventModification *eventMod, QString *uid);
    QQmlEngine *engine;
    CalendarApi *calendarApi;
    QSet<QString> mSavedEvents;
};

void tst_CalendarEvent::initTestCase()
{
    // Create plugin, it shuts down the DB in proper order
    engine = new QQmlEngine(this);
    NemoCalendarPlugin* plugin = new NemoCalendarPlugin();
    plugin->initializeEngine(engine, "foobar");
    calendarApi = new CalendarApi(this);

    // Use test plugins, instead of mKCal ones.
    if (qgetenv("MKCAL_PLUGIN_DIR").isEmpty())
        qputenv("MKCAL_PLUGIN_DIR", "plugins");

    // Ensure a default notebook exists for saving new events
    CalendarManager *manager = CalendarManager::instance();
    if (manager->notebooks().isEmpty()) {
        // Here there is a race condition, because we cannot install
        // the signal before the notebooks are read in the thread.
        // In case it happens, we just let the init listener timeout.
        QSignalSpy init(manager, &CalendarManager::notebooksChanged);
        init.wait();
    }
    if (manager->defaultNotebook().isEmpty()) {
        manager->setDefaultNotebook(manager->notebooks().value(0).uid);
    }

    // FIXME: calls made directly after instantiating seem to have threading issues.
    // QDateTime/Timezone initialization somehow fails and caches invalid timezones,
    // resulting in times such as 2014-11-26T00:00:00--596523:-14
    // (Above offset hour is -2147482800 / (60*60))
    QTest::qWait(100);
}

void tst_CalendarEvent::modSetters()
{
    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    QSignalSpy allDaySpy(eventMod, SIGNAL(allDayChanged()));
    bool allDay = !eventMod->allDay();
    eventMod->setAllDay(allDay);
    QCOMPARE(allDaySpy.count(), 1);
    QCOMPARE(eventMod->allDay(), allDay);

    QSignalSpy descriptionSpy(eventMod, SIGNAL(descriptionChanged()));
    QLatin1String description("Test event");
    eventMod->setDescription(description);
    QCOMPARE(descriptionSpy.count(), 1);
    QCOMPARE(eventMod->description(), description);

    QSignalSpy displayLabelSpy(eventMod, SIGNAL(displayLabelChanged()));
    QLatin1String displayLabel("Test display label");
    eventMod->setDisplayLabel(displayLabel);
    QCOMPARE(displayLabelSpy.count(), 1);
    QCOMPARE(eventMod->displayLabel(), displayLabel);

    QSignalSpy locationSpy(eventMod, SIGNAL(locationChanged()));
    QLatin1String location("Test location");
    eventMod->setLocation(location);
    QCOMPARE(locationSpy.count(), 1);
    QCOMPARE(eventMod->location(), location);

    QSignalSpy endTimeSpy(eventMod, SIGNAL(endTimeChanged()));
    QDateTime endTime = QDateTime::currentDateTime();
    eventMod->setEndTime(endTime, Qt::LocalTime);
    QCOMPARE(endTimeSpy.count(), 1);
    QCOMPARE(eventMod->endTime(), endTime);

    QSignalSpy recurSpy(eventMod, SIGNAL(recurChanged()));
    CalendarEvent::Recur recur = CalendarEvent::RecurDaily; // Default value is RecurOnce
    eventMod->setRecur(recur);
    QCOMPARE(recurSpy.count(), 1);
    QCOMPARE(eventMod->recur(), recur);

    QSignalSpy recurEndSpy(eventMod, SIGNAL(recurEndDateChanged()));
    QDateTime recurEnd = QDateTime::currentDateTime().addDays(100);
    eventMod->setRecurEndDate(recurEnd);
    QCOMPARE(recurEndSpy.count(), 1);
    QCOMPARE(eventMod->recurEndDate(), QDateTime(recurEnd.date())); // day precision

    QSignalSpy reminderSpy(eventMod, SIGNAL(reminderChanged()));
    QVERIFY(eventMod->reminder() < 0); // default is ReminderNone == negative reminder.
    int reminder = 900; // 15 minutes before
    eventMod->setReminder(reminder);
    QCOMPARE(reminderSpy.count(), 1);
    QCOMPARE(eventMod->reminder(), reminder);

    QSignalSpy startTimeSpy(eventMod, SIGNAL(startTimeChanged()));
    QDateTime startTime = QDateTime::currentDateTime();
    eventMod->setStartTime(startTime, Qt::LocalTime);
    QCOMPARE(startTimeSpy.count(), 1);
    QCOMPARE(eventMod->startTime(), startTime);

    delete eventMod;
}

void tst_CalendarEvent::testSave()
{
    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    bool allDay = false;
    eventMod->setAllDay(allDay);
    QCOMPARE(eventMod->allDay(), allDay);

    QLatin1String description("Test event");
    eventMod->setDescription(description);
    QCOMPARE(eventMod->description(), description);

    QLatin1String displayLabel("Test display label");
    eventMod->setDisplayLabel(displayLabel);
    QCOMPARE(eventMod->displayLabel(), displayLabel);

    QLatin1String location("Test location");
    eventMod->setLocation(location);
    QCOMPARE(eventMod->location(), location);

    QDateTime startTime = QDateTime::currentDateTime().addDays(-200);
    const QTime at = startTime.time();
    startTime.setTime(QTime(at.hour(), at.minute(), at.second()));
    eventMod->setStartTime(startTime, Qt::LocalTime);
    QCOMPARE(eventMod->startTime(), startTime);

    QDateTime endTime = startTime.addSecs(3600);
    eventMod->setEndTime(endTime, Qt::LocalTime);
    QCOMPARE(eventMod->endTime(), endTime);

    CalendarEvent::Recur recur = CalendarEvent::RecurDaily;
    eventMod->setRecur(recur);
    QCOMPARE(eventMod->recur(), recur);

    QDateTime recurEnd = endTime.addDays(100);
    eventMod->setRecurEndDate(recurEnd);
    QCOMPARE(eventMod->recurEndDate(), QDateTime(recurEnd.date()));

    int reminder = 0; // at the time of the event
    eventMod->setReminder(reminder);
    QCOMPARE(eventMod->reminder(), reminder);

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    CalendarEventQuery query;
    QSignalSpy eventSpy(&query, &CalendarEventQuery::eventChanged);
    query.setUniqueId(uid);
    QVERIFY(eventSpy.wait());

    CalendarStoredEvent *eventB = (CalendarStoredEvent*) query.event();
    QVERIFY(eventB != 0);

    // mKCal DB stores times as seconds, losing millisecond accuracy.
    // Compare dates with QDateTime::toTime_t() instead of QDateTime::toMSecsSinceEpoch()
    QCOMPARE(eventB->endTime().toTime_t(), endTime.toTime_t());
    QCOMPARE(eventB->startTime().toTime_t(), startTime.toTime_t());

    QCOMPARE(eventB->endTime().timeSpec(), Qt::LocalTime);
    QCOMPARE(eventB->startTime().timeSpec(), Qt::LocalTime);
    QCOMPARE(eventB->endTimeSpec(), endTime.timeSpec());
    QCOMPARE(eventB->startTimeSpec(), startTime.timeSpec());
    QCOMPARE(eventB->endTimeZone().toUtf8(), endTime.timeZone().id());
    QCOMPARE(eventB->startTimeZone().toUtf8(), startTime.timeZone().id());

    QCOMPARE(eventB->allDay(), allDay);
    QCOMPARE(eventB->description(), description);
    QCOMPARE(eventB->displayLabel(), displayLabel);
    QCOMPARE(eventB->location(), location);
    QCOMPARE(eventB->recur(), recur);
    QCOMPARE(eventB->recurEndDate(), QDateTime(recurEnd.date()));
    QCOMPARE(eventB->reminder(), reminder);

    eventB->deleteEvent();
    QVERIFY(eventSpy.wait());
    QVERIFY(!query.event());
    mSavedEvents.remove(uid);

    delete eventMod;
}

void tst_CalendarEvent::testModify()
{
    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    eventMod->setStartTime(QDateTime(QDate(2022, 3, 15), QTime(14, 9)), Qt::UTC);

    QString uid;
    QVERIFY(saveEvent(eventMod, &uid));
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);
    delete eventMod;

    CalendarEventQuery query;
    QSignalSpy eventSpy(&query, &CalendarEventQuery::eventChanged);
    query.setUniqueId(uid);
    QVERIFY(eventSpy.wait());

    CalendarStoredEvent *event = qobject_cast<CalendarStoredEvent*>(query.event());
    QVERIFY(event);

    QSignalSpy modSpy(event, &CalendarStoredEvent::startTimeChanged);
    eventMod = calendarApi->createModification(event);
    eventMod->setStartTime(QDateTime(QDate(2022, 3, 15), QTime(14, 13)), Qt::UTC);
    eventMod->save();
    QVERIFY(modSpy.wait());
}

void tst_CalendarEvent::testTimeZone_data()
{
    QTest::addColumn<Qt::TimeSpec>("spec");

    QTest::newRow("local zone") << Qt::LocalTime;
    QTest::newRow("UTC") << Qt::UTC;
    QTest::newRow("time zone") << Qt::TimeZone;
}

void tst_CalendarEvent::testTimeZone()
{
    QFETCH(Qt::TimeSpec, spec);

    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    QDateTime startTime = QDateTime(QDate(2020, 4, 8), QTime(16, 50));
    if (spec == Qt::TimeZone) {
        // Using the system time zone, because agendamodels are looking for
        // events in the same day in the system time zone.
        eventMod->setStartTime(startTime, spec, QDateTime::currentDateTime().timeZone().id());
    } else {
        eventMod->setStartTime(startTime, spec);
    }
    QDateTime endTime = startTime.addSecs(3600);
    if (spec == Qt::TimeZone) {
        eventMod->setEndTime(endTime, spec, QDateTime::currentDateTime().timeZone().id());
    } else {
        eventMod->setEndTime(endTime, spec);
    }

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    CalendarEventQuery query;
    QSignalSpy eventSpy(&query, &CalendarEventQuery::eventChanged);
    query.setUniqueId(uid);
    QVERIFY(eventSpy.wait());

    CalendarStoredEvent *eventB = (CalendarStoredEvent*) query.event();
    QVERIFY(eventB != 0);

    QCOMPARE(eventB->endTime(), endTime);
    QCOMPARE(eventB->startTime(), startTime);

    QCOMPARE(eventB->endTime().timeSpec(), Qt::LocalTime);
    QCOMPARE(eventB->startTime().timeSpec(), Qt::LocalTime);
    QCOMPARE(eventB->endTimeSpec(), spec);
    QCOMPARE(eventB->startTimeSpec(), spec);
    if (spec != Qt::UTC) {
        QCOMPARE(eventB->endTimeZone().toUtf8(), endTime.timeZone().id());
        QCOMPARE(eventB->startTimeZone().toUtf8(), startTime.timeZone().id());
    }

    eventB->deleteEvent();
    QVERIFY(eventSpy.wait());
    QVERIFY(!query.event());
    mSavedEvents.remove(uid);

    delete eventMod;
}

void tst_CalendarEvent::testRecurrenceException()
{
    CalendarEventModification *event = calendarApi->createNewEvent();
    QVERIFY(event != 0);

    // Define event and exception as if device is in Vietnam,
    // then test reread from French time zone.
    const QByteArray TZenv(qgetenv("TZ"));
    qputenv("TZ", "Asia/Ho_Chi_Minh");

    // main event
    event->setDisplayLabel("Recurring event");
    QDateTime startTime = QDateTime(QDate(2014, 6, 7), QTime(12, 00), QTimeZone("Asia/Ho_Chi_Minh"));
    QDateTime endTime = startTime.addSecs(60 * 60);
    event->setStartTime(startTime, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    event->setEndTime(endTime, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    CalendarEvent::Recur recur = CalendarEvent::RecurWeekly;
    event->setRecur(recur);
    QString uid;
    bool ok = saveEvent(event, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    // need event and occurrence to replace....
    CalendarEventQuery query;
    QSignalSpy updated(&query, &CalendarEventQuery::eventChanged);
    query.setUniqueId(uid);
    QDateTime secondStart = startTime.addDays(7);
    query.setStartTime(secondStart);
    QVERIFY(updated.wait());

    CalendarStoredEvent *savedEvent = (CalendarStoredEvent*) query.event();
    QVERIFY(savedEvent);
    QVERIFY(query.occurrence());

    QSignalSpy dataUpdated(CalendarManager::instance(), &CalendarManager::dataUpdated);

    // adjust second occurrence a bit
    CalendarEventModification *recurrenceException = calendarApi->createModification(savedEvent, qobject_cast<CalendarEventOccurrence*>(query.occurrence()));
    QVERIFY(recurrenceException != 0);
    QVERIFY(!recurrenceException->recurrenceIdString().isEmpty());
    QDateTime modifiedSecond = secondStart.addSecs(10*60); // 12:10
    recurrenceException->setStartTime(modifiedSecond, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), Qt::TimeZone, "Asia/Ho_Chi_Minh");
    recurrenceException->setDisplayLabel("Modified recurring event instance");
    recurrenceException->save();
    QVERIFY(dataUpdated.wait());

    // Delete fourth occurrence
    const QDateTime fourth = startTime.addDays(21).toLocalTime();
    calendarApi->remove(savedEvent->uniqueId(), QString(), fourth);
    QVERIFY(dataUpdated.wait());

    // Create an exception on the fifth occurrence
    QSignalSpy occurrenceChanged(&query, &CalendarEventQuery::occurrenceChanged);
    QDateTime fifthStart = startTime.addDays(28);
    query.setStartTime(fifthStart);
    QVERIFY(occurrenceChanged.wait());
    savedEvent = (CalendarStoredEvent*)query.event();
    QVERIFY(query.event());
    QVERIFY(query.occurrence());
    CalendarEventModification *recurrenceSecondException =
        calendarApi->createModification
        (qobject_cast<CalendarStoredEvent*>(query.event()),
         qobject_cast<CalendarEventOccurrence*>(query.occurrence()));
    QVERIFY(recurrenceSecondException != 0);
    QVERIFY(!recurrenceSecondException->recurrenceIdString().isEmpty());
    recurrenceSecondException->setDisplayLabel("Modified recurring event fifth instance");
    recurrenceSecondException->save();
    QVERIFY(dataUpdated.wait());

    // Test and do actions in another time zone
    qputenv("TZ", "Europe/Paris");

    // check the occurrences are correct
    CalendarEventOccurrence *occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(),
                                                                                         startTime.addDays(-1));
    QVERIFY(occurrence);
    // first
    QCOMPARE(occurrence->startTime(), startTime);
    // third
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(1));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), startTime.addDays(14));
    // second is exception
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(recurrenceException->recurrenceIdString(), Qt::ISODate),
                                                                startTime.addDays(1));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);
    // fourth has been deleted and fifth is an exception
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(15));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), startTime.addDays(35));

    // update the exception time
    QSignalSpy eventChangeSpy(&query, SIGNAL(eventChanged()));
    query.resetStartTime();
    query.setRecurrenceIdString(recurrenceException->recurrenceIdString());
    eventChangeSpy.wait();
    QVERIFY(eventChangeSpy.count() > 0);
    QVERIFY(query.event());

    delete recurrenceException;
    recurrenceException = calendarApi->createModification(static_cast<CalendarStoredEvent*>(query.event()));
    QVERIFY(recurrenceException != 0);

    modifiedSecond = modifiedSecond.addSecs(20*60); // 12:30
    recurrenceException->setStartTime(modifiedSecond, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), Qt::TimeZone, "Asia/Ho_Chi_Minh");
    QString modifiedLabel("Modified recurring event instance, ver 2");
    recurrenceException->setDisplayLabel(modifiedLabel);
    recurrenceException->save();
    QVERIFY(dataUpdated.wait()); // allow saved data to be reloaded

    // check the occurrences are correct
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(-1));
    // first
    QCOMPARE(occurrence->startTime(), startTime);
    // third
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(1));
    QCOMPARE(occurrence->startTime(), startTime.addDays(14));
    // second is exception
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(recurrenceException->recurrenceIdString(), Qt::ISODate),
                                                                startTime.addDays(1));

    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);

    // Delete the second exception and check that no
    // occurrence of the parent is present.
    calendarApi->remove(uid, recurrenceSecondException->recurrenceIdString(), QDateTime());
    QVERIFY(dataUpdated.wait());
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(15));
    QVERIFY(occurrence);
    // Fourth (+21 days) and fifth (+28) are now deleted.
    QCOMPARE(occurrence->startTime(), startTime.addDays(35));

    ///////
    // update the main event time within a day, exception stays intact
    CalendarEventModification *mod = calendarApi->createModification(savedEvent);
    QVERIFY(mod != 0);
    QDateTime modifiedStart = startTime.addSecs(40*60); // 12:40
    mod->setStartTime(modifiedStart, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    mod->setEndTime(modifiedStart.addSecs(40*60), Qt::TimeZone, "Asia/Ho_Chi_Minh");
    mod->save();
    QVERIFY(dataUpdated.wait());

    // and check
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(-1));
    QCOMPARE(occurrence->startTime(), modifiedStart);
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(1));
    // TODO: Would be the best if second occurrence in the main series stays away, but at the moment it doesn't.
    //QCOMPARE(occurrence->startTime(), modifiedStart.addDays(14));
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(recurrenceException->recurrenceIdString(), Qt::ISODate),
                                                                startTime.addDays(1));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);

    // The recurrence exception is not listed at second occurrence date anymore.
    // This is because its recurrenceId is not at an occurrence of the parent
    // and KCalendarCore::OccurrenceIterator is not returning it.
    CalendarAgendaModel agendaModel;
    QSignalSpy populated(&agendaModel, &CalendarAgendaModel::updated);
    agendaModel.setStartDate(startTime.addDays(7).date());
    agendaModel.setEndDate(agendaModel.startDate());
    QVERIFY(populated.wait());

    bool modificationFound = false;
    for (int i = 0; i < agendaModel.count(); ++i) {
        QVariant eventVariant = agendaModel.get(i, CalendarAgendaModel::EventObjectRole);
        CalendarEvent *modelEvent = qvariant_cast<CalendarEvent*>(eventVariant);
        // assuming no left-over events
        if (modelEvent && modelEvent->displayLabel() == modifiedLabel) {
            modificationFound = true;
            break;
        }
    }
    QVERIFY(!modificationFound);

    // ensure all is gone
    calendarApi->removeAll(uid);
    QVERIFY(updated.wait());
    QVERIFY(!query.event());
    mSavedEvents.remove(uid);

    delete recurrenceException;
    delete recurrenceSecondException;
    delete mod;

    if (TZenv.isEmpty()) {
        qunsetenv("TZ");
    } else {
        qputenv("TZ", TZenv);
    }
}

// saves event and tries to watch for new uid
bool tst_CalendarEvent::saveEvent(CalendarEventModification *eventMod, QString *uid)
{
    CalendarAgendaModel agendaModel;
    QSignalSpy updated(&agendaModel, &CalendarAgendaModel::updated);
    agendaModel.setStartDate(eventMod->startTime().toLocalTime().date());
    agendaModel.setEndDate(eventMod->endTime().toLocalTime().date());
    if (!updated.wait()) {
        qWarning() << "saveEvent() - agenda not ready";
        return false;
    }

    int count = agendaModel.count();
    QSignalSpy countSpy(&agendaModel, SIGNAL(countChanged()));
    if (countSpy.count() != 0) {
        qWarning() << "saveEvent() - countSpy == 0";
        return false;
    }

    if (eventMod->calendarUid().isEmpty()) {
        eventMod->setCalendarUid(CalendarManager::instance()->defaultNotebook());
    }
    eventMod->save();
    if (!countSpy.wait()) {
        qWarning() << "saveEvent() - no save event";
        return false;
    }

    if (agendaModel.count() != count + 1
            || countSpy.count() == 0) {
        qWarning() << "saveEvent() - invalid counts" << agendaModel.count();
        return false;
    }

    for (int i = 0; i < agendaModel.count(); ++i) {
        QVariant eventVariant = agendaModel.get(i, CalendarAgendaModel::EventObjectRole);
        CalendarEvent *modelEvent = qvariant_cast<CalendarEvent*>(eventVariant);
        // assume no left-over events with same description
        if (modelEvent && modelEvent->description() == eventMod->description()) {
            *uid = modelEvent->uniqueId();
            break;
        }
    }

    return true;
}

void tst_CalendarEvent::testDate_data()
{
    QTest::addColumn<QDate>("date");
    QTest::newRow("2014/12/7") << QDate(2014, 12, 7);
    QTest::newRow("2014/12/8") << QDate(2014, 12, 8);
}

void tst_CalendarEvent::testDate()
{
    QFETCH(QDate, date);

    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    eventMod->setDisplayLabel(QString("test event for ") + date.toString());
    QDateTime startTime(date, QTime(12, 0));
    eventMod->setStartTime(startTime, Qt::LocalTime);
    eventMod->setEndTime(startTime.addSecs(10*60), Qt::LocalTime);

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }

    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);
}

void tst_CalendarEvent::testRecurrence_data()
{
    QTest::addColumn<CalendarEvent::Recur>("recurType");
    QTest::newRow("No recurrence") << CalendarEvent::RecurOnce;
    QTest::newRow("Every day") << CalendarEvent::RecurDaily;
    QTest::newRow("Every week") << CalendarEvent::RecurWeekly;
    QTest::newRow("Every two weeks") << CalendarEvent::RecurBiweekly;
    QTest::newRow("Every month") << CalendarEvent::RecurMonthly;
    QTest::newRow("Every month on same day of week") << CalendarEvent::RecurMonthlyByDayOfWeek;
    QTest::newRow("Every month on last day of week") << CalendarEvent::RecurMonthlyByLastDayOfWeek;
    QTest::newRow("Every year") << CalendarEvent::RecurYearly;
}

void tst_CalendarEvent::testRecurrence()
{
    QFETCH(CalendarEvent::Recur, recurType);

    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    const QDateTime dt(QDate(2020, 4, 27), QTime(8, 0));
    eventMod->setStartTime(dt, Qt::LocalTime);
    eventMod->setEndTime(dt.addSecs(10*60), Qt::LocalTime);
    eventMod->setRecur(recurType);
    eventMod->setDescription(QMetaEnum::fromType<CalendarEvent::Recur>().valueToKey(recurType));

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    CalendarEventQuery query;
    QSignalSpy eventSpy(&query, &CalendarEventQuery::eventChanged);
    query.setUniqueId(uid);
    QVERIFY(eventSpy.wait());

    CalendarStoredEvent *event = (CalendarStoredEvent*)query.event();
    QVERIFY(event);

    QCOMPARE(event->recur(), recurType);

    event->deleteEvent();
    QVERIFY(eventSpy.wait());
    QVERIFY(!query.event());
    mSavedEvents.remove(uid);
}

void tst_CalendarEvent::testRecurWeeklyDays()
{
    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    CalendarEvent::Days days = CalendarEvent::Tuesday;
    days |= CalendarEvent::Wednesday;
    days |= CalendarEvent::Thursday;
    const QDateTime dt(QDate(2020, 4, 30), QTime(9, 0)); // This is a Thursday
    eventMod->setStartTime(dt, Qt::LocalTime);
    eventMod->setEndTime(dt.addSecs(10*60), Qt::LocalTime);
    eventMod->setRecur(CalendarEvent::RecurWeeklyByDays);
    eventMod->setRecurWeeklyDays(days);
    eventMod->setDescription(QLatin1String("Testing weekly by days..."));

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    CalendarEventQuery query;
    QSignalSpy eventSpy(&query, &CalendarEventQuery::eventChanged);
    query.setUniqueId(uid);
    QVERIFY(eventSpy.wait());

    CalendarStoredEvent *event = (CalendarStoredEvent*)query.event();
    QVERIFY(event);

    QCOMPARE(event->recur(), CalendarEvent::RecurWeeklyByDays);
    QCOMPARE(event->recurWeeklyDays(), days);

    event->deleteEvent();
    QVERIFY(eventSpy.wait());
    QVERIFY(!query.event());
    mSavedEvents.remove(uid);
}

void tst_CalendarEvent::testAttendees()
{
    // Ensure that service handler for invitation is using the test plugin.
    mKCal::ExtendedCalendar::Ptr cal(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(cal);
    QVERIFY(storage->open());
    const QString defaultNotebookUid = CalendarManager::instance()->defaultNotebook();
    QVERIFY(!defaultNotebookUid.isEmpty());
    mKCal::Notebook::Ptr defaultNotebook = storage->notebook(defaultNotebookUid);
    QVERIFY(defaultNotebook);
    defaultNotebook->setPluginName(QString::fromLatin1("TestInvitationPlugin"));
    defaultNotebook->setCustomProperty("TEST_EMAIL", QString::fromLatin1("alice@example.org"));
    QVERIFY(storage->updateNotebook(defaultNotebook));

    // Test first the case without attendee.
    CalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    eventMod->setStartTime(QDateTime(QDate(2022, 3, 29), QTime(11, 7)), Qt::LocalTime);
    eventMod->setEndTime(eventMod->startTime().addSecs(600), Qt::LocalTime);
    eventMod->setDescription(QString::fromLatin1("Test without attendee"));

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);
    delete eventMod;

    CalendarEventQuery query;
    QSignalSpy eventSpy(&query, &CalendarEventQuery::attendeesChanged);
    query.setUniqueId(uid);
    QVERIFY(!eventSpy.wait(250));

    QVERIFY(query.attendees().isEmpty());

    // Test the case with attendees
    TestInvitationPlugin *plugin = static_cast<TestInvitationPlugin*>(mKCal::ServiceHandler::instance().service(defaultNotebook->pluginName()));
    QVERIFY(plugin);
    QVERIFY(!plugin->sentInvitation());

    eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    eventMod->setStartTime(QDateTime::currentDateTime(), Qt::LocalTime);
    eventMod->setEndTime(eventMod->startTime().addSecs(600), Qt::LocalTime);
    const QString initialDescr = QString::fromLatin1("Test attendees");
    eventMod->setDescription(initialDescr);
    CalendarContactModel required, optional;
    const QString alice = QString::fromLatin1("Alice");
    const QString bob = QString::fromLatin1("Bob");
    const QString carl = QString::fromLatin1("Carl");
    const QString aliceEmail = QString::fromLatin1("alice@example.org");
    const QString bobEmail = QString::fromLatin1("bob@example.org");
    const QString carlEmail = QString::fromLatin1("carl@example.org");
    required.append(alice, aliceEmail);
    required.append(bob, bobEmail);
    optional.append(carl, carlEmail);
    eventMod->setAttendees(&required, &optional);
    Person Alice(alice, aliceEmail, true, Person::ChairParticipant, Person::UnknownParticipation);
    Person Bob(bob, bobEmail, false, Person::RequiredParticipant, Person::UnknownParticipation);
    Person Carl(carl, carlEmail, false, Person::OptionalParticipant, Person::UnknownParticipation);

    ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);
    delete eventMod;

    // Check that the sendInvitation() service has received the right data.
    const KCalendarCore::Incidence::Ptr sentInvitation = plugin->sentInvitation();
    QVERIFY(sentInvitation);
    QCOMPARE(sentInvitation->uid(), uid);
    const KCalendarCore::Attendee::List sentAttendees = sentInvitation->attendees();
    QCOMPARE(sentAttendees.count(), 3);
    KCalendarCore::Attendee attAlice(alice, aliceEmail, true, KCalendarCore::Attendee::NeedsAction, KCalendarCore::Attendee::ReqParticipant);
    KCalendarCore::Attendee attBob(bob, bobEmail, true, KCalendarCore::Attendee::NeedsAction, KCalendarCore::Attendee::ReqParticipant);
    KCalendarCore::Attendee attCarl(carl, carlEmail, true, KCalendarCore::Attendee::NeedsAction, KCalendarCore::Attendee::OptParticipant);
    QVERIFY(sentAttendees.contains(attAlice));
    QVERIFY(sentAttendees.contains(attBob));
    QVERIFY(sentAttendees.contains(attCarl));

    // Check that saved event locally is presenting the right data.
    query.setUniqueId(uid);
    QVERIFY(eventSpy.wait());

    QList<QObject*> attendees = query.attendees();
    QCOMPARE(attendees.count(), 3);
    QCOMPARE(*qobject_cast<Person*>(attendees[0]), Alice);
    QCOMPARE(*qobject_cast<Person*>(attendees[1]), Bob);
    QCOMPARE(*qobject_cast<Person*>(attendees[2]), Carl);
    qDeleteAll(attendees);

    // Simulate adding neither optional nor required participants by external means.
    QVERIFY(storage->load(uid));
    KCalendarCore::Incidence::Ptr incidence = cal->incidence(uid);
    QVERIFY(incidence);
    const QString dude = QString::fromLatin1("Dude");
    const QString dudeEmail = QString::fromLatin1("dude@example.org");
    Person Dude(dude, dudeEmail, false, Person::NonParticipant, Person::AcceptedParticipation);
    KCalendarCore::Attendee attDude(dude, dudeEmail, false,
                                    KCalendarCore::Attendee::Accepted,
                                    KCalendarCore::Attendee::NonParticipant);
    incidence->addAttendee(attDude);
    QVERIFY(storage->save());

    QVERIFY(eventSpy.wait());
    attendees = query.attendees();
    QCOMPARE(attendees.count(), 4);
    QCOMPARE(*qobject_cast<Person*>(attendees[0]), Alice);
    QCOMPARE(*qobject_cast<Person*>(attendees[1]), Bob);
    QCOMPARE(*qobject_cast<Person*>(attendees[2]), Carl);
    QCOMPARE(*qobject_cast<Person*>(attendees[3]), Dude);
    qDeleteAll(attendees);

    // Do a local modification, by removing participants and adding new.
    eventMod = calendarApi->createModification(qobject_cast<CalendarStoredEvent*>(query.event()));
    QVERIFY(eventMod);
    required.remove(1); // Remove Bob
    optional.remove(0); // Remove Carl
    const QString emily = QString::fromLatin1("Emily");
    const QString emilyEmail = QString::fromLatin1("emily@example.org");
    Person Emily(emily, emilyEmail, false, Person::RequiredParticipant, Person::UnknownParticipation);
    const QString fanny = QString::fromLatin1("Fanny");
    const QString fannyEmail = QString::fromLatin1("fanny@example.org");
    Person Fanny(fanny, fannyEmail, false, Person::OptionalParticipant, Person::UnknownParticipation);
    required.prepend(emily, emilyEmail);
    optional.append(fanny, fannyEmail);
    eventMod->setAttendees(&required, &optional);
    const QString updatedDescr = QString::fromLatin1("Updated description");
    eventMod->setDescription(updatedDescr);
    eventMod->save();
    delete eventMod;

    QVERIFY(eventSpy.wait());
    attendees = query.attendees();
    QCOMPARE(attendees.count(), 4);
    QCOMPARE(*qobject_cast<Person*>(attendees[0]), Alice);
    QCOMPARE(*qobject_cast<Person*>(attendees[1]), Dude);
    QCOMPARE(*qobject_cast<Person*>(attendees[2]), Emily);
    QCOMPARE(*qobject_cast<Person*>(attendees[3]), Fanny);
    qDeleteAll(attendees);

    // Check that the updateInvitation() service as received the right data.
    const KCalendarCore::Incidence::List updatedInvitations = plugin->updatedInvitations();
    QCOMPARE(updatedInvitations.count(), 2);
    // For the cancelled participants.
    const KCalendarCore::Incidence::Ptr cancelled = updatedInvitations[0];
    QVERIFY(cancelled);
    QCOMPARE(cancelled->description(), updatedDescr);
    QCOMPARE(cancelled->status(), KCalendarCore::Incidence::StatusCanceled);
    const KCalendarCore::Attendee::List cancelledAttendees = cancelled->attendees();
    QCOMPARE(cancelledAttendees.count(), 2);
    QVERIFY(cancelledAttendees.contains(attBob));
    QVERIFY(cancelledAttendees.contains(attCarl));
    // For the updated participants.
    const KCalendarCore::Incidence::Ptr updated = updatedInvitations[1];
    QVERIFY(updated);
    QCOMPARE(updated->description(), updatedDescr);
    QCOMPARE(updated->status(), KCalendarCore::Incidence::StatusNone);
    const KCalendarCore::Attendee::List updatedAttendees = updated->attendees();
    QCOMPARE(updatedAttendees.count(), 4);
    QVERIFY(updatedAttendees.contains(attAlice));
    QVERIFY(updatedAttendees.contains(attDude));
    KCalendarCore::Attendee attEmily(emily, emilyEmail, true, KCalendarCore::Attendee::NeedsAction, KCalendarCore::Attendee::ReqParticipant);
    KCalendarCore::Attendee attFanny(fanny, fannyEmail, true, KCalendarCore::Attendee::NeedsAction, KCalendarCore::Attendee::OptParticipant);
    QVERIFY(updatedAttendees.contains(attEmily));
    QVERIFY(updatedAttendees.contains(attFanny));
}

void tst_CalendarEvent::cleanupTestCase()
{
    QSignalSpy modified(CalendarManager::instance(), &CalendarManager::storageModified);
    foreach (const QString &uid, mSavedEvents)
        calendarApi->removeAll(uid);
    if (!mSavedEvents.isEmpty())
        QVERIFY(modified.wait());
}

#include "tst_calendarevent.moc"
QTEST_MAIN(tst_CalendarEvent)
