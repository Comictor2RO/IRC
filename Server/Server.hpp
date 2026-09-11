#ifndef __SERVER_HPP__
#define __SERVER_HPP__

//#include "../Channel/Channel.hpp" TO DO (important)
#include "../Client/Client.hpp"
#include "../IrcParser/IrcParser.hpp"
#include <string>
#include <iostream>
#include <vector>

class Server{
    public:
        Server(int port, std::string pass);

        void start(); //starts the server
        void stop(); //stops the server

        Client *getClientByFd(int fd);
        Client *getClientByNick(std::string &nick);
        void removeClient(Client *client);

        // TO DO
        void handlePass(IrcMessage &msg,Client *client);
        void handleNick(IrcMessage &msg,Client *client);
        void handleUser(IrcMessage &msg,Client *client);
        void handleJoin(IrcMessage &msg,Client *client);
        void handlePrivmsg(IrcMessage &msg,Client *client);
        void handleKick(IrcMessage &msg,Client *client);
        void handleInvite(IrcMessage &msg,Client *client);
        void handleTopic(IrcMessage &msg,Client *client);
        void handleMode(IrcMessage &msg,Client *client);
        

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