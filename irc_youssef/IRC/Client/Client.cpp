#include "Client.hpp"

Client::Client(int fd) : fd(fd), auth(false), registered(false), markDeletion(false)
{}

// Getters
int Client::getFD() const
{
    return fd;
}

std::string Client::getUsername() const
{
    return username;
}

std::string Client::getNickname() const
{
    return nickname;
}

std::string Client::getRealname() const
{
    return realname;
}

std::string Client::getPassword() const
{
    return Client::password;
}

std::string Client::getPrefix() const
{
    if(!nickname.empty())
        return nickname + "!" + (username.empty() ? "*" : username) + "@localhost";
    return ("*!*@localhost");
}

bool Client::isAuth() const
{
    return auth;
}

bool Client::isRegistered() const
{
    return registered;
}

const std::string &Client::getBuffer() const
{
    return buffer;
}

// Setters
void Client::setUsername(const std::string &username)
{
    this->username = username;
}

void Client::setNickname(const std::string &nickname)
{
    this->nickname = nickname;
}

void Client::setRealname(const std::string &realname)
{
    this->realname = realname;
}

void Client::setPassword(const std::string &password)
{
    this->password = password;
}

void Client::setAuth(bool auth)
{
    this->auth = auth;
}

// Buffer handling
void Client::appendToBuffer(const std::string &data)
{
    buffer = buffer + data;
}

void Client::clearBuffer()
{
    buffer.clear();
}

// Sending methods
void Client::send(const std::string &msg)
{
    std::string fullmsg = msg + "\r\n";
    std::cout << "DEBUG: Sending: [" << fullmsg << "]" << std::endl;
    ssize_t ret = ::send(fd, fullmsg.c_str(), fullmsg.size(), 0);
    std::cout << "DEBUG: send() returned: " << ret << std::endl;
    if (ret < 0)
        std::cerr << "DEBUG: send() error: " << strerror(errno) << std::endl;
}

void Client::sendError(const std::string &code, const std::string &msg)
{
    send(":" + std::string("localhost") + " " + code + " " + (nickname.empty() ? "*" : nickname) + " " + msg);
}

void Client::sendReply(const std::string &code, const std::string &msg)
{
    send(":" + std::string("localhost") + " " + code + " " + (nickname.empty() ? "*" : nickname) + " " + msg);
}

// Registration
void Client::markForDeletion()
{
    markDeletion = true;
}

bool Client::shouldDelete() const
{
    return markDeletion;
}

void Client::tryRegister()
{
    if(auth && !nickname.empty() && !username.empty() && !registered)
    {
        registered = true;
        send(":localhost 001 " + nickname + " :Welcome to the IRC Network");
        send(":localhost 002 " + nickname + " :Your host is localhost, running version 1.0");
        send(":localhost 003 " + nickname + " :This server was created today");
        send(":localhost 004 " + nickname + " localhost 1.0 itkol");
    }
}

// Channels
void Client::joinChannel(Channel *channel)
{
    channels.push_back(channel);
}

void Client::leaveChannel(Channel *channel)
{
    for(size_t i = 0; i < channels.size(); ++i)
    {
        if(channels[i] == channel)
        {
            channels.erase(channels.begin() + i);
            return;
        }
    }
}

std::vector<Channel *> Client::getChannels() const
{
    return channels;
}

// Destructor
Client::~Client()
{
    close(fd);
}