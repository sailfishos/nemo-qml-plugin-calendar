#include "calendareventmodification.h"
#include "calendarmanager.h"

#include <QDebug>

CalendarEventModification::CalendarEventModification(CalendarData::Event data, QObject *parent)
    : QObject(parent), m_event(data), m_attendeesSet(false)
{
}

CalendarEventModification::CalendarEventModification(QObject *parent)
    : QObject(parent), m_attendeesSet(false)
{
    m_event.recur = CalendarEvent::RecurOnce;
    m_event.reminder = -1; // ReminderNone
    m_event.allDay = false;
    m_event.readOnly = false;
    m_event.startTime = KDateTime(QDateTime(), KDateTime::LocalZone);
    m_event.endTime = KDateTime(QDateTime(), KDateTime::LocalZone);
}

CalendarEventModification::~CalendarEventModification()
{
}

QString CalendarEventModification::displayLabel() const
{
    return m_event.displayLabel;
}

void CalendarEventModification::setDisplayLabel(const QString &displayLabel)
{
    if (m_event.displayLabel != displayLabel) {
        m_event.displayLabel = displayLabel;
        emit displayLabelChanged();
    }
}

QString CalendarEventModification::description() const
{
    return m_event.description;
}

void CalendarEventModification::setDescription(const QString &description)
{
    if (m_event.description != description) {
        m_event.description = description;
        emit descriptionChanged();
    }
}

QDateTime CalendarEventModification::startTime() const
{
    return m_event.startTime.dateTime();
}

void CalendarEventModification::setStartTime(const QDateTime &startTime, int spec)
{
    KDateTime::SpecType kSpec = KDateTime::LocalZone;
    if (spec == CalendarEvent::SpecClockTime) {
        kSpec = KDateTime::ClockTime;
    }

    KDateTime time(startTime, kSpec);
    if (m_event.startTime != time) {
        m_event.startTime = time;
        emit startTimeChanged();
    }
}

QDateTime CalendarEventModification::endTime() const
{
    return m_event.endTime.dateTime();
}

void CalendarEventModification::setEndTime(const QDateTime &endTime, int spec)
{
    KDateTime::SpecType kSpec = KDateTime::LocalZone;
    if (spec == CalendarEvent::SpecClockTime) {
        kSpec = KDateTime::ClockTime;
    }
    KDateTime time(endTime, kSpec);

    if (m_event.endTime != time) {
        m_event.endTime = time;
        emit endTimeChanged();
    }
}

bool CalendarEventModification::allDay() const
{
    return m_event.allDay;
}

void CalendarEventModification::setAllDay(bool allDay)
{
    if (m_event.allDay != allDay) {
        m_event.allDay = allDay;
        emit allDayChanged();
    }
}

CalendarEvent::Recur CalendarEventModification::recur() const
{
    return m_event.recur;
}

void CalendarEventModification::setRecur(CalendarEvent::Recur recur)
{
    if (m_event.recur != recur) {
        m_event.recur = recur;
        emit recurChanged();
    }
}

QDateTime CalendarEventModification::recurEndDate() const
{
    return QDateTime(m_event.recurEndDate);
}

bool CalendarEventModification::hasRecurEndDate() const
{
    return m_event.recurEndDate.isValid();
}

void CalendarEventModification::setRecurEndDate(const QDateTime &dateTime)
{
    bool wasValid = m_event.recurEndDate.isValid();
    QDate date = dateTime.date();

    if (m_event.recurEndDate != date) {
        m_event.recurEndDate = date;
        emit recurEndDateChanged();

        if (date.isValid() != wasValid) {
            emit hasRecurEndDateChanged();
        }
    }
}

void CalendarEventModification::unsetRecurEndDate()
{
    setRecurEndDate(QDateTime());
}

QString CalendarEventModification::recurrenceIdString() const
{
    if (m_event.recurrenceId.isValid()) {
        return m_event.recurrenceId.toString();
    } else {
        return QString();
    }
}

int CalendarEventModification::reminder() const
{
    return m_event.reminder;
}

void CalendarEventModification::setReminder(int seconds)
{
    if (seconds != m_event.reminder) {
        m_event.reminder = seconds;
        emit reminderChanged();
    }
}

QString CalendarEventModification::location() const
{
    return m_event.location;
}

void CalendarEventModification::setLocation(const QString &newLocation)
{
    if (newLocation != m_event.location) {
        m_event.location = newLocation;
        emit locationChanged();
    }
}

QString CalendarEventModification::calendarUid() const
{
    return m_event.calendarUid;
}

void CalendarEventModification::setCalendarUid(const QString &uid)
{
    if (m_event.calendarUid != uid) {
        m_event.calendarUid = uid;
        emit calendarUidChanged();
    }
}

void CalendarEventModification::setAttendees(CalendarContactModel *required, CalendarContactModel *optional)
{
    if (!required || !optional) {
        qWarning() << "Missing attendeeList";
        return;
    }

    m_attendeesSet = true;
    m_requiredAttendees = required->getList();
    m_optionalAttendees = optional->getList();
}

void CalendarEventModification::save()
{
    CalendarManager::instance()->saveModification(m_event, m_attendeesSet,
                                                  m_requiredAttendees, m_optionalAttendees);
}

CalendarChangeInformation *
CalendarEventModification::replaceOccurrence(CalendarEventOccurrence *occurrence)
{
    return CalendarManager::instance()->replaceOccurrence(m_event, occurrence, m_attendeesSet,
                                                          m_requiredAttendees, m_optionalAttendees);
}
