#ifndef NEMOCALENDARDATA_H
#define NEMOCALENDARDATA_H

#include <QString>
#include <QUrl>

// KCalCore
#include <attendee.h>
#include <KDateTime>

#include "calendarevent.h"

namespace CalendarData {

struct EventOccurrence {
    QString eventUid;
    KDateTime recurrenceId;
    QDateTime startTime;
    QDateTime endTime;

    QString getId() const
    {
        return QString("%1-%2").arg(eventUid).arg(startTime.toMSecsSinceEpoch());
    }
};

struct Event {
    QString displayLabel;
    QString description;
    KDateTime startTime;
    KDateTime endTime;
    bool allDay = false;
    bool readOnly = false;
    bool rsvp = false;
    bool externalInvitation = false;
    CalendarEvent::Recur recur;
    QDate recurEndDate;
    int reminder; // seconds; 15 minutes before event = +900, at time of event = 0, no reminder = negative value.
    QString uniqueId;
    KDateTime recurrenceId;
    QString location;
    CalendarEvent::Secrecy secrecy;
    QString calendarUid;
    CalendarEvent::Response ownerStatus = CalendarEvent::ResponseUnspecified;

    bool operator==(const Event& other) const
    {
        return uniqueId == other.uniqueId;
    }

    bool isValid() const
    {
        return !uniqueId.isEmpty();
    }
};

struct Notebook {
    QString name;
    QString uid;
    QString description;
    QString color;
    QString emailAddress;
    int accountId;
    QUrl accountIcon;
    bool isDefault;
    bool readOnly;
    bool localCalendar;
    bool excluded;

    Notebook() : accountId(0), isDefault(false), readOnly(false), localCalendar(false), excluded(false) { }

    bool operator==(const Notebook other) const
    {
        return uid == other.uid && name == other.name && description == other.description
                && color == other.color && emailAddress == other.emailAddress
                && accountId == other.accountId && accountIcon == other.accountIcon
                && isDefault == other.isDefault && readOnly == other.readOnly && localCalendar == other.localCalendar
                && excluded == other.excluded;
    }

    bool operator!=(const Notebook other) const
    {
        return !operator==(other);
    }
};

typedef QPair<QDate,QDate> Range;

struct Attendee {
    bool isOrganizer = false;
    QString name;
    QString email;
    KCalCore::Attendee::Role participationRole = KCalCore::Attendee::OptParticipant;
    KCalCore::Attendee::PartStat status = KCalCore::Attendee::None;

    bool operator==(const Attendee &other) const {
        return isOrganizer == other.isOrganizer
                && name == other.name
                && email == other.email
                && participationRole == other.participationRole
                && status == other.status;
    }
};

struct EmailContact {
    EmailContact(const QString &aName, const QString &aEmail)
        : name(aName), email(aEmail) {}
    QString name;
    QString email;
};

}
#endif // NEMOCALENDARDATA_H
