#include <QObject>
#include <QtTest>
#include <QQmlEngine>
#include <QSignalSpy>

#include "calendarapi.h"
#include "calendarmanager.h"
#include "calendarsearchmodel.h"

#include "plugin.cpp"

class tst_CalendarSearchModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void test_searchString();

private:
    QQmlEngine *engine;
    CalendarApi *calendarApi;
    QSet<QString> mSavedEvents;
};

void tst_CalendarSearchModel::initTestCase()
{
    // Create plugin, it shuts down the DB in proper order
    engine = new QQmlEngine(this);
    NemoCalendarPlugin* plugin = new NemoCalendarPlugin();
    plugin->initializeEngine(engine, "foobar");
    calendarApi = new CalendarApi(this);

    // Use test plugins, instead of mKCal ones.
    if (qgetenv("MKCAL_PLUGIN_DIR").isEmpty())
        qputenv("MKCAL_PLUGIN_DIR", "plugins");

    QSignalSpy modified(CalendarManager::instance(),
                        &CalendarManager::storageModified);
    CalendarEventModification *event = calendarApi->createNewEvent();
    QVERIFY(event != 0);
    event->setStartTime(QDateTime(QDate(2023,5,22), QTime(15,31)), Qt::LocalTime);
    event->setDisplayLabel(QString::fromLatin1("Summary with string 'azerty' %plop."));
    event->save();
    QVERIFY(modified.wait());
}

void tst_CalendarSearchModel::test_searchString()
{
    CalendarSearchModel *model = new CalendarSearchModel(this);
    QSignalSpy searchStringSet(model, &CalendarSearchModel::searchStringChanged);
    QSignalSpy identifiersSet(model, &CalendarSearchModel::identifiersChanged);
    QSignalSpy countSet(model, &CalendarSearchModel::countChanged);

    QVERIFY(model->identifiers().isEmpty());
    model->setSearchString(QString::fromLatin1("azerty' %p"));
    QCOMPARE(searchStringSet.count(), 1);
    QVERIFY(identifiersSet.wait());
    QCOMPARE(model->identifiers().length(), 1);
    QVERIFY(countSet.wait());
    QCOMPARE(model->count(), 1);
}


#include "tst_calendarsearchmodel.moc"
QTEST_MAIN(tst_CalendarSearchModel)
