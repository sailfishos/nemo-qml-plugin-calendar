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

#include "calendardataservice.h"

#include <QtCore/QDate>
#include <QtCore/QDebug>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include "calendardataserviceadaptor.h"
#include "../../src/calendaragendamodel.h"
#include "../../src/calendarevent.h"
#include "../../src/calendareventoccurrence.h"
#include "../../src/calendarmanager.h"

CalendarDataService::CalendarDataService(QObject *parent) :
    QObject(parent), m_agendaModel(0), m_transactionIdCounter(0)
{
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(2000);
    connect(&m_killTimer, SIGNAL(timeout()), this, SLOT(shutdown()));
    m_killTimer.start();

    registerCalendarDataServiceTypes();
    new CalendarDataServiceAdaptor(this);
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerObject("/org/nemomobile/calendardataservice", this))
        qWarning("Can't register org/nemomobile/calendardataservice object for the D-Bus service.");

    if (!connection.registerService("org.nemomobile.calendardataservice"))
        qWarning("Can't register org.nemomobile.calendardataservice service on the session bus");

    if (connection.lastError().isValid())
        QCoreApplication::exit(1);
}

QString CalendarDataService::getEvents(const QString &startDate, const QString &endDate)
{
    m_killTimer.stop();
    QDate start = QDate::fromString(startDate, Qt::ISODate);
    QDate end = QDate::fromString(endDate, Qt::ISODate);
    QString transactionId;
    if (!start.isValid() || !end.isValid()) {
        qWarning() << "Invalid date parameter(s):" << startDate << ", " << endDate;
    } else {
        transactionId = QString("%1-%2")
                .arg(QCoreApplication::applicationPid())
                .arg(++m_transactionIdCounter);
        DataRequest dataRequest = { start, end, transactionId };
        m_dataRequestQueue.insert(0, dataRequest);
    }
    // Delay triggering until after return to ensure that client gets transactionId
    QTimer::singleShot(1, this, SLOT(processQueue()));
    return transactionId;
}

void CalendarDataService::updated()
{
    EventDataList reply;
    for (int i = 0; i < m_agendaModel->count(); i++) {
        QVariant variant = m_agendaModel->get(i, CalendarAgendaModel::EventObjectRole);
        QVariant occurrenceVariant = m_agendaModel->get(i, CalendarAgendaModel::OccurrenceObjectRole);
        if (variant.canConvert<CalendarStoredEvent *>() && occurrenceVariant.canConvert<CalendarEventOccurrence *>()) {
            CalendarStoredEvent* event = variant.value<CalendarStoredEvent *>();
            CalendarEventOccurrence* occurrence = occurrenceVariant.value<CalendarEventOccurrence *>();
            EventData eventStruct;
            eventStruct.displayLabel = event->displayLabel();
            eventStruct.description = event->description();
            if (event->allDay()) {
                eventStruct.startTime = occurrence->startTime().date().toString(Qt::ISODate);
                eventStruct.endTime = occurrence->endTime().date().toString(Qt::ISODate);
            } else {
                eventStruct.startTime = occurrence->startTime().toString(Qt::ISODate);
                eventStruct.endTime = occurrence->endTime().toString(Qt::ISODate);
            }
            eventStruct.allDay = event->allDay();
            eventStruct.color = event->color();
            eventStruct.instanceId = event->instanceId();
            eventStruct.cancelled = event->status() == CalendarEvent::StatusCancelled;
            reply << eventStruct;
        }
    }
    if (!m_currentDataRequest.transactionId.isEmpty()
            && m_currentDataRequest.start == m_agendaModel->startDate()
            && m_currentDataRequest.end == m_agendaModel->endDate()) {
        emit getEventsResult(m_currentDataRequest.transactionId, reply);
        m_currentDataRequest = DataRequest();
    } else {
        qWarning() << "No transactionId, discarding results";
    }
    processQueue();
}

void CalendarDataService::shutdown()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.unregisterService("org.nemomobile.calendardataservice");
    connection.unregisterObject("/org/nemomobile/calendardataservice");

    if (m_agendaModel) {
        // Call CalendarManager dtor to ensure that the QThread managed by it
        // will be destroyed via deleteLater when control returns to the event loop.
        // Delete the AgendaModel first, its destructor refers to CalendarManager
        delete m_agendaModel;
        delete CalendarManager::instance();
    }
    QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
}

void CalendarDataService::initialize()
{
    if (!m_agendaModel) {
        m_agendaModel = new CalendarAgendaModel(this);
        connect(m_agendaModel, SIGNAL(updated()), this, SLOT(updated()));
    }
}

void CalendarDataService::processQueue()
{
    if (m_dataRequestQueue.isEmpty()) {
        m_killTimer.start();
        return;
    }

    if (m_currentDataRequest.transactionId.isEmpty()) {
        initialize();
        m_currentDataRequest = m_dataRequestQueue.takeLast();
        if (m_agendaModel->startDate() == m_currentDataRequest.start
                && m_agendaModel->endDate() == m_currentDataRequest.end) {
            // We already have the events, go to updated() directly
            updated();
        } else {
            m_agendaModel->setStartDate(m_currentDataRequest.start);
            m_agendaModel->setEndDate(m_currentDataRequest.end);
        }
    }
}
