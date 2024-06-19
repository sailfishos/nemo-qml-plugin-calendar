#include <QObject>
#include <QtTest>

// mKCal
#include "extendedcalendar.h"
#include "extendedstorage.h"

// kcalendarcore
#include <KCalendarCore/CalFormat>

#include "calendarmanager.h"
#include "calendaragendamodel.h"
#include <QSignalSpy>

class tst_CalendarManager : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void test_isRangeLoaded_data();
    void test_isRangeLoaded();
    void test_addRanges_data();
    void test_addRanges();
    void test_notebookApi();
    void cleanupTestCase();

private:
    mKCal::Notebook::Ptr createNotebook();

    CalendarManager *m_manager = nullptr;
    mKCal::ExtendedCalendar::Ptr m_calendar;
    mKCal::ExtendedStorage::Ptr m_storage;
    QList<mKCal::Notebook::Ptr> m_addedNotebooks;
    QString m_defaultNotebook;
};

void tst_CalendarManager::cleanup()
{
    delete m_manager;
    m_manager = nullptr;
}

void tst_CalendarManager::test_isRangeLoaded_data()
{
    QTest::addColumn<QList<CalendarData::Range> >("loadedRanges");
    QTest::addColumn<CalendarData::Range>("testRange");
    QTest::addColumn<bool>("loaded");
    QTest::addColumn<QList<CalendarData::Range> >("correctNewRanges");

    QDate march01(2014, 3, 1);
    QDate march02(2014, 3, 2);
    QDate march03(2014, 3, 3);
    QDate march04(2014, 3, 4);
    QDate march05(2014, 3, 5);
    QDate march06(2014, 3, 6);
    QDate march18(2014, 3, 18);
    QDate march19(2014, 3, 19);
    QDate march20(2014, 3, 20);
    QDate march21(2014, 3, 21);
    QDate march29(2014, 3, 29);
    QDate march30(2014, 3, 30);
    QDate march31(2014, 3, 31);
    QDate april17(2014, 4, 17);

    // Legend:
    // * |------|: time scale
    // * x: existing loaded period
    // * n: new period
    // * l: to be loaded
    //

    // Range loaded
    //    nnnnnn
    // |-xxxxxxxxxx------------|
    //    llllll
    QList<CalendarData::Range> rangeList;
    rangeList << CalendarData::Range(march01, march01)
              << CalendarData::Range(march05, march05)
              << CalendarData::Range(march31, march31)
              << CalendarData::Range(march01, march02)
              << CalendarData::Range(march19, march20)
              << CalendarData::Range(march05, march20)
              << CalendarData::Range(march02, march30)
              << CalendarData::Range(march01, march31);

    QList<CalendarData::Range> loadedRanges;
    loadedRanges.append(CalendarData::Range(march01, march31));
    QList<CalendarData::Range> correctNewRanges;
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Range loaded") << loadedRanges << testRange << true << correctNewRanges;

    // Range not loaded
    //    nnnnnn
    // |-----------------------|
    //    llllll
    loadedRanges.clear();
    for (const CalendarData::Range &testRange : rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(testRange);
        QTest::newRow("Range not loaded") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Range not loaded 2
    //     nnnnnn
    // |-xx------xxxx--x--|
    //     llllll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march02));
    loadedRanges.append(CalendarData::Range(march30, march31));
    loadedRanges.append(CalendarData::Range(april17, april17));
    rangeList.clear();

    rangeList << CalendarData::Range(march03, march05)
              << CalendarData::Range(march03, march20)
              << CalendarData::Range(march03, march29)
              << CalendarData::Range(march05, march05)
              << CalendarData::Range(march05, march21)
              << CalendarData::Range(march05, march29);

    for (const CalendarData::Range &testRange : rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(testRange);
        QTest::newRow("Range not loaded 2") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Beginning missing
    //     nnnnnn
    // |-x-----xxxx--x--|
    //     llll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march01));
    loadedRanges.append(CalendarData::Range(march20, march31));
    loadedRanges.append(CalendarData::Range(april17, april17));
    rangeList.clear();
    rangeList << CalendarData::Range(march02, march20)
              << CalendarData::Range(march05, march20)
              << CalendarData::Range(march19, march20)
              << CalendarData::Range(march02, march30)
              << CalendarData::Range(march05, march30)
              << CalendarData::Range(march19, march30)
              << CalendarData::Range(march02, march31)
              << CalendarData::Range(march05, march31)
              << CalendarData::Range(march19, march31);

    for (const CalendarData::Range &testRange : rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(CalendarData::Range(testRange.first, march19));
        QTest::newRow("Beginning missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

    // End missing
    //      nnnnnn
    // |--xxxx--------|
    //        llll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march19));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march20)
              << CalendarData::Range(march02, march20)
              << CalendarData::Range(march05, march20)
              << CalendarData::Range(march19, march20)
              << CalendarData::Range(march20, march20)
              << CalendarData::Range(march01, march30)
              << CalendarData::Range(march02, march30)
              << CalendarData::Range(march05, march30)
              << CalendarData::Range(march19, march30)
              << CalendarData::Range(march20, march30);
    for (const CalendarData::Range &testRange : rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(CalendarData::Range(march20, testRange.second));
        QTest::newRow("End missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Middle missing
    //      nnnnnn
    // |--xxxx---xxx-----|
    //        lll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march05));
    loadedRanges.append(CalendarData::Range(march20, march31));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march20)
              << CalendarData::Range(march01, march30)
              << CalendarData::Range(march01, march31)
              << CalendarData::Range(march02, march20)
              << CalendarData::Range(march02, march30)
              << CalendarData::Range(march02, march31)
              << CalendarData::Range(march05, march20)
              << CalendarData::Range(march05, march30)
              << CalendarData::Range(march05, march31);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march06, march19));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Middle missing") << loadedRanges << testRange << false << correctNewRanges;

    // Two periods missing
    //      nnnnnnnnnn
    // |--xxxx---xxx-----|
    //        lll   ll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march02));
    loadedRanges.append(CalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march30)
              << CalendarData::Range(march02, march30)
              << CalendarData::Range(march03, march30);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march03, march18));
    correctNewRanges.append(CalendarData::Range(march21, march30));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Two periods missing 1") << loadedRanges << testRange << false << correctNewRanges;

    // Two periods missing
    //   nnnnnnnnnnnn
    // |----xxxx---xxxx----|
    //   lll    lll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march19, march20));
    loadedRanges.append(CalendarData::Range(march30, march31));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march30)
              << CalendarData::Range(march01, march31)
              << CalendarData::Range(march01, march29);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march01, march18));
    correctNewRanges.append(CalendarData::Range(march21, march29));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Two periods missing 2") << loadedRanges << testRange << false << correctNewRanges;

    // Two periods missing
    //   nnnnnnnnnn
    // |----xxxx-------|
    //   lll    lll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march29);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march01, march18));
    correctNewRanges.append(CalendarData::Range(march21, march29));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Two periods missing 3") << loadedRanges << testRange << false << correctNewRanges;

    // Two periods missing
    //     nnnnnn
    // |----xxxx-------|
    //     l    l
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << CalendarData::Range(march18, march21);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march18, march18));
    correctNewRanges.append(CalendarData::Range(march21, march21));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Two periods missing 4") << loadedRanges << testRange << false << correctNewRanges;

    // Three periods missing
    //   nnnnnnnnnnnnnnnn
    // |----xxxx---xxxx----|
    //   lll    lll    ll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march05, march05));
    loadedRanges.append(CalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march31);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march01, march04));
    correctNewRanges.append(CalendarData::Range(march06, march18));
    correctNewRanges.append(CalendarData::Range(march21, march31));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Three periods missing 1") << loadedRanges << testRange << false << correctNewRanges;

    // Three periods missing
    //   nnnnnnnnnnnnnnnnnnn
    // |-xx---xxxx---xxxx--xx--|
    //     lll    lll    ll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march02));
    loadedRanges.append(CalendarData::Range(march05, march05));
    loadedRanges.append(CalendarData::Range(march19, march20));
    loadedRanges.append(CalendarData::Range(march30, march31));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, march31);
    rangeList << CalendarData::Range(march02, march31);
    rangeList << CalendarData::Range(march03, march31);
    rangeList << CalendarData::Range(march01, march30);
    rangeList << CalendarData::Range(march02, march30);
    rangeList << CalendarData::Range(march03, march30);
    rangeList << CalendarData::Range(march01, march29);
    rangeList << CalendarData::Range(march02, march29);
    rangeList << CalendarData::Range(march03, march29);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march03, march04));
    correctNewRanges.append(CalendarData::Range(march06, march18));
    correctNewRanges.append(CalendarData::Range(march21, march29));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Three periods missing 2") << loadedRanges << testRange << false << correctNewRanges;

    // Five periods missing
    //   nnnnnnnnnnnnnnnnnnnnnnnnnnnn
    // |-xx---xxxx---xxxx--xx--x---x--|
    //     lll    lll    ll  ll lll
    loadedRanges.clear();
    loadedRanges.append(CalendarData::Range(march01, march01));
    loadedRanges.append(CalendarData::Range(march03, march03));
    loadedRanges.append(CalendarData::Range(march05, march05));
    loadedRanges.append(CalendarData::Range(march19, march20));
    loadedRanges.append(CalendarData::Range(march30, march30));
    rangeList.clear();
    rangeList << CalendarData::Range(march01, april17);
    correctNewRanges.clear();
    correctNewRanges.append(CalendarData::Range(march02, march02));
    correctNewRanges.append(CalendarData::Range(march04, march04));
    correctNewRanges.append(CalendarData::Range(march06, march18));
    correctNewRanges.append(CalendarData::Range(march21, march29));
    correctNewRanges.append(CalendarData::Range(march31, april17));
    for (const CalendarData::Range &testRange : rangeList)
        QTest::newRow("Five periods missing") << loadedRanges << testRange << false << correctNewRanges;
}

