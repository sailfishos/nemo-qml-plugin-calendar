/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Petri M. Gerdt <petri.gerdt@jollamobile.com>
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

#include "calendareventsmodel.h"

#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <qqmlinfo.h>

#include "calendardataserviceproxy.h"

CalendarEventsModel::CalendarEventsModel(QObject *parent) :
    QAbstractListModel(parent),
    mProxy(0),
    mWatcher(new QFileSystemWatcher(this)),
    mFilterMode(FilterNone),
    mContentType(ContentAll),
    mEventLimit(1000),
    mTotalCount(0),
    mEventDisplayTime(0),
    m_mkcalTracked(false)
{
    registerCalendarDataServiceTypes();
    mProxy = new CalendarDataServiceProxy("org.nemomobile.calendardataservice",
                                          "/org/nemomobile/calendardataservice",
                                          QDBusConnection::sessionBus(),
                                          this);
    connect(mProxy, SIGNAL(getEventsResult(QString,EventDataList)),
            this, SLOT(getEventsResult(QString,EventDataList)));

    mUpdateDelayTimer.setInterval(500);
    mUpdateDelayTimer.setSingleShot(true);
    connect(&mUpdateDelayTimer, SIGNAL(timeout()), this, SLOT(update()));

    trackMkcal();

    QSettings settings("nemo", "nemo-qml-plugin-calendar");
    QDir settingsDir(QFileInfo(settings.fileName()).absoluteDir());

    if (!settingsDir.exists()) {
        // forcefully create a settings path so changes can be followed
        settingsDir.mkpath(QStringLiteral("."));
    }

    if (!mWatcher->addPath(settingsDir.absolutePath())) {
        qWarning() << "CalendarEventsModel: error following settings file changes" << settingsDir.absolutePath();
    }

    // Updates to the calendar db will cause several file change notifications, delay update a bit
    connect(mWatcher, SIGNAL(directoryChanged(QString)), &mUpdateDelayTimer, SLOT(start()));
    connect(mWatcher, SIGNAL(fileChanged(QString)), &mUpdateDelayTimer, SLOT(start())); // for mkcal tracking
}

int CalendarEventsModel::count() const
{
    return qMin(mEventDataList.count(), mEventLimit);
}

int CalendarEventsModel::totalCount() const
{
    return mTotalCount;
}

QDateTime CalendarEventsModel::creationDate() const
{
    return mCreationDate;
}

QDateTime CalendarEventsModel::expiryDate() const
{
    return mExpiryDate;
}

int CalendarEventsModel::eventLimit() const
{
    return mEventLimit;
}

void CalendarEventsModel::setEventLimit(int limit)
{
    if (mEventLimit == limit || limit <= 0)
        return;

    mEventLimit = limit;
    emit eventLimitChanged();
    restartUpdateTimer(); // TODO: Could change list content without fetching data
}

int CalendarEventsModel::eventDisplayTime() const
{
    return mEventDisplayTime;
}

void CalendarEventsModel::setEventDisplayTime(int seconds)
{
    if (mEventDisplayTime == seconds)
        return;

    mEventDisplayTime = seconds;
    emit eventDisplayTimeChanged();

    restartUpdateTimer();
}

QDateTime CalendarEventsModel::startDate() const
{
    return mStartDate;
}

void CalendarEventsModel::setStartDate(const QDateTime &startDate)
{
    if (mStartDate == startDate)
        return;

    mStartDate = startDate;
    emit startDateChanged();

    restartUpdateTimer();
}

QDateTime CalendarEventsModel::endDate() const
{
    return mEndDate;
}

void CalendarEventsModel::setEndDate(const QDateTime &endDate)
{
    if (mEndDate == endDate)
        return;

    mEndDate = endDate;
    emit endDateChanged();

    restartUpdateTimer();
}

int CalendarEventsModel::filterMode() const
{
    return mFilterMode;
}

void CalendarEventsModel::setFilterMode(int mode)
{
    if (mFilterMode == mode)
        return;

    mFilterMode = mode;
    emit filterModeChanged();
    restartUpdateTimer();
}


int CalendarEventsModel::contentType() const
{
    return mContentType;
}

void CalendarEventsModel::setContentType(int contentType)
{
    if (mContentType == contentType)
        return;

    mContentType = contentType;
    emit contentTypeChanged();
    restartUpdateTimer();
}

int CalendarEventsModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return mEventDataList.count();
}

QVariant CalendarEventsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= mEventDataList.count())
        return QVariant();

    EventData eventData = mEventDataList.at(index.row());

    switch(role) {
    case DisplayLabelRole:
        return eventData.displayLabel;
    case DescriptionRole:
        return eventData.description;
    case StartTimeRole:
        if (eventData.allDay) {
            return QDateTime(QDate::fromString(eventData.startTime, Qt::ISODate));
        } else {
            return QDateTime::fromString(eventData.startTime, Qt::ISODate);
        }
    case EndTimeRole:
        if (eventData.allDay) {
            return QDateTime(QDate::fromString(eventData.endTime, Qt::ISODate));
        } else {
            return QDateTime::fromString(eventData.endTime, Qt::ISODate);
        }
    case RecurrenceIdRole:
        return eventData.recurrenceId;
    case AllDayRole:
        return eventData.allDay;
    case LocationRole:
        return eventData.location;
    case CalendarUidRole:
        return eventData.calendarUid;
    case UidRole:
        return eventData.uniqueId;
    case ColorRole:
        return QColor(eventData.color);
    case CancelledRole:
        return eventData.cancelled;
    default:
        return QVariant();
    }
}

