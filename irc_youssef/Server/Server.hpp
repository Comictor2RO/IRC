#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <vector>
#include <poll.h>
#include <map>
#include "Client.hpp"

class Server{
private:
    int         port;
    std::string password;
    int         server_fd; // socket d'ascolto, -1 se non aperto
    std::vector<pollfd> pollFds; // pollFds[0] + server_fd, il resto per i client
    std::map<int, Client*> clientsByFD;
    //std::map<std::string, Channel*>  channels;

public:
};

#endif