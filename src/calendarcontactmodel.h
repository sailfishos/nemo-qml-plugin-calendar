#ifndef CALENDARCONTACTMODEL_H
#define CALENDARCONTACTMODEL_H

#include <QAbstractListModel>

#include "calendardata.h"

class CalendarContactModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum ContactRoles {
        NameRole = Qt::UserRole,
        EmailRole
    };

    explicit CalendarContactModel(QObject *parent = nullptr);
    virtual ~CalendarContactModel();

    int rowCount(const QModelIndex &index) const;
    QVariant data(const QModelIndex &index, int role) const;

    int count() const;

    Q_INVOKABLE void add(const QString &name, const QString &email);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE bool hasEmail(const QString &email) const;
    Q_INVOKABLE QString name(int index) const;
    Q_INVOKABLE QString email(int index) const;

    QList<CalendarData::EmailContact> getList();

signals:
    void countChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private:
    QList<CalendarData::EmailContact> m_contacts;
};

#endif