void CalendarEventsModel::update()
{
    mTransactionId.clear();
    QDateTime endDate = (mEndDate.isValid()) ? mEndDate : mStartDate;
    QDBusPendingCall pcall = mProxy->getEvents(mStartDate.date().toString(Qt::ISODate),
                                               endDate.date().toString(Qt::ISODate));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this, SLOT(updateFinished(QDBusPendingCallWatcher*)));
}

void CalendarEventsModel::updateFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;
    if (reply.isError())
        qWarning() << "dbus error:" << reply.error().name() << reply.error().message();
    else
        mTransactionId = reply.value();

    call->deleteLater();
}

void CalendarEventsModel::getEventsResult(const QString &transactionId, const EventDataList &eventDataList)
{
    // mkcal database didn't necessarily exist on startup but after calendar service has checked
    // events it should be there.
    trackMkcal();

    if (mTransactionId != transactionId)
        return;

    int oldcount = mEventDataList.count();
    int oldTotalCount = mTotalCount;
    beginResetModel();
    mEventDataList.clear();
    QDateTime now = QDateTime::currentDateTime();
    QDateTime expiryDate;
    mTotalCount = 0;
    foreach (const EventData &e, eventDataList) {
        if ((e.allDay && mContentType == ContentEvents)
                || (!e.allDay && mContentType == ContentAllDay)) {
            continue;
        }

        QDateTime startTime;
        QDateTime endTime;

        if (e.allDay) {
            startTime = QDateTime(QDate::fromString(e.startTime, Qt::ISODate));
            // returned value inclusive, need to know when event is over so getting the following day
            endTime = QDateTime(QDate::fromString(e.endTime, Qt::ISODate).addDays(1));
        } else {
            startTime = QDateTime::fromString(e.startTime, Qt::ISODate);

            if (mEventDisplayTime > 0) {
                endTime = startTime.addSecs(mEventDisplayTime);
            } else {
                endTime = QDateTime::fromString(e.endTime, Qt::ISODate);
            }
        }

        if ((mFilterMode == FilterPast && now < endTime)
                || (mFilterMode == FilterPastAndCurrent && now < startTime)
                || (mFilterMode == FilterNone)) {
            if (mEventDataList.count() < mEventLimit) {
                mEventDataList.append(e);

                if (mFilterMode == FilterPast && (!expiryDate.isValid() || expiryDate > endTime)) {
                    expiryDate = endTime;
                } else if (mFilterMode == FilterPastAndCurrent && (!expiryDate.isValid() || expiryDate > startTime)) {
                    expiryDate = startTime;
                }
            }
            mTotalCount++;
        }
    }

    mCreationDate = QDateTime::currentDateTime();
    emit creationDateChanged();

    if (!expiryDate.isValid()) {
        if (mEndDate.isValid()) {
            expiryDate = mEndDate;
        } else {
            expiryDate = mStartDate.addDays(1);
            expiryDate.setTime(QTime(0,0,0,1));
        }
    }

    if (mExpiryDate != expiryDate) {
        mExpiryDate = expiryDate;
        emit expiryDateChanged();
    }

    endResetModel();
    if (count() != oldcount) {
        emit countChanged();
    }
    if (mTotalCount != oldTotalCount) {
        emit totalCountChanged();
    }
}

QHash<int, QByteArray> CalendarEventsModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[DisplayLabelRole] = "displayLabel";
    roleNames[DescriptionRole] = "description";
    roleNames[StartTimeRole] = "startTime";
    roleNames[EndTimeRole] = "endTime";
    roleNames[RecurrenceIdRole] = "recurrenceId";
    roleNames[AllDayRole] = "allDay";
    roleNames[LocationRole] = "location";
    roleNames[CalendarUidRole] = "calendarUid";
    roleNames[UidRole] = "uid";
    roleNames[ColorRole] = "color";
    roleNames[CancelledRole] = "cancelled";

    return roleNames;
}

void CalendarEventsModel::restartUpdateTimer()
{
    if (mStartDate.isValid())
        mUpdateDelayTimer.start();
    else
        mUpdateDelayTimer.stop();
}

void CalendarEventsModel::trackMkcal()
{
    if (m_mkcalTracked) {
        return;
    }

    QString privilegedDataDir = QString("%1/.local/share/system/privileged/Calendar/mkcal/db").arg(QDir::homePath());
    if (QFile::exists(privilegedDataDir)) {
        if (!mWatcher->addPath(privilegedDataDir)) {
            qWarning() << "CalendarEventsModel: error adding filesystem watcher for calendar db";
        } else {
            m_mkcalTracked = true;
        }
    } else {
        qWarning() << "CalendarEventsModel not following database changes, dir not found:" << privilegedDataDir;
    }
}
