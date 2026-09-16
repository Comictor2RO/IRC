#pragma once

#include <vector>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>

class Channel;

class Client{
    public:
        // Constructor
        Client(int fd);

        // Getters
        int getFD() const;
        std::string getUsername() const;
        std::string getNickname() const;
        std::string getRealname() const;
        std::string getPassword() const;
        std::string getPrefix() const;
        bool isAuth() const;
        bool isRegistered() const;
        const std::string &getBuffer() const;

        // Setters
        void setUsername(const std::string &username);
        void setNickname(const std::string &nickname);
        void setRealname(const std::string &realname);
        void setPassword(const std::string &password);
        void setAuth(bool auth);

        // Buffer handling
        void appendToBuffer(const std::string &data);
        void clearBuffer();

        // Sending methods
        void send(const std::string &msg);

        void sendError(const std::string &code, const std::string &msg);
        void sendReply(const std::string &code, const std::string &msg);

        // Registration
        void markForDeletion();
        bool shouldDelete() const;
        void tryRegister();

        // Channels
        void joinChannel(Channel *channel);
        void leaveChannel(Channel *channel);
        std::vector<Channel *> getChannels() const;

        // Destructor
        ~Client();

    private:
        int fd;
        std::string username;
        std::string nickname;
        std::string realname;
        std::string password;
        std::string buffer;
        bool auth;
        bool registered;
        bool markDeletion;
        std::vector<Channel *> channels;
};