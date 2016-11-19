//
// Created by lorenzodonini on 13.11.16.
//

#include "ConnectionManager.h"

std::shared_ptr<Connection> ConnectionManager::getConnection(uint8_t localPort) {
    connectionsMutex.lock();
    std::shared_ptr<Connection> connection = openConnections[localPort];
    connectionsMutex.unlock();

    return connection;
}

bool ConnectionManager::handleNewConnection(S3TP_PACKET * newConnectionRequest) {
    connectionsMutex.lock();

    if (openConnections[newConnectionRequest->getHeader()->srcPort]) {
        //Port is open already
        connectionsMutex.unlock();
        return false;
    }
    std::shared_ptr<Connection> connection(new Connection(newConnectionRequest));
    openConnections[newConnectionRequest->getHeader()->srcPort];
    connectionsMutex.unlock();

    return true;
}

bool ConnectionManager::openConnection(uint8_t srcPort, uint8_t destPort, uint8_t virtualChannel, uint8_t options) {
    connectionsMutex.lock();
    if (openConnections[srcPort]) {
        //Port is open already
        connectionsMutex.unlock();
        return false;
    }
    std::shared_ptr<Connection> connection(new Connection(srcPort, destPort, virtualChannel, options));
    openConnections[srcPort] = connection;
    connectionsMutex.unlock();

    return true;
}

bool ConnectionManager::closeConnection(uint8_t srcPort) {
    connectionsMutex.lock();
    std::shared_ptr<Connection> connection = openConnections[srcPort];
    if (connection == nullptr) {
        //Port was closed to begin with
        connectionsMutex.unlock();
        return false;
    }
    Connection::STATE state = (*connection).getCurrentState();
    if (state == Connection::DISCONNECTING || state == Connection::DISCONNECTED) {
        //Cannot close port right now
        connectionsMutex.unlock();
        return false;
    }
    (*connection).close();
    connectionsMutex.unlock();

    return true;
}

int ConnectionManager::openConnectionsCount() {
    connectionsMutex.lock();
    int openConnectionsCount = (int)openConnections.size();
    connectionsMutex.unlock();

    return openConnectionsCount;
}

/*
 * Listener setters
 */
void ConnectionManager::setInPacketListener(InPacketListener * listener) {
    this->inListener = listener;
}

void ConnectionManager::setOutPacketListener(OutPacketListener * listener) {
    this->outListener = listener;
}

/*
 * Connection Listener callbacks
 */
void ConnectionManager::onConnectionStatusChanged(Connection& connection) {
    if (connection.getCurrentState() == Connection::DISCONNECTED) {
        connectionsMutex.lock();
        openConnections.erase(connection.getSourcePort());
        connectionsMutex.unlock();
    }
    //TODO: other status to handle?
}

void ConnectionManager::onConnectionOutOfBandRequested(S3TP_PACKET * pkt) {

}

void ConnectionManager::onConnectionError(Connection& connection, std::string error) {

}

void ConnectionManager::onNewOutPacket(Connection& connection) {
    if (outListener != nullptr) {
        outListener->onNewOutPacket(connection);
    }
}

void ConnectionManager::onNewInPacket(Connection& connection) {
    if (inListener != nullptr) {
        inListener->onNewInPacket(connection);
    }
}