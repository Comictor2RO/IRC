#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <vector>
#include <poll.h>
#include <map>
#include "Client.hpp"
#include "Channel.hpp"
#include "../../IrcParser/IrcParser.hpp"

class Server{
private:
    int         port; //parametri d av[1]/av[2]
    std::string password; //serve in handlePass()
    int         server_fd; // socket d'ascolto, -1 se non aperto
    bool        running;
    std::vector<pollfd> pollFds; // pollFds[0] + server_fd, il resto per i client 
    //la funzione poll() vuole un ARRAY contiguo di struct pollfd; il vector 
    //garatisce contiguita' (&pollFds[0]). Tiene sempre l'indice 0 per il listener
    std::map<int, Client*> clientsByFD; // e' l'owner principale, chiave + fd, perche' 
    //poll() ti dice quale fd e' pronto e tu devi risalire al client in O(log(n)).
    std::map<std::string, Client*> clientsByNick;//indice SECONDARIO (non possiede 
    // nulla: stessi puntatori di clientsByFD) serve per PRIVMSG/INVITE/KICK/WHO 
    // su un NICK non ha ancora un entry qui (nick vuoto -> NON inserirlo, altrimenti
    // ne avresti una sola per tutti i "senza nick").
    std::map<std::string, Channel*>  channels; // chiave = nome canale (es. "#general"); owner dei channel

    //Handlers copiati direttamente dalla versione vecchia me messi qui come private

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

public:
    Server(int port, const std::string &password);

    void start(); // init socket + loop princpale (bloccnte fino a stop/SIGINT)
    void storp(); // running = false

    ~Server();
};

#endif