/*
 * This file is a part of the Voice Call Manager project
 *
 * Copyright (C) 2011-2012  Tom Swindell <t.swindell@rubyx.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include "common.h"

#include "telepathyhandler.h"
#include "telepathyprovider.h"

#include <TelepathyQt/PendingReady>
#include <TelepathyQt/PendingChannel>
#include <TelepathyQt/PendingChannelRequest>

class TelepathyProviderPrivate
{
    Q_DECLARE_PUBLIC(TelepathyProvider)

public:
    TelepathyProviderPrivate(Tp::AccountPtr a, VoiceCallManagerInterface *m, TelepathyProvider *q)
        : q_ptr(q), manager(m), account(a),
          errorString(QString::null),
          tpPendingChannel(NULL)
    { /* ... */ }

    TelepathyProvider           *q_ptr;
    VoiceCallManagerInterface   *manager;

    Tp::AccountPtr               account;

    QString                      errorString;

    QHash<QString,AbstractVoiceCallHandler*> voiceCalls;

    Tp::PendingChannel      *tpPendingChannel;
};

TelepathyProvider::TelepathyProvider(Tp::AccountPtr account, VoiceCallManagerInterface *manager, QObject *parent)
    : AbstractVoiceCallProvider(parent),
      d_ptr(new TelepathyProviderPrivate(account, manager, this))
{
    TRACE
    QObject::connect(account.data()->becomeReady(), SIGNAL(finished(Tp::PendingOperation*)), SLOT(onAccountBecomeReady(Tp::PendingOperation*)));
}

TelepathyProvider::~TelepathyProvider()
{
    TRACE
    delete d_ptr;
}

QString TelepathyProvider::errorString() const
{
    TRACE
    Q_D(const TelepathyProvider);
    return d->errorString;
}

QString TelepathyProvider::providerId() const
{
    TRACE
    Q_D(const TelepathyProvider);
    return QString("telepathy-") + d->account->uniqueIdentifier();
}

QString TelepathyProvider::providerType() const
{
    TRACE
    Q_D(const TelepathyProvider);
    return d->account.data()->protocolName();
}

QList<AbstractVoiceCallHandler*> TelepathyProvider::voiceCalls() const
{
    TRACE
    Q_D(const TelepathyProvider);
    return d->voiceCalls.values();
}

bool TelepathyProvider::dial(const QString &msisdn)
{
    TRACE
    Q_D(TelepathyProvider);
    if(d->tpPendingChannel)
    {
        d->errorString = "Can't initiate a call when one is pending!";
        WARNING_T(d->errorString);
        emit this->error(d->errorString);
        return false;
    }

    if (d->account->protocolName() == "sip") {
        d->tpPendingChannel = d->account->ensureAndHandleAudioCall(msisdn);
        QObject::connect(d->tpPendingChannel,
                         SIGNAL(finished(Tp::PendingOperation*)),
                         SLOT(onDialFinished(Tp::PendingOperation*)));
    } else if (d->account->protocolName() == "tel") {
        d->tpPendingChannel = d->account->ensureAndHandleStreamedMediaAudioCall(msisdn);
        QObject::connect(d->tpPendingChannel,
                         SIGNAL(finished(Tp::PendingOperation*)),
                         SLOT(onDialFinished(Tp::PendingOperation*)));
     } else {
        d->errorString = "Attempting to dial an unknown protocol";
        WARNING_T(d->errorString);
        emit this->error(d->errorString);
        return false;
    }

    return true;
}

void TelepathyProvider::onAccountBecomeReady(Tp::PendingOperation *op)
{
    TRACE
    Q_D(TelepathyProvider);
    if(op->isError())
    {
        WARNING_T(QString("Operation failed: ") + op->errorName() + ": " + op->errorMessage());
        d->errorString = QString("Telepathy Operation Failed: %1 - %2").arg(op->errorName(), op->errorMessage());
        emit this->error(d->errorString);
        return;
    }

    DEBUG_T(QString("Account %1 became ready.").arg(d->account.data()->uniqueIdentifier()));

    QObject::connect(d->account.data(), SIGNAL(stateChanged(bool)), SLOT(onAccountAvailabilityChanged()));
    QObject::connect(d->account.data(), SIGNAL(onlinenessChanged(bool)), SLOT(onAccountAvailabilityChanged()));
    QObject::connect(d->account.data(), SIGNAL(connectionStatusChanged(Tp::ConnectionStatus)), SLOT(onAccountAvailabilityChanged()));
    this->onAccountAvailabilityChanged();
}

void TelepathyProvider::onAccountAvailabilityChanged()
{
    TRACE
    Q_D(TelepathyProvider);

    if(d->account.data()->isEnabled() && d->account.data()->isOnline() && d->account.data()->connectionStatus() == Tp::ConnectionStatusConnected)
    {
        d->manager->appendProvider(this);
    }
    else
    {
        d->manager->removeProvider(this);
    }
}

void TelepathyProvider::createHandler(Tp::ChannelPtr ch, const QDateTime &userActionTime)
{
    TRACE
    Q_D(TelepathyProvider);
    DEBUG_T(QString("\tProcessing channel: %1").arg(ch->objectPath()));
    TelepathyHandler *handler = new TelepathyHandler(d->manager->generateHandlerId(), ch, userActionTime, this);
    d->voiceCalls.insert(handler->handlerId(), handler);

    QObject::connect(handler, SIGNAL(error(QString)), SIGNAL(error(QString)));

    QObject::connect(handler,
                     SIGNAL(invalidated(QString,QString)),
                     SLOT(onHandlerInvalidated(QString,QString)));

    emit this->voiceCallAdded(handler);
    emit this->voiceCallsChanged();
}

void TelepathyProvider::onDialFinished(Tp::PendingOperation *op)
{
    TRACE
    Q_D(TelepathyProvider);
    if(op->isError())
    {
        WARNING_T(QString("Operation failed: ") + op->errorName() + ": " + op->errorMessage());
        d->tpPendingChannel = NULL;
        d->errorString = QString("Telepathy Operation Failed: %1 - %2").arg(op->errorName(), op->errorMessage());
        emit this->error(d->errorString);
        return;
    }

    this->createHandler(d->tpPendingChannel->channel(), QDateTime::currentDateTime());
    d->tpPendingChannel = NULL;

    emit this->voiceCallsChanged();
}

void TelepathyProvider::onHandlerInvalidated(const QString &errorName, const QString &errorMessage)
{
    TRACE
    Q_UNUSED(errorName)
    Q_UNUSED(errorMessage)
    Q_D(TelepathyProvider);

    TelepathyHandler *handler = qobject_cast<TelepathyHandler*>(QObject::sender());
    d->voiceCalls.remove(handler->handlerId());

    emit this->voiceCallRemoved(handler->handlerId());
    emit this->voiceCallsChanged();

    handler->deleteLater();
}
