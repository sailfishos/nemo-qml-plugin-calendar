#include <QObject>
#include <QtTest>

// mKCal
#include "extendedcalendar.h"
#include "extendedstorage.h"

// kcalendarcore
#include <KCalendarCore/CalFormat>

#include "calendarmanager.h"
#include <QSignalSpy>

class tst_CalendarManager : public QObject
{
    Q_OBJECT

private slots:
    void test_isRangeLoaded_data();
    void test_isRangeLoaded();
    void test_addRanges_data();
    void test_addRanges();
    void test_notebookApi();
    void cleanupTestCase();

private:
    mKCal::Notebook::Ptr createNotebook();

    CalendarManager mManager;
    mKCal::ExtendedCalendar::Ptr mCalendar;
    mKCal::ExtendedStorage::Ptr mStorage;
    QList<mKCal::Notebook::Ptr> mAddedNotebooks;
    QString mDefaultNotebook;
};

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
    foreach (const CalendarData::Range &testRange, rangeList)
        QTest::newRow("Range loaded") << loadedRanges << testRange << true << correctNewRanges;

    // Range not loaded
    //    nnnnnn
    // |-----------------------|
    //    llllll
    loadedRanges.clear();
    foreach (const CalendarData::Range &testRange, rangeList) {
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

    foreach (const CalendarData::Range &testRange, rangeList) {
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

    foreach (const CalendarData::Range &testRange, rangeList) {
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
    foreach (const CalendarData::Range &testRange, rangeList) {
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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Middle missing") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 2") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 3") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 4") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Three periods missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Three periods missing 2") << loadedRanges << testRange << false << correctNewRanges;
    }

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
    foreach (const CalendarData::Range &testRange, rangeList) {
        QTest::newRow("Five periods missing") << loadedRanges << testRange << false << correctNewRanges;
    }
}

void tst_CalendarManager::test_isRangeLoaded()
{
    QFETCH(QList<CalendarData::Range>, loadedRanges);
    QFETCH(CalendarData::Range, testRange);
    QFETCH(bool, loaded);
    QFETCH(QList<CalendarData::Range>, correctNewRanges);

    mManager.mLoadedRanges = loadedRanges;
    QList<CalendarData::Range> newRanges;
    bool result = mManager.isRangeLoaded(testRange, &newRanges);

    QCOMPARE(result, loaded);
    QCOMPARE(newRanges.count(), correctNewRanges.count());

    foreach (const CalendarData::Range &range, newRanges)
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
    QList<CalendarData::Range> result = mManager.addRanges(oldRanges, newRanges);
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
    CalendarManager *manager = CalendarManager::instance();
    QSignalSpy notebookSpy(manager, SIGNAL(notebooksChanged(QList<CalendarData::Notebook>)));
    QSignalSpy defaultNotebookSpy(manager, SIGNAL(defaultNotebookChanged(QString)));

    // Wait for the manager to open the calendar database, etc
    for (int i = 0; i < 30; i++) {
        if (notebookSpy.count() > 0)
            break;

        QTest::qWait(100);
    }
    QCOMPARE(notebookSpy.count(), 1);
    QCOMPARE(defaultNotebookSpy.count(), 1);
    int notebookCount = manager->notebooks().count();
    mDefaultNotebook = manager->defaultNotebook();

    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mStorage = mCalendar->defaultStorage(mCalendar);
    mStorage->open();

    mAddedNotebooks << createNotebook();
    QVERIFY(mStorage->addNotebook(mAddedNotebooks.last()));
    mStorage->save();
    for (int i = 0; i < 30; i++) {
        if (notebookSpy.count() > 1)
            break;

        QTest::qWait(100);
    }
    QCOMPARE(notebookSpy.count(), 2);
    QCOMPARE(manager->notebooks().count(), notebookCount + 1);

    mAddedNotebooks << createNotebook();
    QVERIFY(mStorage->addNotebook(mAddedNotebooks.last()));
    mStorage->save();
    for (int i = 0; i < 30; i++) {
        if (notebookSpy.count() > 2)
            break;

        QTest::qWait(100);
    }
    QCOMPARE(notebookSpy.count(), 3);
    QCOMPARE(manager->notebooks().count(), notebookCount + 2);

    QStringList uidList;
    foreach (const CalendarData::Notebook &notebook, manager->notebooks())
        uidList << notebook.uid;

    foreach (const mKCal::Notebook::Ptr &notebookPtr, mAddedNotebooks)
        QVERIFY(uidList.contains(notebookPtr->uid()));

    manager->setDefaultNotebook(mAddedNotebooks.first()->uid());
    for (int i = 0; i < 30; i++) {
        if (notebookSpy.count() > 3)
            break;

        QTest::qWait(100);
    }
    QCOMPARE(manager->defaultNotebook(), mAddedNotebooks.first()->uid());
    QCOMPARE(defaultNotebookSpy.count(), 2);

    manager->setDefaultNotebook(mAddedNotebooks.last()->uid());
    for (int i = 0; i < 30; i++) {
        if (notebookSpy.count() > 4)
            break;

        QTest::qWait(100);
    }
    QCOMPARE(manager->defaultNotebook(), mAddedNotebooks.last()->uid());
    QCOMPARE(defaultNotebookSpy.count(), 3);
}

void tst_CalendarManager::cleanupTestCase()
{
    CalendarManager::instance()->setDefaultNotebook(mDefaultNotebook);
    delete CalendarManager::instance();
    foreach (const mKCal::Notebook::Ptr &notebookPtr, mAddedNotebooks)
        mStorage->deleteNotebook(notebookPtr);

    mStorage->save();
    mStorage->close();
    mStorage.clear();
    mCalendar.clear();
}

#include "tst_calendarmanager.moc"
QTEST_MAIN(tst_CalendarManager)
