//
// Created by lorenzodonini on 13.11.16.
//

#include "ConnectionManager.h"

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
    std::unique_lock<std::mutex> lock{connectionsMutex};

    return (int)openConnections.size();
}

std::shared_ptr<Client> ConnectionManager::getClient(uint8_t localPort) {
    std::unique_lock<std::mutex> lock{connectionsMutex};

    return clients[localPort];
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