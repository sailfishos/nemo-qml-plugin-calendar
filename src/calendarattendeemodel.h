#ifndef CALENDARATTENDEEMODEL_H
#define CALENDARATTENDEEMODEL_H

#include <QVector>
#include <QAbstractListModel>
#include "calendareventquery.h"

class CalendarAttendeeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_ENUMS(SectionRoles)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum AttendeeRoles {
        NameRole = Qt::UserRole,
        EmailRole,
        IsOrganizerRole,
        ParticipationRoleRole,
        ParticipationStatusRole,
        ParticipationSectionRole
    };

    enum SectionRoles {
        RequiredSection = Person::RequiredParticipant,
        OptionalSection = Person::OptionalParticipant,
        NonSection      = Person::NonParticipant,
        ChairSection    = Person::ChairParticipant,
        OrganizerSection
    };

    explicit CalendarAttendeeModel(QObject *parent = nullptr);
    virtual ~CalendarAttendeeModel();

    int rowCount(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    int count() const;

    Q_INVOKABLE void doFill(const QList<QObject*>& people);

signals:
    void countChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const override;

private:
    QVector<Person*> m_people;

};

#endif // CALENDARATTENDEEMODEL_H
