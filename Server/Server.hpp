#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <string>
#include <iostream>
#include <vector>

class Server{
    public:
        Server(int port, std::string pass);

        void start(); //starts the server
        void stop(); //stops the server

        ~Server();

    private:
        int port;
        std::string pass;
        int server_fd;
        bool running;
};

#endif