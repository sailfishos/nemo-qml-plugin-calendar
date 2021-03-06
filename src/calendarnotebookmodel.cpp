/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Aaron Kennedy <aaron.kennedy@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "calendarnotebookmodel.h"

#include "calendardata.h"
#include "calendarmanager.h"

CalendarNotebookModel::CalendarNotebookModel()
{
    connect(CalendarManager::instance(), SIGNAL(notebooksChanged(QList<CalendarData::Notebook>)),
            this, SLOT(notebooksChanged()));
    connect(CalendarManager::instance(), SIGNAL(notebooksAboutToChange()),
            this, SLOT(notebooksAboutToChange()));
}

int CalendarNotebookModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return CalendarManager::instance()->notebooks().count();
}

QVariant CalendarNotebookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= CalendarManager::instance()->notebooks().count())
        return QVariant();

    CalendarData::Notebook notebook = CalendarManager::instance()->notebooks().at(index.row());

    switch (role) {
    case NameRole:
        return notebook.name;
    case UidRole:
        return notebook.uid;
    case DescriptionRole:
        return notebook.description;
    case ColorRole:
        return notebook.color;
    case DefaultRole:
        return notebook.isDefault;
    case ReadOnlyRole:
        return notebook.readOnly;
    case ExcludedRole:
        return notebook.excluded;
    case LocalCalendarRole:
        return notebook.localCalendar;
    case AccountIdRole:
        return notebook.accountId;
    case AccountIconRole:
        return notebook.accountIcon;
    default:
        return QVariant();
    }
}

bool CalendarNotebookModel::setData(const QModelIndex &index, const QVariant &data, int role)
{
    // QAbstractItemModel::setData() appears to assume values getting set synchronously, however
    // we use asynchronous functions from CalendarManager in this function.
    // TODO: cache the notebook data to improve change signaling

    if (!index.isValid()
            || index.row() >= CalendarManager::instance()->notebooks().count()
            || (role != ColorRole && role != DefaultRole))
        return false;

    CalendarData::Notebook notebook = CalendarManager::instance()->notebooks().at(index.row());

    if (role == ColorRole) {
        CalendarManager::instance()->setNotebookColor(notebook.uid, data.toString());
    } else if (role == DefaultRole) {
        CalendarManager::instance()->setDefaultNotebook(notebook.uid);
    }

    return true;
}

void CalendarNotebookModel::notebooksAboutToChange()
{
    beginResetModel();
}

void CalendarNotebookModel::notebooksChanged()
{
    endResetModel();
}

QHash<int, QByteArray> CalendarNotebookModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "name";
    roleNames[UidRole] = "uid";
    roleNames[DescriptionRole] = "description";
    roleNames[ColorRole] = "color";
    roleNames[DefaultRole] = "isDefault";
    roleNames[ReadOnlyRole] = "readOnly";
    roleNames[ExcludedRole] = "excluded";
    roleNames[LocalCalendarRole] = "localCalendar";
    roleNames[AccountIdRole] = "accountId";
    roleNames[AccountIconRole] = "accountIcon";

    return roleNames;
}
