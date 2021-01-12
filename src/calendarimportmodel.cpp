/*
 * Copyright (C) 2015 - 2019 Jolla Ltd.
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#include "calendarimportmodel.h"
#include "calendarimportevent.h"
#include "calendarutils.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QUrl>

// mkcal
#include <extendedcalendar.h>
#include <extendedstorage.h>

// kcalendarcore
#include <KCalendarCore/MemoryCalendar>

CalendarImportModel::CalendarImportModel(QObject *parent)
    : QAbstractListModel(parent),
      mError(false)
{
}

CalendarImportModel::~CalendarImportModel()
{
}

int CalendarImportModel::count() const
{
    return mEventList.count();
}

QString CalendarImportModel::fileName() const
{
    return mFileName;
}

void CalendarImportModel::setFileName(const QString &fileName)
{
    if (mFileName == fileName)
        return;

    mFileName = fileName;
    emit fileNameChanged();
    reload();
}

QString CalendarImportModel::icsString() const
{
    return QString::fromUtf8(mIcsRawData);
}

void CalendarImportModel::setIcsString(const QString &icsData)
{
    QByteArray data = icsData.toUtf8();
    if (mIcsRawData == data)
        return;

    mIcsRawData = data;
    emit icsStringChanged();
    reload();
}

bool CalendarImportModel::error() const
{
    return mError;
}

QObject *CalendarImportModel::getEvent(int index)
{
    if (index < 0 || index >= mEventList.count())
        return 0;

    return new CalendarImportEvent(mEventList.at(index));
}

int CalendarImportModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return mEventList.count();
}

QVariant CalendarImportModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= mEventList.count())
        return QVariant();

    KCalendarCore::Event::Ptr event = mEventList.at(index.row());

    switch(role) {
    case DisplayLabelRole:
        return event->summary();
    case DescriptionRole:
        return event->description();
    case StartTimeRole:
        return event->dtStart();
    case EndTimeRole:
        return event->dtEnd();
    case AllDayRole:
        return event->allDay();
    case LocationRole:
        return event->location();
    case UidRole:
        return event->uid();
    default:
        return QVariant();
    }
}

bool CalendarImportModel::importToNotebook(const QString &notebookUid)
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = calendar->defaultStorage(calendar);
    if (!storage->open()) {
        qWarning() << "Unable to open calendar DB";
        return false;
    }

    if (!notebookUid.isEmpty()) {
        if (! (storage->defaultNotebook() && storage->defaultNotebook()->uid() == notebookUid)) {
            mKCal::Notebook::Ptr notebook = storage->notebook(notebookUid);
            if (notebook) {
                // TODO: should we change default notebook back if we change it?
                storage->setDefaultNotebook(notebook);
            } else {
                qWarning() << "Invalid notebook UID" << notebookUid;
                return false;
            }
        }
    }

    if (CalendarUtils::importFromFile(mFileName, calendar))
        storage->save();

    storage->close();

    return true;
}

QHash<int, QByteArray> CalendarImportModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[DisplayLabelRole] = "displayLabel";
    roleNames[DescriptionRole] = "description";
    roleNames[StartTimeRole] = "startTime";
    roleNames[EndTimeRole] = "endTime";
    roleNames[AllDayRole] = "allDay";
    roleNames[LocationRole] = "location";
    roleNames[UidRole] = "uid";

    return roleNames;
}

static bool incidenceLessThan(const KCalendarCore::Incidence::Ptr e1,
                              const KCalendarCore::Incidence::Ptr e2)
{
    if (e1->dtStart() == e2->dtStart()) {
        int cmp = QString::compare(e1->summary(),
                                   e2->summary(),
                                   Qt::CaseInsensitive);
        if (cmp == 0)
            return QString::compare(e1->uid(), e2->uid()) < 0;
        else
            return cmp < 0;
    } else {
        return e1->dtStart() < e2->dtStart();
    }
}

void CalendarImportModel::reload()
{
    if (!mFileName.isEmpty() || !mIcsRawData.isEmpty()) {
        bool success = importToMemory(mFileName, mIcsRawData);
        setError(!success);
    } else if (!mEventList.isEmpty()) {
        beginResetModel();
        mEventList.clear();
        endResetModel();
        emit countChanged();
        setError(false);
    }
}

bool CalendarImportModel::importToMemory(const QString &fileName, const QByteArray &icsData)
{
    if (!mEventList.isEmpty())
        mEventList.clear();

    bool success = false;

    beginResetModel();
    KCalendarCore::MemoryCalendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    if (!fileName.isEmpty()) {
        success = CalendarUtils::importFromFile(fileName, cal);
    } else {
        success = CalendarUtils::importFromIcsRawData(icsData, cal);
    }
    KCalendarCore::Incidence::List incidenceList = cal->incidences();
    for (int i = 0; i < incidenceList.size(); i++) {
        KCalendarCore::Incidence::Ptr incidence = incidenceList.at(i);
        if (incidence->type() == KCalendarCore::IncidenceBase::TypeEvent)
            mEventList.append(incidence.staticCast<KCalendarCore::Event>());
    }
    if (!mEventList.isEmpty())
        qSort(mEventList.begin(), mEventList.end(), incidenceLessThan);

    endResetModel();
    emit countChanged();
    return success;
}

void CalendarImportModel::setError(bool error)
{
    if (error != mError) {
        mError = error;
        emit errorChanged();
    }
}

