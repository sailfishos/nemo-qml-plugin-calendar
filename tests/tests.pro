TEMPLATE = subdirs
SUBDIRS = \
    tst_calendarmanager \
    tst_calendarevent \
    tst_calendaragendamodel \
    tst_calendarimportmodel

tests_xml.path = /opt/tests/nemo-qml-plugins-qt5/calendar
tests_xml.files = tests.xml
INSTALLS += tests_xml

OTHER_FILES += tests.xml
