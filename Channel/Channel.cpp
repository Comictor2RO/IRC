#include "Channel.hpp"
#include "../Client/Client.hpp"
#include <iostream>

Channel::Channel(const std::string& name)
    : name(name), limit(0), inviteOnly(false), topicRestricted(false)
{}

Channel::~Channel() {}

std::string Channel::getName() const {
    return name;
}

std::string Channel::getTopic() const {
    return topic;
}

std::string Channel::getKey() const {
    return key;
}

int Channel::getLimit() const {
    return limit;
}

std::string Channel::getModes() const {
    std::string modes;
    if (inviteOnly) modes += "i";
    if (topicRestricted) modes += "t";
    if (hasKey()) modes += "k";
    if (limit > 0) modes += "l";
    return modes;
}

bool Channel::isInviteOnly() const {
    return inviteOnly;
}

bool Channel::isTopicRestricted() const {
    return topicRestricted;
}

bool Channel::hasKey() const {
    return !key.empty();
}

bool Channel::isFull() const {
    return limit > 0 && (int)clients.size() >= limit;
}

// === SETTERS ===

void Channel::setTopic(const std::string& t) {
    topic = t;
}

void Channel::setKey(const std::string& k) {
    key = k;
}

void Channel::setLimit(int l) {
    limit = l;
}

void Channel::setInviteOnly(bool io) {
    inviteOnly = io;
}

void Channel::setTopicRestricted(bool tr) {
    topicRestricted = tr;
}


void Channel::addClient(Client& client) {
    clients.push_back(&client);
    client.joinChannel(this);
    
    if (clients.size() == 1)
        addOperator(client);
}

void Channel::removeClient(Client& client) {
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == &client) {
            bool wasOperator = isOperator(client);
            clients.erase(clients.begin() + i);
            client.leaveChannel(this);
            removeOperator(client);

            if (wasOperator && operators.empty() && !clients.empty())
                addOperator(*clients[0]);
            break;
        }
    }
}

bool Channel::hasClient(Client& client) const {
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == &client)
            return true;
    }
    return false;
}

std::vector<Client*> Channel::getClients() const {
    return clients;
}

int Channel::getClientCount() const {
    return clients.size();
}

void Channel::addOperator(Client& client) {
    operators.insert(&client);
}

void Channel::removeOperator(Client& client) {
    operators.erase(&client);
}

bool Channel::isOperator(Client& client) const {
    return operators.find(&client) != operators.end();
}

void Channel::addInvite(Client& client) {
    invites.insert(&client);
}

bool Channel::isInvited(Client& client) const {
    return invites.find(&client) != invites.end();
}

void Channel::removeInvite(Client& client) {
    invites.erase(&client);
}


void Channel::addBan(const std::string& mask) {
    bans.insert(mask);
}

void Channel::removeBan(const std::string& mask) {
    bans.erase(mask);
}

bool Channel::isBanned(Client& client) const {
    for (std::set<std::string>::const_iterator it = bans.begin(); it != bans.end(); ++it) {
        if (client.getNickname() == *it)
            return true;
    }
    return false;
}

void Channel::broadcast(const std::string& msg, Client* exclude) {
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] != exclude)
            clients[i]->send(msg);
    }
}

void Channel::sendTopic(Client& client) {
    if (!topic.empty())
        client.sendReply("332", name + " :" + topic);
    else
        client.sendReply("331", name + " :No topic is set");
}

void Channel::sendNames(Client& client) {
    std::string names;
    for (size_t i = 0; i < clients.size(); i++) {
        if (i > 0)
            names += " ";
        if (isOperator(*clients[i]))
            names += "@";
        names += clients[i]->getNickname();
    }
    client.sendReply("353", "= " + name + " :" + names);
}