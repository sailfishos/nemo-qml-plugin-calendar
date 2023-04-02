/*
 * Copyright (C) 2023 Damien Caliste <dcaliste@free.fr>
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

#include "calendarsearchmodel.h"
#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "logging_p.h"

#include <QDebug>

CalendarSearchModel::CalendarSearchModel(QObject *parent)
    : CalendarEventListModel(parent)
{
    setStartTime(QDateTime::currentDateTimeUtc());
}

CalendarSearchModel::~CalendarSearchModel()
{
    CalendarManager::instance()->cancelSearch(this);
}

QHash<int, QByteArray> CalendarSearchModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = CalendarEventListModel::roleNames();
    roleNames[YearRole] = "year";
    return roleNames;
}

QVariant CalendarSearchModel::data(const QModelIndex &index, int role) const
{
    if (role < YearRole)
        return CalendarEventListModel::data(index, role);

    QVariant data = CalendarEventListModel::data(index, CalendarEventListModel::OccurrenceObjectRole);
    if (!data.isValid())
        return QVariant();

    CalendarEventOccurrence *occurrence = qobject_cast<CalendarEventOccurrence*>(data.value<QObject*>());
    if (!occurrence)
        return QVariant();

    switch (role) {
    case YearRole:
        return QVariant(occurrence->startTime().date().year());
    default:
        qWarning() << "CalendarSearchModel: Unknown role asked";
        return QVariant();
    }
}

void CalendarSearchModel::setSearchString(const QString &searchString)
{
    if (searchString == mSearchString)
        return;

    mSearchString = searchString;
    emit searchStringChanged();

    setIdentifiers(QStringList());
    if (!mSearchString.isEmpty()) {
        CalendarManager::instance()->search(this);
        emit loadingChanged();
    }
}

QString CalendarSearchModel::searchString() const
{
    return mSearchString;
}

int CalendarSearchModel::limit() const
{
    return mLimit;
}

void CalendarSearchModel::setLimit(int limit)
{
    if (limit == mLimit)
        return;

    mLimit = limit;
    emit limitChanged();

    if (!mSearchString.isEmpty()) {
        CalendarManager::instance()->search(this);
        emit loadingChanged();
    }
}

void CalendarSearchModel::setIdentifiers(const QStringList &ids)
{
    CalendarEventListModel::setIdentifiers(ids);
    emit loadingChanged();
}

bool CalendarSearchModel::loading() const
{
    return CalendarManager::instance()->isSearching(this)
        || CalendarEventListModel::loading();
}