void tst_CalendarManager::test_isRangeLoaded()
{
    QFETCH(QList<CalendarData::Range>, loadedRanges);
    QFETCH(CalendarData::Range, testRange);
    QFETCH(bool, loaded);
    QFETCH(QList<CalendarData::Range>, correctNewRanges);

    m_manager = new CalendarManager;
    m_manager->m_loadedRanges = loadedRanges;
    QList<CalendarData::Range> newRanges;
    bool result = m_manager->isRangeLoaded(testRange, &newRanges);

    QCOMPARE(result, loaded);
    QCOMPARE(newRanges.count(), correctNewRanges.count());

    for (const CalendarData::Range &range : newRanges)
        QVERIFY(correctNewRanges.contains(range));
}

void tst_CalendarManager::test_addRanges_data()
{
    QDate march01(2014, 3, 1);
    QDate march02(2014, 3, 2);
    QDate march03(2014, 3, 3);
    QDate march04(2014, 3, 4);
    QDate march05(2014, 3, 5);
    QDate march06(2014, 3, 6);
    QDate march18(2014, 3, 18);
    QDate march19(2014, 3, 19);
    QDate march30(2014, 3, 30);
    QDate march31(2014, 3, 31);

    QTest::addColumn<QList<CalendarData::Range> >("oldRanges");
    QTest::addColumn<QList<CalendarData::Range> >("newRanges");
    QTest::addColumn<QList<CalendarData::Range> >("combinedRanges");

    QList<CalendarData::Range> oldRanges;
    QList<CalendarData::Range> newRanges;
    QList<CalendarData::Range> combinedRanges;

    QTest::newRow("Empty parameters") << oldRanges << newRanges << combinedRanges;

    oldRanges << CalendarData::Range(march01, march02);
    combinedRanges << CalendarData::Range(march01, march02);
    QTest::newRow("Empty newRange parameter, 1 old range") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    newRanges << CalendarData::Range(march01, march02);
    QTest::newRow("Empty oldRanges parameter, 1 new range") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march01, march01)
              << CalendarData::Range(march03, march03)
              << CalendarData::Range(march05, march06)
              << CalendarData::Range(march18, march19);
    newRanges.clear();
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01)
                   << CalendarData::Range(march03, march03)
                   << CalendarData::Range(march05, march06)
                   << CalendarData::Range(march18, march19);
    QTest::newRow("Empty newRange parameter, 4 sorted old ranges") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march01, march01)
              << CalendarData::Range(march18, march19)
              << CalendarData::Range(march03, march03)
              << CalendarData::Range(march05, march06);
    newRanges.clear();
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01)
                   << CalendarData::Range(march03, march03)
                   << CalendarData::Range(march05, march06)
                   << CalendarData::Range(march18, march19);
    QTest::newRow("Empty newRange parameter, 4 unsorted old ranges") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    newRanges.clear();
    newRanges << CalendarData::Range(march01, march01)
              << CalendarData::Range(march03, march03)
              << CalendarData::Range(march05, march06)
              << CalendarData::Range(march18, march19);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01)
                   << CalendarData::Range(march03, march03)
                   << CalendarData::Range(march05, march06)
                   << CalendarData::Range(march18, march19);
    QTest::newRow("Empty oldRanges parameter, 4 sorted new ranges") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    newRanges.clear();
    newRanges << CalendarData::Range(march01, march01)
              << CalendarData::Range(march18, march19)
              << CalendarData::Range(march03, march03)
              << CalendarData::Range(march05, march06);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01)
                   << CalendarData::Range(march03, march03)
                   << CalendarData::Range(march05, march06)
                   << CalendarData::Range(march18, march19);
    QTest::newRow("Empty oldRanges parameter, 4 unsorted new ranges") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march30, march31);
    newRanges.clear();
    newRanges << CalendarData::Range(march01, march01);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01)
                   << CalendarData::Range(march30, march31);
    QTest::newRow("Add one range") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march01, march02);
    newRanges.clear();
    newRanges << CalendarData::Range(march18, march19);
    newRanges << CalendarData::Range(march30, march31);
    newRanges << CalendarData::Range(march04, march05);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march02)
                   << CalendarData::Range(march04, march05)
                   << CalendarData::Range(march18, march19)
                   << CalendarData::Range(march30, march31);
    QTest::newRow("Add two unsorted ranges") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march01, march02);
    newRanges.clear();
    newRanges << CalendarData::Range(march04, march05)
              << CalendarData::Range(march18, march18)
              << CalendarData::Range(march03, march03)
              << CalendarData::Range(march19, march30);
    combinedRanges.clear();
    combinedRanges <<  CalendarData::Range(march01, march05)
                    << CalendarData::Range(march18, march30);
    QTest::newRow("Add four unsorted, ranges, combines into two") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march02, march02)
              << CalendarData::Range(march04, march04)
              << CalendarData::Range(march06, march06);
    newRanges.clear();
    newRanges << CalendarData::Range(march03, march03)
              << CalendarData::Range(march05, march05)
              << CalendarData::Range(march01, march01);
    combinedRanges.clear();
    combinedRanges <<  CalendarData::Range(march01, march06);
    QTest::newRow("Add three ranges to three existing, combines into one") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march02, march05)
              << CalendarData::Range(march18, march30);
    newRanges.clear();
    newRanges << CalendarData::Range(march19, march31)
              << CalendarData::Range(march01, march03)
              << CalendarData::Range(march06, march06)
              << CalendarData::Range(march31, march31);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march06)
                   << CalendarData::Range(march18, march31);
    QTest::newRow("Add overlapping ranges, combines into two") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    newRanges.clear();
    newRanges << CalendarData::Range(march01, march01)
              << CalendarData::Range(march01, march01)
              << CalendarData::Range(march01, march01)
              << CalendarData::Range(march01, march01);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01);
    QTest::newRow("Add one range four times, combines into one") << oldRanges << newRanges << combinedRanges;

    oldRanges.clear();
    oldRanges << CalendarData::Range(march01, march01);
    newRanges.clear();
    newRanges << CalendarData::Range(march01, march01)
              << CalendarData::Range(march01, march01)
              << CalendarData::Range(march01, march01)
              << CalendarData::Range(march01, march01);
    combinedRanges.clear();
    combinedRanges << CalendarData::Range(march01, march01);
    QTest::newRow("Add existing range four times") << oldRanges << newRanges << combinedRanges;
}

