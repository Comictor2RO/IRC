#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "../Channel/Channel.hpp"
#include "../Client/Client.hpp"
#include "../IrcParser/IrcParser.hpp"
#include <string>
#include <iostream>
#include <cmath>
#include <vector>

class Server{
    public:
        Server(int port, std::string pass);

        void start(); //starts the server
        void stop(); //stops the server

        Client *getClientByFd(int fd);
        Client *getClientByNick(const std::string &nick);
        void removeClient(Client *client);

        Channel* getChannel(const std::string& name);
        Channel* createChannel(const std::string& name);
        bool isNickTaken(const std::string& nick, Client* exclude = NULL) const;

        // TO DO
        void handlePass(const IrcMessage &msg, Client *client);
        void handleNick(const IrcMessage &msg, Client *client);
        void handleUser(const IrcMessage &msg, Client *client);
        void handleJoin(const IrcMessage &msg, Client *client);
        void handleWho(const IrcMessage &msg, Client *client);
        void handlePrivmsg(const IrcMessage &msg, Client *client);
        void handleKick(const IrcMessage &msg, Client *client);
        void handleInvite(const IrcMessage &msg, Client *client);
        void handleTopic(const IrcMessage &msg, Client *client);
        void handleMode(const IrcMessage &msg, Client *client);
        void handleChannelMode(const IrcMessage &msg, Client *client, const std::string &channelName);
        void handleQuit(const IrcMessage &msg, Client *client);
        

        ~Server();

    private:
        int port;
        std::string pass;
        int server_fd;
        bool running;
        std::vector<Client*> clients;
        std::vector<Channel*> channels;
};

#endif