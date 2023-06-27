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

// mkcal
#include <calendarstorage.h>

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
    setError(!importToMemory(mFileName, mIcsRawData));
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
    setError(!importToMemory(mFileName, mIcsRawData));
}

QString CalendarImportModel::notebookUid() const
{
    return mNotebookUid;
}

void CalendarImportModel::setNotebookUid(const QString &notebookUid)
{
    if (notebookUid == mNotebookUid)
        return;

    mNotebookUid = notebookUid;
    emit notebookUidChanged();

    setupDuplicates();
}

bool CalendarImportModel::hasDuplicates() const
{
    return !mDuplicates.isEmpty();
}

bool CalendarImportModel::hasInvitations() const
{
    return !mInvitations.isEmpty();
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
    case DuplicateRole:
        return mDuplicates.contains(event->instanceIdentifier());
    case InvitationRole:
        return mInvitations.contains(event->instanceIdentifier());
    case UidRole:
        return event->uid();
    default:
        return QVariant();
    }
}

bool CalendarImportModel::save(bool discardInvitation) const
{
    mKCal::CalendarStorage::Ptr storage = mKCal::CalendarStorage::systemStorage();
    storage->calendar()->setId(mNotebookUid);
    if (!storage->open()) {
        qWarning() << "Unable to open calendar DB";
    }
    for (const KCalendarCore::Event::Ptr& incidence : mEventList) {
        const KCalendarCore::Incidence::Ptr old =
            storage->calendar()->incidence(incidence->uid(), incidence->recurrenceId());
        if (old) {
            // Unconditionally overwrite existing incidence with the same UID/RecID.
            storage->calendar()->deleteIncidence(old);
        }
        if (discardInvitation) {
            incidence->setOrganizer(KCalendarCore::Person());
            incidence->clearAttendees();
        }
        storage->calendar()->addIncidence(incidence);
    }

    return storage->save();
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
    roleNames[DuplicateRole] = "duplicate";
    roleNames[InvitationRole] = "invitation";
    roleNames[UidRole] = "uid";

    return roleNames;
}

void CalendarImportModel::setupDuplicates()
{
    mDuplicates.clear();
    if (!mNotebookUid.isEmpty()) {
        mKCal::CalendarStorage::Ptr storage = mKCal::CalendarStorage::systemStorage();
        storage->calendar()->setId(mNotebookUid);
        if (!storage->open()) {
            qWarning() << "Unable to open system storage for notebook" << mNotebookUid;
            storage.clear();
        }
        // To avoid detach here, use qAsConst when available.
        for (const KCalendarCore::Event::Ptr &event : mEventList) {
            storage->load(event->uid());
            const KCalendarCore::Event::Ptr old =
                storage->calendar()->event(event->uid(), event->recurrenceId());
            if (old) {
                mDuplicates.insert(old->instanceIdentifier());
            }
        }
    }
    emit hasDuplicatesChanged();
}

bool CalendarImportModel::importToMemory(const QString &fileName, const QByteArray &icsData)
{
    bool success = false;

    KCalendarCore::MemoryCalendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    if (!fileName.isEmpty()) {
        success = CalendarUtils::importFromFile(fileName, cal);
    } else if (!icsData.isEmpty()) {
        success = CalendarUtils::importFromIcsRawData(icsData, cal);
    }

    beginResetModel();
    mInvitations.clear();
    mEventList = cal->events(KCalendarCore::EventSortStartDate);
    // To avoid detach here, use qAsConst when available.
    for (const KCalendarCore::Event::Ptr &event : mEventList) {
        if (!event->organizer().isEmpty()) {
            mInvitations.insert(event->instanceIdentifier());
        }
    }
    setupDuplicates();
    endResetModel();
    emit countChanged();
    emit hasInvitationsChanged();

    return success;
}

void CalendarImportModel::setError(bool error)
{
    if (error != mError) {
        mError = error;
        emit errorChanged();
    }
}

