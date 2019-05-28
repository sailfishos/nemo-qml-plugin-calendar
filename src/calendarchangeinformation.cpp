#include "calendarchangeinformation.h"
#include <QDebug>

CalendarChangeInformation::CalendarChangeInformation(QObject *parent) :
    QObject(parent), m_pending(true)
{
}

CalendarChangeInformation::~CalendarChangeInformation()
{
}

void CalendarChangeInformation::setInformation(const QString &uniqueId, const KDateTime &recurrenceId)
{
    m_uniqueId = uniqueId;
    m_recurrenceId = recurrenceId;
    m_pending = false;

    emit uniqueIdChanged();
    emit recurrenceIdChanged();
    emit pendingChanged();
}

bool CalendarChangeInformation::pending()
{
    return m_pending;
}

QString CalendarChangeInformation::uniqueId()
{
    return m_uniqueId;
}

QString CalendarChangeInformation::recurrenceId()
{
    return m_recurrenceId.toString();
}
