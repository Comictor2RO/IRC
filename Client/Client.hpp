#pragma once

#include <vector>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

class Channel;

class Client{
    public:
        // Constructor
        Client(int fd) : fd(fd), auth(false), registered(false), markDeletion(false)
        {}

        // Getters
        int getFD() const
        {
            return fd;
        }

        std::string getUsername() const
        {
            return username;
        }

        std::string getNickname() const
        {
            return nickname;
        }

        std::string getRealname() const
        {
            return realname;
        }

        std::string getPassword() const
        {
            return password;
        }

        std::string getPrefix() const
        {
            if(!nickname.empty())
                return nickname + "!" + (username.empty() ? "*" : username) + "@localhost";
            return ("*!*@localhost");
        }

        bool isAuth() const
        {
            return auth;
        }

        bool isRegistered() const
        {
            return registered;
        }

        const std::string &getBuffer() const
        {
            return buffer;
        }

        // Setters
        void setUsername(const std::string &username)
        {
            this->username = username;
        }

        void setNickname(const std::string &nickname)
        {
            this->nickname = nickname;
        }

        void setRealname(const std::string &realname)
        {
            this->realname = realname;
        }

        void setPassword(const std::string &password)
        {
            this->password = password;
        }

        void setAuth(bool auth)
        {
            this->auth = auth;
        }

        // Buffer handling
        void appendToBuffer(const std::string &data)
        {
            buffer = buffer + data;
        }

        void clearBuffer()
        {
            buffer.clear();
        }

        // Sending methods
        void send(const std::string &msg)
        {
            std::string fullmsg = msg + "\r\n";
            ::send(fd, fullmsg.c_str(), fullmsg.size(), 0);
        }
    
        void sendError(const std::string &code, const std::string &msg)
        {
            send(":" + std::string("localhost") + " " + code + " " + (nickname.empty() ? "*" : nickname) + " " + msg);
        }

        void sendReply(const std::string &code, const std::string &msg)
        {
            send(":" + std::string("localhost") + " " + code + " " + (nickname.empty() ? "*" : nickname) + " " + msg);
        }

        // Registration
        void markForDeletion()
        {
            markDeletion = true;
        }

        bool shouldDelete() const
        {
            return markDeletion;
        }

        void tryRegister()
        {
            if(auth && !nickname.empty() && !username.empty() && !registered)
            {
                registered = true;

                send("001 " + nickname + " :Welcome to the IRC Network");
                send("002 " + nickname + " :Your host is localhost, running version 1.0");
                send("003 " + nickname + " :This server was created today");
                send("004 " + nickname + " localhost 1.0 itkol");
            }
        }

        // Channels
        void joinChannel(Channel *channel)
        {
            channels.push_back(channel);
        }

        void leaveChannel(Channel *channel)
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

        std::vector<Channel *> getChannels() const
        {
            return channels;
        }

        // Destructor
        ~Client()
        {
            close(fd);
        }

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