#include "calendarcontactmodel.h"

CalendarContactModel::CalendarContactModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

CalendarContactModel::~CalendarContactModel()
{
}

int CalendarContactModel::rowCount(const QModelIndex &index) const
{
    if (index.isValid()) {
        return 0;
    }
    return m_contacts.length();
}

QVariant CalendarContactModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contacts.length())
        return QVariant();

    switch(role) {
    case NameRole:
        return m_contacts.at(index.row()).name;
    case EmailRole:
        return m_contacts.at(index.row()).email;
    default:
        return QVariant();
    }
}

int CalendarContactModel::count() const
{
    return m_contacts.length();
}

void CalendarContactModel::remove(int index)
{
    if (index < 0 || index >= m_contacts.length()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    m_contacts.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

bool CalendarContactModel::hasEmail(const QString &email) const
{
    for (auto &contact : m_contacts) {
        if (contact.email == email) {
            return true;
        }
    }

    return false;
}

QString CalendarContactModel::name(int index) const
{
    if (index < 0 || index >= m_contacts.length()) {
        return QString();
    }

    return m_contacts.at(index).name;
}

QString CalendarContactModel::email(int index) const
{
    if (index < 0 || index >= m_contacts.length()) {
        return QString();
    }

    return m_contacts.at(index).email;
}

QList<CalendarData::EmailContact> CalendarContactModel::getList()
{
    return m_contacts;
}

void CalendarContactModel::add(const QString &name, const QString &email)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_contacts.prepend(CalendarData::EmailContact(name, email));
    endInsertRows();

    emit countChanged();
}

QHash<int, QByteArray> CalendarContactModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "name";
    roleNames[EmailRole] = "email";
    return roleNames;
}
