#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <poll.h>
#include <map>

class Client{
private:
    /* data */
    int _serverFd;
    int _port;
    std::string _password;
    std::vector<pollfd> _pollFds;
    std::map<int, Client> _clients;
    bool _isRegistred;

public:
    Client(int fd);
    ~Client();
    int getFd();
    const std::string& getNickname(); //torna un riferimeto a una strina costante per leggere il nickname senza copiarlo
    const std::string& getUsername() const; //torna un riferimento COSTANTE 
    const std::string& getBuffer() const; //serivira' nel server per controllare se nel buffer e' arrivato un \r\n
    bool isRegistred() const; //per sapere se l'utente ha superato la fase di login (PASS / NICK / USER)

};

#endif