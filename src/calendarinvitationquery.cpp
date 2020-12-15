/*
 * Copyright (C) 2017 Jolla Ltd.
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

#include "calendarinvitationquery.h"

#include "calendarmanager.h"
#include "calendareventoccurrence.h"
#include "calendarutils.h"

CalendarInvitationQuery::CalendarInvitationQuery()
    : mIsComplete(false)
    , mNeedQuery(false)
{
}

CalendarInvitationQuery::~CalendarInvitationQuery()
{
    CalendarManager *manager = CalendarManager::instance(false);
    if (manager) {
        manager->unRegisterInvitationQuery(this);
    }
}

QString CalendarInvitationQuery::notebookUid() const
{
    return mNotebookUid;
}

QString CalendarInvitationQuery::uid() const
{
    return mUid;
}

QString CalendarInvitationQuery::rid() const
{
    return mRid;
}

QString CalendarInvitationQuery::startTime() const
{
    return mStartTime;
}

QString CalendarInvitationQuery::invitationFile() const
{
    return mInvitationFile;
}

bool CalendarInvitationQuery::busy() const
{
    return mBusy;
}

void CalendarInvitationQuery::setInvitationFile(const QString &file)
{
    if (mInvitationFile != file) {
        mInvitationFile = file;
        emit invitationFileChanged();
    }

    query();
}

void CalendarInvitationQuery::classBegin()
{
    mIsComplete = false;
}

void CalendarInvitationQuery::componentComplete()
{
    mIsComplete = true;
    if (mNeedQuery) {
        query();
    }
}

void CalendarInvitationQuery::query()
{
    if (!mInvitationFile.isEmpty()) {
        // note: we allow scheduling the query even if mBusy is true
        // as the client could have changed the invitation file
        // and in that case, the old query is orphaned by the manager.
        bool oldBusy = mBusy;
        mBusy = true;
        if (!oldBusy) {
            emit busyChanged();
        }

        if (mIsComplete) {
            CalendarManager::instance()->scheduleInvitationQuery(this, mInvitationFile);
        } else {
            mNeedQuery = true;
        }
    }
}

void CalendarInvitationQuery::queryResult(CalendarData::Event event)
{
    bool needNUidEmit = false;
    bool needUidEmit = false;
    bool needRidEmit = false;
    bool needSTEmit = false;

    if (mNotebookUid != event.calendarUid) {
        mNotebookUid = event.calendarUid;
        needNUidEmit = true;
    }

    if (mUid != event.uniqueId) {
        mUid = event.uniqueId;
        needUidEmit = true;
    }

    const QString &recurrenceIdString = CalendarUtils::recurrenceIdToString(event.recurrenceId);
    if (mRid != recurrenceIdString) {
        mRid = recurrenceIdString;
        needRidEmit = true;
    }

    if (mStartTime != event.startTime.toString(Qt::ISODate)) {
        mStartTime = event.startTime.toString(Qt::ISODate);
        needSTEmit = true;
    }

    mBusy = false;

    if (needNUidEmit) {
        emit notebookUidChanged();
    }
    if (needUidEmit) {
        emit uidChanged();
    }
    if (needRidEmit) {
        emit ridChanged();
    }
    if (needSTEmit) {
        emit startTimeChanged();
    }

    emit busyChanged();
    emit queryFinished();
}
