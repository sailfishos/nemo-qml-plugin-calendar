/*
 * Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>.
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

#include "test_plugin.h"

#include <QDebug>

const QString NAME("TestInvitationPlugin");

TestInvitationPlugin::TestInvitationPlugin()
{
}

TestInvitationPlugin::~TestInvitationPlugin()
{
}

bool TestInvitationPlugin::sendInvitation(const QString &accountId, const QString &notebookUid,
                                          const KCalendarCore::Incidence::Ptr &invitation, const QString &body)
{
    Q_UNUSED(accountId);
    Q_UNUSED(notebookUid);
    Q_UNUSED(invitation);
    Q_UNUSED(body);

    mSentInvitation = invitation;

    return true;
}

bool TestInvitationPlugin::sendUpdate(const QString &accountId, const KCalendarCore::Incidence::Ptr &invitation,
                                         const QString &body)
{
    Q_UNUSED(accountId);
    Q_UNUSED(invitation);
    Q_UNUSED(body);

    mUpdatedInvitations << invitation;

    return true;
}

bool TestInvitationPlugin::sendResponse(const QString &accountId, const KCalendarCore::Incidence::Ptr &invitation,
                                           const QString &body)
{
    Q_UNUSED(accountId);
    Q_UNUSED(invitation);
    Q_UNUSED(body);

    return true;
}

QString TestInvitationPlugin::pluginName() const
{
    return NAME;
}

QString TestInvitationPlugin::icon() const
{
    return QString();
}

QString TestInvitationPlugin::uiName() const
{
    return QLatin1String("Test");
}

bool TestInvitationPlugin::multiCalendar() const
{
    return false;
}

QString TestInvitationPlugin::emailAddress(const mKCal::Notebook::Ptr &notebook)
{
    return notebook->customProperty("TEST_EMAIL");
}

QString TestInvitationPlugin::displayName(const mKCal::Notebook::Ptr &notebook) const
{
    return notebook->name();
}

bool TestInvitationPlugin::downloadAttachment(const mKCal::Notebook::Ptr &notebook, const QString &uri,
                                                 const QString &path)
{
    Q_UNUSED(notebook);
    Q_UNUSED(uri);
    Q_UNUSED(path);
    return false;
}

bool TestInvitationPlugin::deleteAttachment(const mKCal::Notebook::Ptr &notebook, const KCalendarCore::Incidence::Ptr &incidence,
                                               const QString &uri)
{
    Q_UNUSED(notebook);
    Q_UNUSED(incidence);
    Q_UNUSED(uri);
    return false;
}

bool TestInvitationPlugin::shareNotebook(const mKCal::Notebook::Ptr &notebook, const QStringList &sharedWith)
{
    notebook->setSharedWith(sharedWith);
    return true;
}

QStringList TestInvitationPlugin::sharedWith(const mKCal::Notebook::Ptr &notebook)
{
    return notebook->sharedWith();
}

QString TestInvitationPlugin::TestInvitationPlugin::serviceName() const
{
    return NAME;
}


QString TestInvitationPlugin::defaultNotebook() const
{
    return QString();
}

bool TestInvitationPlugin::checkProductId(const QString &prodId) const
{
    Q_UNUSED(prodId);
    return false;
}

ServiceInterface::ErrorCode TestInvitationPlugin::error() const
{
    return ServiceInterface::ErrorOk;
}

KCalendarCore::Incidence::Ptr TestInvitationPlugin::sentInvitation() const
{
    return mSentInvitation;
}

KCalendarCore::Incidence::List TestInvitationPlugin::updatedInvitations() const
{
    return mUpdatedInvitations;
}
