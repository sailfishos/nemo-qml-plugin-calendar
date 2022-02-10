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

#include "plugin.cpp"

class tst_CalendarEvent : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void modSetters();
    void testSave();
    void testTimeZone_data();
    void testTimeZone();
    void testRecurrenceException();
    void testDate_data();
    void testDate();
    void testRecurrence_data();
    void testRecurrence();
    void testRecurWeeklyDays();

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

    // Ensure a default notebook exists for saving new events
    CalendarManager *manager = CalendarManager::instance();
    // Need to wait for the notebooks to be loaded from worker;
    QTest::qWait(1000);
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

    QDateTime endTime = QDateTime::currentDateTime();
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

    QDateTime startTime = QDateTime::currentDateTime();
    eventMod->setStartTime(startTime, Qt::LocalTime);
    QCOMPARE(eventMod->startTime(), startTime);

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    CalendarEventQuery query;
    query.setUniqueId(uid);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    CalendarEvent *eventB = (CalendarEvent*) query.event();
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

    calendarApi->remove(uid);
    mSavedEvents.remove(uid);

    delete eventMod;
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
    query.setUniqueId(uid);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    CalendarEvent *eventB = (CalendarEvent*) query.event();
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

    calendarApi->remove(uid);
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
    query.setUniqueId(uid);
    QDateTime secondStart = startTime.addDays(7);
    query.setStartTime(secondStart);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    CalendarEvent *savedEvent = (CalendarEvent*) query.event();
    QVERIFY(savedEvent);
    QVERIFY(query.occurrence());

    // adjust second occurrence a bit
    CalendarEventModification *recurrenceException = calendarApi->createModification(savedEvent);
    QVERIFY(recurrenceException != 0);
    QDateTime modifiedSecond = secondStart.addSecs(10*60); // 12:10
    recurrenceException->setStartTime(modifiedSecond, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), Qt::TimeZone, "Asia/Ho_Chi_Minh");
    recurrenceException->setDisplayLabel("Modified recurring event instance");
    CalendarChangeInformation *info
            = recurrenceException->replaceOccurrence(static_cast<CalendarEventOccurrence*>(query.occurrence()));
    QVERIFY(info);
    QSignalSpy doneSpy(info, SIGNAL(pendingChanged()));
    doneSpy.wait();
    QCOMPARE(doneSpy.count(), 1);
    QVERIFY(!info->recurrenceId().isEmpty());

    // Delete fourth occurrence
    const QDateTime fourth = startTime.addDays(21).toLocalTime();
    calendarApi->remove(savedEvent->uniqueId(), QString(), fourth);
    QTest::qWait(1000); // allow saved data to be reloaded

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
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(info->recurrenceId(), Qt::ISODate),
                                                                startTime.addDays(1));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);
    // fourth has been deleted
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(15));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), startTime.addDays(28));
    delete recurrenceException;
    recurrenceException = 0;

    // update the exception time
    QSignalSpy eventChangeSpy(&query, SIGNAL(eventChanged()));
    query.resetStartTime();
    query.setRecurrenceIdString(info->recurrenceId());
    eventChangeSpy.wait();
    QVERIFY(eventChangeSpy.count() > 0);
    QVERIFY(query.event());

    recurrenceException = calendarApi->createModification(static_cast<CalendarEvent*>(query.event()));
    QVERIFY(recurrenceException != 0);

    modifiedSecond = modifiedSecond.addSecs(20*60); // 12:30
    recurrenceException->setStartTime(modifiedSecond, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), Qt::TimeZone, "Asia/Ho_Chi_Minh");
    QString modifiedLabel("Modified recurring event instance, ver 2");
    recurrenceException->setDisplayLabel(modifiedLabel);
    recurrenceException->save();
    QTest::qWait(1000); // allow saved data to be reloaded

    // check the occurrences are correct
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(-1));
    // first
    QCOMPARE(occurrence->startTime(), startTime);
    // third
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(1));
    QCOMPARE(occurrence->startTime(), startTime.addDays(14));
    // second is exception
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(info->recurrenceId(), Qt::ISODate),
                                                                startTime.addDays(1));

    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);

    ///////
    // update the main event time within a day, exception stays intact
    CalendarEventModification *mod = calendarApi->createModification(savedEvent);
    QVERIFY(mod != 0);
    QDateTime modifiedStart = startTime.addSecs(40*60); // 12:40
    mod->setStartTime(modifiedStart, Qt::TimeZone, "Asia/Ho_Chi_Minh");
    mod->setEndTime(modifiedStart.addSecs(40*60), Qt::TimeZone, "Asia/Ho_Chi_Minh");
    mod->save();
    QTest::qWait(1000);

    // and check
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(-1));
    QCOMPARE(occurrence->startTime(), modifiedStart);
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(1));
    // TODO: Would be the best if second occurrence in the main series stays away, but at the moment it doesn't.
    //QCOMPARE(occurrence->startTime(), modifiedStart.addDays(14));
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(info->recurrenceId(), Qt::ISODate),
                                                                startTime.addDays(1));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);

    // at least the recurrence exception should be found at second occurrence date. for now we allow also newly
    // appeared occurrence from main event
    CalendarAgendaModel agendaModel;
    agendaModel.setStartDate(startTime.addDays(7).date());
    agendaModel.setEndDate(agendaModel.startDate());
    QTest::qWait(2000);

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
    QVERIFY(modificationFound);

    // ensure all gone, this emits two warning for not finding the two occurrences.
    calendarApi->removeAll(uid);
    mSavedEvents.remove(uid);
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime(), startTime.addDays(-1));
    QVERIFY(!occurrence->startTime().isValid());
    occurrence = CalendarManager::instance()->getNextOccurrence(uid, QDateTime::fromString(info->recurrenceId(), Qt::ISODate),
                                                                startTime.addDays(1));
    QVERIFY(!occurrence->startTime().isValid());

    delete info;
    delete recurrenceException;
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
    agendaModel.setStartDate(eventMod->startTime().toLocalTime().date());
    agendaModel.setEndDate(eventMod->endTime().toLocalTime().date());

    // Wait for the agendaModel to get updated, no way to determine when that happens, so just wait
    QTest::qWait(2000);

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
    for (int i = 0; i < 30; i++) {
        if (agendaModel.count() > count)
            break;

        QTest::qWait(100);
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
    query.setUniqueId(uid);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    CalendarEvent *event = (CalendarEvent*)query.event();
    QVERIFY(event);

    QCOMPARE(event->recur(), recurType);

    calendarApi->removeAll(uid);
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
    query.setUniqueId(uid);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    CalendarEvent *event = (CalendarEvent*)query.event();
    QVERIFY(event);

    QCOMPARE(event->recur(), CalendarEvent::RecurWeeklyByDays);
    QCOMPARE(event->recurWeeklyDays(), days);

    calendarApi->removeAll(uid);
    mSavedEvents.remove(uid);
}

void tst_CalendarEvent::cleanupTestCase()
{
    foreach (const QString &uid, mSavedEvents) {
        calendarApi->removeAll(uid);
        // make sure worker thread has time to complete removal before being destroyed.
        // TODO: finish method invocation queue before quitting?
        QTest::qWait(1000);
    }
}

#include "tst_calendarevent.moc"
QTEST_MAIN(tst_CalendarEvent)
