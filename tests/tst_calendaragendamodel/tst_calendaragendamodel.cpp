/*
 * Copyright (c) 2021 Damien Caliste <dcaliste@free.fr>
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
#include <QSignalSpy>
#include <QtTest>

#include "calendaragendamodel.h"
#include "calendareventmodification.h"

#include "plugin.cpp"

class tst_CalendarAgendaModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testStartEndDate();
    void testTimeZone();
    void testAllDays();
};

void tst_CalendarAgendaModel::initTestCase()
{
    CalendarManager *manager = CalendarManager::instance();
    QSignalSpy *ready = new QSignalSpy(manager, &CalendarManager::notebooksChanged);
    QVERIFY(ready->wait());
    
    QSignalSpy *modified = new QSignalSpy(manager, &CalendarManager::storageModified);
    CalendarEventModification *event1 = new CalendarEventModification;
    event1->setDescription(QString::fromLatin1("event 1"));
    event1->setStartTime(QDateTime(QDate(2021, 11, 18), QTime(13, 29)),
                         Qt::TimeZone, QTimeZone::systemTimeZoneId());
    event1->setEndTime(QDateTime(QDate(2021, 11, 18), QTime(14, 29)),
                       Qt::TimeZone, QTimeZone::systemTimeZoneId());
    event1->save();
    QVERIFY(modified->wait());
    CalendarEventModification *event2 = new CalendarEventModification;
    event2->setDescription(QString::fromLatin1("event 2"));
    event2->setStartTime(QDateTime(QDate(2021, 11, 19), QTime(14, 34)),
                         Qt::TimeZone, QTimeZone::systemTimeZoneId());
    event2->setEndTime(QDateTime(QDate(2021, 11, 20), QTime(14, 34)),
                       Qt::TimeZone, QTimeZone::systemTimeZoneId());
    event2->save();
    QVERIFY(modified->wait());
    CalendarEventModification *event3 = new CalendarEventModification;
    event3->setDescription(QString::fromLatin1("event far away"));
    event3->setStartTime(QDateTime(QDate(2021, 11, 11), QTime(3, 0)),
                         Qt::TimeZone, "Asia/Ho_Chi_Minh");
    event3->setEndTime(QDateTime(QDate(2021, 11, 11), QTime(4, 0)),
                       Qt::TimeZone, "Asia/Ho_Chi_Minh");
    event3->save();
    QVERIFY(modified->wait());
    CalendarEventModification *allDayEvent1 = new CalendarEventModification;
    allDayEvent1->setDescription(QString::fromLatin1("all day event 1"));
    allDayEvent1->setStartTime(QDateTime(QDate(2021, 10, 11), QTime(0, 0)),
                               Qt::TimeZone, QTimeZone::systemTimeZoneId());
    allDayEvent1->setEndTime(QDateTime(QDate(2021, 10, 11), QTime(0, 0)),
                             Qt::TimeZone, QTimeZone::systemTimeZoneId());
    allDayEvent1->setAllDay(true);
    allDayEvent1->save();
    QVERIFY(modified->wait());
    CalendarEventModification *allDayEvent2 = new CalendarEventModification;
    allDayEvent2->setDescription(QString::fromLatin1("all day event 2"));
    allDayEvent2->setStartTime(QDateTime(QDate(2021, 10, 12), QTime(0, 0)),
                               Qt::TimeZone, QTimeZone::systemTimeZoneId());
    allDayEvent2->setEndTime(QDateTime(QDate(2021, 10, 15), QTime(0, 0)),
                             Qt::TimeZone, QTimeZone::systemTimeZoneId());
    allDayEvent2->setAllDay(true);
    allDayEvent2->save();
    QVERIFY(modified->wait());
    CalendarEventModification *allDayEvent3 = new CalendarEventModification;
    allDayEvent3->setDescription(QString::fromLatin1("all day event 3"));
    allDayEvent3->setStartTime(QDateTime(QDate(2021, 10, 10), QTime(0, 0)),
                               Qt::TimeZone, QTimeZone::systemTimeZoneId());
    allDayEvent3->setEndTime(QDateTime(QDate(2021, 10, 10), QTime(0, 0)),
                             Qt::TimeZone, QTimeZone::systemTimeZoneId());
    allDayEvent3->setAllDay(true);
    allDayEvent3->save();
    QVERIFY(modified->wait());
}

void tst_CalendarAgendaModel::testStartEndDate()
{
    CalendarAgendaModel *model = new CalendarAgendaModel;

    QSignalSpy *updated = new QSignalSpy(model, &CalendarAgendaModel::updated);
    model->setStartDate(QDate(2021, 11, 17));
    model->setEndDate(QDate(2021, 11, 19));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 2);
    CalendarEventOccurrence *occurrence1 = model->get(0, CalendarAgendaModel::OccurrenceObjectRole).value<CalendarEventOccurrence*>();
    QCOMPARE(occurrence1->eventObject()->description(), QString::fromLatin1("event 1"));
    CalendarEventOccurrence *occurrence2 = model->get(1, CalendarAgendaModel::OccurrenceObjectRole).value<CalendarEventOccurrence*>();
    QCOMPARE(occurrence2->eventObject()->description(), QString::fromLatin1("event 2"));
    model->setStartDate(QDate(2021, 11, 17));
    model->setEndDate(QDate(2021, 11, 17));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 0);
    model->setStartDate(QDate(2021, 11, 18));
    model->setEndDate(QDate(2021, 11, 18));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 1);

    delete updated;
    delete model;
}

void tst_CalendarAgendaModel::testTimeZone()
{
    const QByteArray TZenv(qgetenv("TZ"));
    qputenv("TZ", "Europe/Paris");
    CalendarAgendaModel *model = new CalendarAgendaModel;

    QSignalSpy *updated = new QSignalSpy(model, &CalendarAgendaModel::updated);
    model->setStartDate(QDate(2021, 11, 10));
    model->setEndDate(QDate(2021, 11, 10));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 1);
    CalendarEventOccurrence *occurrence1 = model->get(0, CalendarAgendaModel::OccurrenceObjectRole).value<CalendarEventOccurrence*>();
    QCOMPARE(occurrence1->eventObject()->description(), QString::fromLatin1("event far away"));
    model->setStartDate(QDate(2021, 11, 9));
    model->setEndDate(QDate(2021, 11, 10));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 1);

    delete updated;
    delete model;

    if (TZenv.isEmpty()) {
        qunsetenv("TZ");
    } else {
        qputenv("TZ", TZenv);
    }
}

void tst_CalendarAgendaModel::testAllDays()
{
    CalendarAgendaModel *model = new CalendarAgendaModel;

    QSignalSpy *updated = new QSignalSpy(model, &CalendarAgendaModel::updated);
    model->setStartDate(QDate(2021, 10, 11));
    model->setEndDate(QDate(2021, 10, 12));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 2);
    CalendarEventOccurrence *occurrence1 = model->get(0, CalendarAgendaModel::OccurrenceObjectRole).value<CalendarEventOccurrence*>();
    QCOMPARE(occurrence1->eventObject()->description(), QString::fromLatin1("all day event 1"));
    CalendarEventOccurrence *occurrence2 = model->get(1, CalendarAgendaModel::OccurrenceObjectRole).value<CalendarEventOccurrence*>();
    QCOMPARE(occurrence2->eventObject()->description(), QString::fromLatin1("all day event 2"));
    model->setStartDate(QDate(2021, 10, 9));
    model->setEndDate(QDate(2021, 10, 9));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 0);
    model->setStartDate(QDate(2021, 10, 11));
    model->setEndDate(QDate(2021, 10, 11));
    QVERIFY(updated->wait());
    QCOMPARE(model->count(), 1);

    delete updated;
    delete model;
}

#include "tst_calendaragendamodel.moc"
QTEST_MAIN(tst_CalendarAgendaModel)
