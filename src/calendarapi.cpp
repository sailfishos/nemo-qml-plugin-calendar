/*
 * Copyright (C) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2020 Open Mobile Platform LLC.
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

#include "calendarapi.h"

#include <QQmlEngine>
#include "calendarevent.h"
#include "calendareventmodification.h"
#include "calendareventoccurrence.h"
#include "calendarmanager.h"

CalendarApi::CalendarApi(QObject *parent)
: QObject(parent)
{
    connect(CalendarManager::instance(), SIGNAL(excludedNotebooksChanged(QStringList)),
            this, SIGNAL(excludedNotebooksChanged()));
    connect(CalendarManager::instance(), SIGNAL(defaultNotebookChanged(QString)),
            this, SIGNAL(defaultNotebookChanged()));
}

CalendarEventModification *CalendarApi::createNewEvent()
{
    return new CalendarEventModification();
}

CalendarEventModification * CalendarApi::createModification(CalendarStoredEvent *sourceEvent,
                                                            CalendarEventOccurrence *occurrence)
{
    return new CalendarEventModification(sourceEvent, occurrence);
}

void CalendarApi::remove(const QString &uid, const QString &recurrenceId, const QDateTime &time)
{
    QDateTime recurrenceTime = QDateTime::fromString(recurrenceId, Qt::ISODate);
    CalendarManager::instance()->deleteEvent(uid, recurrenceTime, time);

    // TODO: this sucks
    CalendarManager::instance()->save();
}

void CalendarApi::removeAll(const QString &uid)
{
    CalendarManager::instance()->deleteAll(uid);
    CalendarManager::instance()->save();
}

QStringList CalendarApi::excludedNotebooks() const
{
    return CalendarManager::instance()->excludedNotebooks();
}

void CalendarApi::setExcludedNotebooks(const QStringList &list)
{
    CalendarManager::instance()->setExcludedNotebooks(list);
}

QString CalendarApi::defaultNotebook() const
{
    return CalendarManager::instance()->defaultNotebook();
}

void CalendarApi::setDefaultNotebook(const QString &notebook)
{
    CalendarManager::instance()->setDefaultNotebook(notebook);
}

QObject *CalendarApi::New(QQmlEngine *e, QJSEngine *)
{
    return new CalendarApi(e);
}
