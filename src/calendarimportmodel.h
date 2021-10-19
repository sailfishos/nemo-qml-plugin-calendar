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

#ifndef CALENDARIMPORT_H
#define CALENDARIMPORT_H

#include <QAbstractListModel>

#include <KCalendarCore/Calendar>
#include <extendedstorage.h>

class CalendarImportModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString icsString READ icsString WRITE setIcsString NOTIFY icsStringChanged)
    Q_PROPERTY(bool hasDuplicates READ hasDuplicates NOTIFY hasDuplicatesChanged)
    Q_PROPERTY(bool error READ error NOTIFY errorChanged)

public:
    enum {
        DisplayLabelRole = Qt::UserRole,
        DescriptionRole,
        StartTimeRole,
        EndTimeRole,
        AllDayRole,
        LocationRole,
        UidRole,
        DuplicateRole,
    };

    explicit CalendarImportModel(QObject *parent = 0);
    ~CalendarImportModel();

    int count() const;

    QString fileName() const;
    void setFileName(const QString& fileName);

    QString icsString() const;
    void setIcsString(const QString &icsData);

    bool hasDuplicates() const;

    bool error() const;

    virtual int rowCount(const QModelIndex &index) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

public slots:
    QObject *getEvent(int index);

signals:
    void countChanged();
    void fileNameChanged();
    void icsStringChanged();
    void hasDuplicatesChanged();
    bool errorChanged();

public slots:
    bool importToNotebook(const QString &notebookUid = QString()) const;

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private:
    bool importToMemory(const QString &fileName, const QByteArray &icsData);
    void setError(bool error);

    QString mFileName;
    QByteArray mIcsRawData;
    KCalendarCore::Event::List mEventList;
    mKCal::ExtendedStorage::Ptr mStorage;
    QSet<QString> mDuplicates;
    bool mError;
};

#endif // CALENDARIMPORT_H
