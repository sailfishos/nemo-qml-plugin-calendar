#include "calendarattendeemodel.h"
#include <algorithm>

static CalendarAttendeeModel::SectionRoles sectionRole(const Person *person)
{
    return person->isOrganizer()
            ? CalendarAttendeeModel::OrganizerSection
            : CalendarAttendeeModel::SectionRoles(qBound<int>(0, person->participationRole(), 3));
}

CalendarAttendeeModel::CalendarAttendeeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

CalendarAttendeeModel::~CalendarAttendeeModel()
{
    qDeleteAll(m_people);
}

int CalendarAttendeeModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return m_people.size();
}

QVariant CalendarAttendeeModel::data(const QModelIndex &index, int role) const
{
    QVariant ret;
    if (!index.isValid() || index.row() < 0 || index.row() >= m_people.size())
        return ret;

    const Person* person = m_people[index.row()];

    switch (role) {
    case NameRole:
        ret = person->name();
        break;
    case EmailRole:
        ret = person->email();
        break;
    case IsOrganizerRole:
        ret = person->isOrganizer();
        break;
    case ParticipationRoleRole:
        ret = person->participationRole();
        break;
    case ParticipationStatusRole:
        ret = person->participationStatus();
        break;
    case ParticipationSectionRole:
        ret = sectionRole(person);
        break;
    }
    return ret;
}

int CalendarAttendeeModel::count() const
{
    return m_people.size();
}

void CalendarAttendeeModel::doFill(const QList<QObject*> &people)
{
    const int rulesTable[] = {
        1,  // Required
        3,  // Optional
        4,  // Non,
        2,  // Chair,
        0,  // Organizer
    };

    auto groupingAndSorting = [&rulesTable](const Person* lhs, const Person* rhs) -> bool {
        const SectionRoles lhsRole = sectionRole(lhs);
        const SectionRoles rhsRole = sectionRole(rhs);

        return lhsRole != rhsRole
                ? rulesTable[lhsRole] < rulesTable[rhsRole]
                : QString::localeAwareCompare(lhs->name(), rhs->name()) < 0;
    };

    beginResetModel();
    for (auto it : people) {
        auto person = qobject_cast<Person*>(it);
        Q_ASSERT_X(person != nullptr,
                   "qobject cast to class Person",
                   "The container doesn't contain an instance of Person class");
        m_people.push_back(new Person(*person));
    }
    std::sort(m_people.begin(), m_people.end(), groupingAndSorting);
    endResetModel();

    if (people.size() != m_people.size()) {
        emit countChanged();
    }
}

QHash<int, QByteArray> CalendarAttendeeModel::roleNames() const
{
    static QHash<int, QByteArray> roleNames {
        { NameRole,                 "name" },
        { EmailRole,                "email" },
        { IsOrganizerRole,          "isOrganizer" },
        { ParticipationRoleRole,    "participationRole" },
        { ParticipationStatusRole,  "participationStatus" },
        { ParticipationSectionRole, "participationSection" }
    };
    return roleNames;
}
