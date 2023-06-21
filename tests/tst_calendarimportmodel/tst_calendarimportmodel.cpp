/*
 * Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>
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

#include "calendarimportmodel.h"

class tst_CalendarImportModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void testByString();
    void testError();
};

void tst_CalendarImportModel::initTestCase()
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = calendar->defaultStorage(calendar);
    QVERIFY(storage);
    QVERIFY(storage->open());

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setUid(QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A73"));
    event->setDtStart(QDateTime(QDate(2022, 6, 8), QTime(15, 7)));

    QVERIFY(calendar->addIncidence(event, storage->defaultNotebook()->uid()));
    QVERIFY(storage->save());
}

void tst_CalendarImportModel::testByString()
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = calendar->defaultStorage(calendar);
    QVERIFY(storage);
    QVERIFY(storage->open());

    CalendarImportModel *model = new CalendarImportModel;
    model->setNotebookUid(storage->defaultNotebook()->uid());

    const QString icsData =
        QStringLiteral("BEGIN:VCALENDAR\n"
                       "PRODID:-//NemoMobile.org/Nemo//NONSGML v1.0//EN\n"
                       "VERSION:2.0\n"
                       "BEGIN:VEVENT\n"
                       "DTSTART:20190607\n"
                       "UID:14B902BC-8D24-4A97-8541-63DF7FD41A73\n"
                       "SUMMARY:Test 1\n"
                       "END:VEVENT\n"
                       "BEGIN:VEVENT\n"
                       "DTSTART:20190607T143923\n"
                       "UID:14B902BC-8D24-4A97-8541-63DF7FD41A74\n"
                       "SUMMARY:Test 2\n"
                       "DESCRIPTION:ploum ploum\n"
                       "ORGANIZER;CN=Boss:MAILTO:boss@example.org\n"
                       "END:VEVENT\n"
                       "END:VCALENDAR");

    // Parse ICS data and check properties of the model.
    QSignalSpy *icsChanged = new QSignalSpy(model, &CalendarImportModel::icsStringChanged);
    QSignalSpy *countChanged = new QSignalSpy(model, &CalendarImportModel::countChanged);
    QSignalSpy *errorChanged = new QSignalSpy(model, &CalendarImportModel::errorChanged);
    QSignalSpy *hasDuplicatesChanged = new QSignalSpy(model, &CalendarImportModel::hasDuplicatesChanged);
    QSignalSpy *hasInvitationsChanged = new QSignalSpy(model, &CalendarImportModel::hasInvitationsChanged);
    model->setIcsString(icsData);
    QCOMPARE(icsChanged->count(), 1);
    QCOMPARE(countChanged->count(), 1);
    QCOMPARE(model->count(), 2);
    QCOMPARE(errorChanged->count(), 0);
    QCOMPARE(hasDuplicatesChanged->count(), 1);
    QVERIFY(model->hasDuplicates());
    QCOMPARE(hasInvitationsChanged->count(), 1);
    QVERIFY(model->hasInvitations());
    delete icsChanged;
    delete countChanged;
    delete errorChanged;
    delete hasDuplicatesChanged;
    delete hasInvitationsChanged;

    // Check that the first object of the model has the right properties
    const QModelIndex at = model->index(0, 0);
    QCOMPARE(model->data(at, int(CalendarImportModel::DisplayLabelRole)).toString(),
             QString::fromLatin1("Test 1"));
    QVERIFY(model->data(at, int(CalendarImportModel::DescriptionRole)).toString().isEmpty());
    QCOMPARE(model->data(at, int(CalendarImportModel::StartTimeRole)).toDateTime(),
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
             QDate(2019, 6, 7).startOfDay());
#else
             QDateTime(QDate(2019, 6, 7)));
#endif
    QCOMPARE(model->data(at, int(CalendarImportModel::EndTimeRole)).toDateTime(),
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
             QDate(2019, 6, 7).startOfDay());
#else
             QDateTime(QDate(2019, 6, 7)));
#endif
    QVERIFY(model->data(at, int(CalendarImportModel::AllDayRole)).toBool());
    QVERIFY(model->data(at, int(CalendarImportModel::LocationRole)).toString().isEmpty());
    QCOMPARE(model->data(at, int(CalendarImportModel::UidRole)).toString(),
             QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A73"));
    QVERIFY(model->data(at, int(CalendarImportModel::DuplicateRole)).toBool());
    QVERIFY(!model->data(at, int(CalendarImportModel::InvitationRole)).toBool());

    // Check that the second object of the model has the right properties
    const QModelIndex at2 = model->index(1, 0);
    QCOMPARE(model->data(at2, int(CalendarImportModel::DisplayLabelRole)).toString(),
             QString::fromLatin1("Test 2"));
    QCOMPARE(model->data(at2, int(CalendarImportModel::DescriptionRole)).toString(),
             QString::fromLatin1("ploum ploum"));
    QCOMPARE(model->data(at2, int(CalendarImportModel::StartTimeRole)).toDateTime(),
             QDateTime(QDate(2019, 6, 7), QTime(14, 39, 23)));
    QCOMPARE(model->data(at2, int(CalendarImportModel::EndTimeRole)).toDateTime(),
             QDateTime(QDate(2019, 6, 7), QTime(14, 39, 23)));
    QVERIFY(!model->data(at2, int(CalendarImportModel::AllDayRole)).toBool());
    QVERIFY(model->data(at2, int(CalendarImportModel::LocationRole)).toString().isEmpty());
    QCOMPARE(model->data(at2, int(CalendarImportModel::UidRole)).toString(),
             QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A74"));
    QVERIFY(!model->data(at2, int(CalendarImportModel::DuplicateRole)).toBool());
    QVERIFY(model->data(at2, int(CalendarImportModel::InvitationRole)).toBool());

    // Check that importation to the local calendar is working
    QVERIFY(model->save());

    QVERIFY(storage->load());
    const KCalendarCore::Incidence::Ptr ev1 = calendar->incidence(QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A73"));
    QVERIFY(ev1);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QCOMPARE(ev1->dtStart(), QDate(2019, 6, 7).startOfDay());
#else
    QCOMPARE(ev1->dtStart(), QDateTime(QDate(2019, 6, 7)));
#endif
    const KCalendarCore::Incidence::Ptr ev2 = calendar->incidence(QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A74"));
    QVERIFY(ev2);
    QVERIFY(!ev2->organizer().isEmpty());

    // Reimport purging invitations this time
    QVERIFY(model->save(true));

    QVERIFY(storage->close());
    calendar->close();
    QVERIFY(storage->open());
    QVERIFY(storage->load());
    QVERIFY(calendar->incidence(QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A74")));
    QVERIFY(calendar->incidence(QString::fromLatin1("14B902BC-8D24-4A97-8541-63DF7FD41A74"))->organizer().isEmpty());

    delete model;
}

void tst_CalendarImportModel::testError()
{
    CalendarImportModel *model = new CalendarImportModel;

    const QString icsData =
        QStringLiteral("BEGIN:VCALENDAR\n"
                       "PRODID:-//NemoMobile.org/Nemo//NONSGML v1.0//EN\n"
                       "VERSION:2.0\n"
                       "BEGIN:VEVENT\n"
                       "DTSTART:20190607\n"
                       "UID:14B902BC-8D24-4A97-8541-63DF7FD41A73\n"
                       "SUMMARY:Test 1");

    QSignalSpy *icsChanged = new QSignalSpy(model, &CalendarImportModel::icsStringChanged);
    QSignalSpy *countChanged = new QSignalSpy(model, &CalendarImportModel::countChanged);
    QSignalSpy *errorChanged = new QSignalSpy(model, &CalendarImportModel::errorChanged);
    model->setIcsString(icsData);
    QCOMPARE(icsChanged->count(), 1);
    QCOMPARE(countChanged->count(), 1);
    QCOMPARE(model->count(), 0);
    QCOMPARE(errorChanged->count(), 1);
    QVERIFY(model->error());

    delete icsChanged;
    delete countChanged;
    delete errorChanged;
    delete model;
}

#include "tst_calendarimportmodel.moc"
QTEST_MAIN(tst_CalendarImportModel)
