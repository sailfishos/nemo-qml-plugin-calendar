/*
 * Copyright (C) 2021 Damien Caliste <dcaliste@free.fr>
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

#ifndef CALENDAREVENTLISTMODEL_H
#define CALENDAREVENTLISTMODEL_H

#include <QAbstractListModel>
#include <QQmlParserStatus>

class CalendarEventOccurrence;

class CalendarEventListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList identifiers READ identifiers WRITE setIdentifiers NOTIFY identifiersChanged)
    Q_PROPERTY(QStringList missingItems READ missingItems NOTIFY missingItemsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    enum EventListRoles {
        EventObjectRole = Qt::UserRole,
        OccurrenceObjectRole,
        IdentifierRole
    };
    Q_ENUM(EventListRoles)

    explicit CalendarEventListModel(QObject *parent = 0);
    virtual ~CalendarEventListModel();

    int count() const;

    QStringList identifiers() const;
    void setIdentifiers(const QStringList &ids);

    QStringList missingItems() const;

    bool loading() const;

    int rowCount(const QModelIndex &index) const;
    QVariant data(const QModelIndex &index, int role) const;

    virtual void classBegin();
    virtual void componentComplete();

signals:
    void countChanged();
    void identifiersChanged();
    void missingItemsChanged();
    virtual void loadingChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private slots:
    void doRefresh();
    void onTimezoneChanged();

private:
    void refresh();

    bool mIsComplete;
    QStringList mIdentifiers;
    QStringList mMissingItems;
    QList<CalendarEventOccurrence*> mEvents;
    QStringList mEventIdentifiers; // contains a subset of mIdentifiers corresponding to events in mEvents
};

#endif // CALENDAREVENTLISTMODEL_H
