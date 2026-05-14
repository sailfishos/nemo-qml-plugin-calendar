TEMPLATE = subdirs
SUBDIRS = \
    tst_calendarmanager \
    tst_calendarevent \
    tst_calendaragendamodel \
    tst_calendarimportmodel \
    tst_calendarsearchmodel

tests_xml.path = /opt/tests/nemo-qml-plugin-calendar-qt$${QT_MAJOR_VERSION}
tests_xml.files = tests.xml
INSTALLS += tests_xml

OTHER_FILES += tests.xml
