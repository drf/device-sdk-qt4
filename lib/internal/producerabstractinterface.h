/*
 * Copyright (C) 2017 Ispirata Srl
 *
 * This file is part of Astarte.
 * Astarte is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Astarte is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Astarte.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _HYPERSPACE_PLUS_PROVIDERABSTRACTINTERFACE_H_
#define _HYPERSPACE_PLUS_PROVIDERABSTRACTINTERFACE_H_

#include "abstractwavetarget.h"

#include <QtCore/QDateTime>

namespace Astarte {
    class Transport;
}

enum Retention {
    UnknownRetention = 0,
    Discard = 1,
    Volatile = 2,
    Stored = 3
};

enum Reliability {
    UnknownReliability = 0,
    Unreliable = 1,
    Guaranteed = 2,
    Unique = 3
};

class ProducerAbstractInterface : public AbstractWaveTarget
{
    Q_OBJECT

    public:
        enum DispatchResult {
            Success,
            CouldNotConvertPayload,
            IndexNotFound
        };

        ProducerAbstractInterface(const QByteArray &interface, Astarte::Transport *astarteTransport, QObject *parent);
        virtual ~ProducerAbstractInterface();

    protected:
        virtual void waveFunction(const Wave &wave);

        void insertTransition(int state, QByteArray token, int newState);
        void insertDispatchState(int state, int dispatchIndex);

        int dispatchIndex(const QList<QByteArray> &inputTokens);

        virtual void populateTokensAndStates() = 0;
        virtual ProducerAbstractInterface::DispatchResult dispatch(int i, const QByteArray &value, const QList<QByteArray> &inputTokens) = 0;

        void sendRawDataOnEndpoint(const QByteArray &value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>());

        void sendDataOnEndpoint(const QByteArray &value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(double value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(int value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(qint64 value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(bool value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(const QString &value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(const QDateTime &value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());
        void sendDataOnEndpoint(const QVariantHash &value, const QByteArray &target, const QHash<QByteArray, QByteArray> &attributes = QHash<QByteArray, QByteArray>(),
                                const QDateTime &timestamp = QDateTime(), const QVariantHash &metadata = QVariantHash());

        bool payloadToValue(const QByteArray &payload, QByteArray *value);
        bool payloadToValue(const QByteArray &payload, int *value);
        bool payloadToValue(const QByteArray &payload, qint64 *value);
        bool payloadToValue(const QByteArray &payload, bool *value);
        bool payloadToValue(const QByteArray &payload, double *value);
        bool payloadToValue(const QByteArray &payload, QString *value);
        bool payloadToValue(const QByteArray &payload, QDateTime *value);

    private:
        class Private;
        Private *const d;
};

#endif