void tst_CalendarManager::test_addRanges()
{
    QFETCH(QList<CalendarData::Range>, oldRanges);
    QFETCH(QList<CalendarData::Range>, newRanges);
    QFETCH(QList<CalendarData::Range>, combinedRanges);

    m_manager = new CalendarManager;
    QList<CalendarData::Range> result = m_manager->addRanges(oldRanges, newRanges);
    QVERIFY(result == combinedRanges);
}

mKCal::Notebook::Ptr tst_CalendarManager::createNotebook()
{
    return mKCal::Notebook::Ptr(new mKCal::Notebook(KCalendarCore::CalFormat::createUniqueId(),
                                                    "",
                                                    QLatin1String(""),
                                                    "#110000",
                                                    false, // Not shared.
                                                    true, // Is master.
                                                    false, // Not synced to Ovi.
                                                    false, // Writable.
                                                    true)); // Visible.
}

void tst_CalendarManager::test_notebookApi()
{
    m_manager = new CalendarManager;
    QSignalSpy notebookSpy(m_manager, SIGNAL(notebooksChanged(QList<CalendarData::Notebook>)));

    // Wait for the manager to open the calendar database, etc
    QTRY_COMPARE(notebookSpy.count(), 1);
    int notebookCount = m_manager->notebooks().count();
    m_defaultNotebook = m_manager->defaultNotebook();

    m_calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = m_calendar->defaultStorage(m_calendar);
    m_storage->open();

    m_addedNotebooks << createNotebook();
    QVERIFY(m_storage->addNotebook(m_addedNotebooks.last()));
    m_storage->save();
    QTRY_COMPARE(notebookSpy.count(), 2);
    QCOMPARE(m_manager->notebooks().count(), notebookCount + 1);

    m_addedNotebooks << createNotebook();
    QVERIFY(m_storage->addNotebook(m_addedNotebooks.last()));
    m_storage->save();
    QTRY_COMPARE(notebookSpy.count(), 3);
    QCOMPARE(m_manager->notebooks().count(), notebookCount + 2);

    QStringList uidList;
    for (const CalendarData::Notebook &notebook : m_manager->notebooks())
        uidList << notebook.uid;

    for (const mKCal::Notebook::Ptr &notebookPtr : m_addedNotebooks)
        QVERIFY(uidList.contains(notebookPtr->uid()));

    QSignalSpy defaultNotebookSpy(m_manager, SIGNAL(defaultNotebookChanged(QString)));
    notebookSpy.clear();
    m_manager->setDefaultNotebook(m_addedNotebooks.first()->uid());
    QTRY_VERIFY(!notebookSpy.empty());
    QCOMPARE(m_manager->defaultNotebook(), m_addedNotebooks.first()->uid());
    QCOMPARE(defaultNotebookSpy.count(), 1);

    notebookSpy.clear();
    m_manager->setDefaultNotebook(m_addedNotebooks.last()->uid());
    QTRY_VERIFY(!notebookSpy.empty());
    QCOMPARE(m_manager->defaultNotebook(), m_addedNotebooks.last()->uid());
    QCOMPARE(defaultNotebookSpy.count(), 2);

    QSignalSpy dataUpdatedSpy(m_manager, &CalendarManager::dataUpdated);
    CalendarAgendaModel agenda;
    agenda.setStartDate(QDate(2023, 6, 8));
    m_manager->scheduleAgendaRefresh(&agenda);
    QTRY_VERIFY(!dataUpdatedSpy.isEmpty());
    dataUpdatedSpy.clear();
    QSignalSpy excludedNotebooksSpy(m_manager, &CalendarManager::excludedNotebooksChanged);
    m_manager->excludeNotebook(m_addedNotebooks.first()->uid(), true);
    QTRY_VERIFY(!dataUpdatedSpy.isEmpty());
    QVERIFY(!excludedNotebooksSpy.isEmpty());
    QCOMPARE(excludedNotebooksSpy.count(), 1);
    QCOMPARE(m_manager->excludedNotebooks(), QStringList() << m_addedNotebooks.first()->uid());
    dataUpdatedSpy.clear();
    excludedNotebooksSpy.clear();
    m_manager->excludeNotebook(m_addedNotebooks.first()->uid(), false);
    QTRY_VERIFY(!dataUpdatedSpy.isEmpty());
    QVERIFY(!excludedNotebooksSpy.isEmpty());
    QCOMPARE(excludedNotebooksSpy.count(), 1);
    QVERIFY(m_manager->excludedNotebooks().isEmpty());
}

void tst_CalendarManager::cleanupTestCase()
{
    m_manager = new CalendarManager;
    m_manager->setDefaultNotebook(m_defaultNotebook);
    delete m_manager;
    m_manager = nullptr;

    if (m_storage) {
        for (const mKCal::Notebook::Ptr &notebookPtr : m_addedNotebooks)
            m_storage->deleteNotebook(notebookPtr);
        m_storage->save();
        m_storage->close();
        m_storage.clear();
    }
    m_calendar.clear();
}

#include "tst_calendarmanager.moc"
QTEST_MAIN(tst_CalendarManager)
