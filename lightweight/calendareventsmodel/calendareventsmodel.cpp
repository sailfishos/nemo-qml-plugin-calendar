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
    m_proxy(0),
    m_watcher(new QFileSystemWatcher(this)),
    m_filterMode(FilterNone),
    m_contentType(ContentAll),
    m_eventLimit(1000),
    m_totalCount(0),
    m_eventDisplayTime(0),
    m_mkcalTracked(false)
{
    registerCalendarDataServiceTypes();
    m_proxy = new CalendarDataServiceProxy("org.nemomobile.calendardataservice",
                                           "/org/nemomobile/calendardataservice",
                                           QDBusConnection::sessionBus(),
                                           this);
    connect(m_proxy, SIGNAL(getEventsResult(QString,EventDataList)),
            this, SLOT(getEventsResult(QString,EventDataList)));

    m_updateDelayTimer.setInterval(500);
    m_updateDelayTimer.setSingleShot(true);
    connect(&m_updateDelayTimer, SIGNAL(timeout()), this, SLOT(update()));

    trackMkcal();

    QSettings settings("nemo", "nemo-qml-plugin-calendar");
    QDir settingsDir(QFileInfo(settings.fileName()).absoluteDir());

    if (!settingsDir.exists()) {
        // forcefully create a settings path so changes can be followed
        settingsDir.mkpath(QStringLiteral("."));
    }

    if (!m_watcher->addPath(settingsDir.absolutePath())) {
        qWarning() << "CalendarEventsModel: error following settings file changes" << settingsDir.absolutePath();
    }

    // Updates to the calendar db will cause several file change notifications, delay update a bit
    connect(m_watcher, SIGNAL(directoryChanged(QString)), &m_updateDelayTimer, SLOT(start()));
    connect(m_watcher, SIGNAL(fileChanged(QString)), &m_updateDelayTimer, SLOT(start())); // for mkcal tracking
}

int CalendarEventsModel::count() const
{
    return qMin(m_eventDataList.count(), m_eventLimit);
}

int CalendarEventsModel::totalCount() const
{
    return m_totalCount;
}

QDateTime CalendarEventsModel::creationDate() const
{
    return m_creationDate;
}

QDateTime CalendarEventsModel::expiryDate() const
{
    return m_expiryDate;
}

int CalendarEventsModel::eventLimit() const
{
    return m_eventLimit;
}

void CalendarEventsModel::setEventLimit(int limit)
{
    if (m_eventLimit == limit || limit <= 0)
        return;

    m_eventLimit = limit;
    emit eventLimitChanged();
    restartUpdateTimer(); // TODO: Could change list content without fetching data
}

int CalendarEventsModel::eventDisplayTime() const
{
    return m_eventDisplayTime;
}

void CalendarEventsModel::setEventDisplayTime(int seconds)
{
    if (m_eventDisplayTime == seconds)
        return;

    m_eventDisplayTime = seconds;
    emit eventDisplayTimeChanged();

    restartUpdateTimer();
}

QDateTime CalendarEventsModel::startDate() const
{
    return m_startDate;
}

void CalendarEventsModel::setStartDate(const QDateTime &startDate)
{
    if (m_startDate == startDate)
        return;

    m_startDate = startDate;
    emit startDateChanged();

    restartUpdateTimer();
}

QDateTime CalendarEventsModel::endDate() const
{
    return m_endDate;
}

void CalendarEventsModel::setEndDate(const QDateTime &endDate)
{
    if (m_endDate == endDate)
        return;

    m_endDate = endDate;
    emit endDateChanged();

    restartUpdateTimer();
}

int CalendarEventsModel::filterMode() const
{
    return m_filterMode;
}

void CalendarEventsModel::setFilterMode(int mode)
{
    if (m_filterMode == mode)
        return;

    m_filterMode = mode;
    emit filterModeChanged();
    restartUpdateTimer();
}


int CalendarEventsModel::contentType() const
{
    return m_contentType;
}

void CalendarEventsModel::setContentType(int contentType)
{
    if (m_contentType == contentType)
        return;

    m_contentType = contentType;
    emit contentTypeChanged();
    restartUpdateTimer();
}

int CalendarEventsModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return m_eventDataList.count();
}

QVariant CalendarEventsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_eventDataList.count())
        return QVariant();

    EventData eventData = m_eventDataList.at(index.row());

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
    case AllDayRole:
        return eventData.allDay;
    case LocationRole:
        return eventData.location;
    case CalendarUidRole:
        return eventData.calendarUid;
    case InstanceIdRole:
        return eventData.instanceId;
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
    m_transactionId.clear();
    QDateTime endDate = (m_endDate.isValid()) ? m_endDate : m_startDate;
    QDBusPendingCall pcall = m_proxy->getEvents(m_startDate.date().toString(Qt::ISODate),
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
        m_transactionId = reply.value();

    call->deleteLater();
}

void CalendarEventsModel::getEventsResult(const QString &transactionId, const EventDataList &eventDataList)
{
    // mkcal database didn't necessarily exist on startup but after calendar service has checked
    // events it should be there.
    trackMkcal();

    if (m_transactionId != transactionId)
        return;

    int oldcount = m_eventDataList.count();
    int oldTotalCount = m_totalCount;
    beginResetModel();
    m_eventDataList.clear();
    QDateTime now = QDateTime::currentDateTime();
    QDateTime expiryDate;
    m_totalCount = 0;
    foreach (const EventData &e, eventDataList) {
        if ((e.allDay && m_contentType == ContentEvents)
                || (!e.allDay && m_contentType == ContentAllDay)) {
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

            if (m_eventDisplayTime > 0) {
                endTime = startTime.addSecs(m_eventDisplayTime);
            } else {
                endTime = QDateTime::fromString(e.endTime, Qt::ISODate);
            }
        }

        if ((m_filterMode == FilterPast && now < endTime)
                || (m_filterMode == FilterPastAndCurrent && now < startTime)
                || (m_filterMode == FilterNone)) {
            if (m_eventDataList.count() < m_eventLimit) {
                m_eventDataList.append(e);

                if (m_filterMode == FilterPast && (!expiryDate.isValid() || expiryDate > endTime)) {
                    expiryDate = endTime;
                } else if (m_filterMode == FilterPastAndCurrent && (!expiryDate.isValid() || expiryDate > startTime)) {
                    expiryDate = startTime;
                }
            }
            m_totalCount++;
        }
    }

    m_creationDate = QDateTime::currentDateTime();
    emit creationDateChanged();

    if (!expiryDate.isValid()) {
        if (m_endDate.isValid()) {
            expiryDate = m_endDate;
        } else {
            expiryDate = m_startDate.addDays(1);
            expiryDate.setTime(QTime(0,0,0,1));
        }
    }

    if (m_expiryDate != expiryDate) {
        m_expiryDate = expiryDate;
        emit expiryDateChanged();
    }

    endResetModel();
    if (count() != oldcount) {
        emit countChanged();
    }
    if (m_totalCount != oldTotalCount) {
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
    roleNames[AllDayRole] = "allDay";
    roleNames[LocationRole] = "location";
    roleNames[CalendarUidRole] = "calendarUid";
    roleNames[InstanceIdRole] = "instanceId";
    roleNames[ColorRole] = "color";
    roleNames[CancelledRole] = "cancelled";

    return roleNames;
}

void CalendarEventsModel::restartUpdateTimer()
{
    if (m_startDate.isValid())
        m_updateDelayTimer.start();
    else
        m_updateDelayTimer.stop();
}

void CalendarEventsModel::trackMkcal()
{
    if (m_mkcalTracked) {
        return;
    }

    QString privilegedDataDir = QString("%1/.local/share/system/privileged/Calendar/mkcal/db").arg(QDir::homePath());
    if (QFile::exists(privilegedDataDir)) {
        if (!m_watcher->addPath(privilegedDataDir)) {
            qWarning() << "CalendarEventsModel: error adding filesystem watcher for calendar db";
        } else {
            m_mkcalTracked = true;
        }
    } else {
        qWarning() << "CalendarEventsModel not following database changes, dir not found:" << privilegedDataDir;
    }
}
