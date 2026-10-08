#ifndef SERVER_HPP                     // include guard: evita inclusioni multiple
#define SERVER_HPP                     // definisce la macro della guard

#include <string>                      // std::string: password, nick, nomi dei canali
#include <vector>                      // std::vector<pollfd>: l'array passato a poll()
#include <map>                         // std::map: indici dei client e dei canali
#include <csignal>                     // sig_atomic_t: tipo sicuro per il flag del segnale
#include <poll.h>                      // struct pollfd, poll(), POLLIN, POLLOUT...

class Client;                          // forward declaration: nell'header usiamo solo puntatori
class Channel;                         // idem: gli #include completi stanno in Server.cpp
struct IrcMessage;                     // idem: struttura prodotta dal parser (passata per riferimento)

/*
** =====================================================================================
**  CLASSE Server  —  l'orchestratore: possiede tutto e gira dentro UN SOLO poll()
** =====================================================================================
**  Responsabilità:
**   1. aprire il socket di ascolto (socket, setsockopt, fcntl, bind, listen);
**   2. eseguire il loop principale: UNA chiamata a poll() per iterazione che controlla
**      INSIEME il socket di ascolto e tutti i client (regola del subject);
**   3. accettare nuove connessioni, leggere i dati, dividerli in righe, passarle al parser
**      e smistare ogni comando all'handler giusto;
**   4. spedire i messaggi in coda solo quando poll() segnala POLLOUT;
**   5. rimuovere i client disconnessi e i canali rimasti vuoti, senza leak e senza fd aperti;
**   6. spegnersi in modo pulito con Ctrl+C (SIGINT) o Ctrl+\ (SIGQUIT).
**
**  Proprietà della memoria:
**   - clientsByFD  possiede i Client  (new in acceptNewClient, delete in removeClient);
**   - channels     possiede i Channel (new in createChannel, delete in removeChannelIfEmpty);
**   - clientsByNick contiene GLI STESSI puntatori di clientsByFD: è solo un indice di ricerca.
**
**  Gli HANDLER dei comandi (handlePass, handleJoin, ...) sono metodi di Server perché
**  devono cercare client e canali, ma sono scritti dagli altri membri del gruppo e
**  implementati in file separati (cartella _DA_ELIMINARE_codice_altri/Commands).
** =====================================================================================
*/
class Server
{
public:
    // ---------------------------------------------------------------- interfaccia pubblica
    Server(int port, const std::string &password); // salva i parametri (non apre ancora nulla)
    ~Server();                                     // chiude tutto e libera la memoria

    void        run();                             // apre il socket ed esegue il loop fino a Ctrl+C
    static void signalHandler(int signum);         // chiamata dal sistema operativo su SIGINT/SIGQUIT

private:
    // Puntatore a un metodo handler: void Server::handleXxx(const IrcMessage &, Client *)
    typedef void (Server::*CommandHandler)(const IrcMessage &msg, Client *client);

    // Copia vietata (C++98: private e non implementate): il Server possiede fd e memoria,
    // una copia porterebbe a chiudere due volte gli stessi fd e a fare double delete.
    Server(const Server &other);
    Server &operator=(const Server &other);

    // ---------------------------------------------------------------- dati
    int                                     port;           // porta TCP (av[1])
    std::string                             password;       // password di connessione (av[2])
    int                                     server_fd;      // socket di ascolto, -1 se non aperto
    std::vector<pollfd>                     pollFds;        // [0] = server_fd, poi un elemento per client
    std::map<int, Client *>                 clientsByFD;    // fd -> Client   (PROPRIETARIO)
    std::map<std::string, Client *>         clientsByNick;  // nick in minuscolo -> Client (solo indice)
    std::map<std::string, Channel *>        channels;       // nome in minuscolo -> Channel (PROPRIETARIO)
    std::map<std::string, CommandHandler>   commands;       // "JOIN" -> &Server::handleJoin, ...
    static volatile sig_atomic_t            stopSignal;     // 1 quando arriva Ctrl+C: il loop termina

    // ---------------------------------------------------------------- avvio
    void setupSignals();                    // SIGINT/SIGQUIT -> stop pulito, SIGPIPE ignorato
    void setupSocket();                     // socket -> SO_REUSEADDR -> O_NONBLOCK -> bind -> listen
    void initCommands();                    // riempie la tabella commands

    // ---------------------------------------------------------------- loop e I/O
    void buildPollFds();                    // ricostruisce pollFds partendo da server_fd + clientsByFD
    void handlePollEvents();                // esamina i revents dopo poll()
    void acceptNewClient();                 // accept() + O_NONBLOCK + new Client
    void readFromClient(Client *client);    // recv() -> buffer -> righe -> parser -> dispatch
    void writeToClient(Client *client);     // spedisce la coda di uscita (POLLOUT)
    void dispatchCommand(const IrcMessage &msg, Client *client); // controlla la registrazione e chiama l'handler
    void cleanupClients();                  // a fine iterazione elimina i client segnati

    // ---------------------------------------------------------------- gestione client
    void    removeClient(Client *client);                   // QUIT agli altri, esce dai canali, delete
    Client  *getClientByFd(int fd);                         // ricerca per fd
    Client  *getClientByNick(const std::string &nick);      // ricerca per nick (maiuscole/minuscole uguali)
    bool    isNickTaken(const std::string &nick, Client *exclude = NULL) const; // nick già usato da un altro?
    void    updateNick(Client *client, const std::string &newNick); // cambia nick tenendo coerente l'indice

    // ---------------------------------------------------------------- gestione canali
    Channel *getChannel(const std::string &name);           // ricerca per nome (NULL se non esiste)
    Channel *createChannel(const std::string &name);        // lo crea se non esiste e lo restituisce
    void    removeChannelIfEmpty(Channel *channel);         // se non ha membri: delete + erase

    // ---------------------------------------------------------------- utilità
    static std::string ircLower(const std::string &str);    // minuscolo: "Alice" e "alice" sono lo stesso nick

    // =================================================================================
    //  HANDLER DEI COMANDI — implementati dagli altri membri del gruppo
    //  (file in _DA_ELIMINARE_codice_altri/Commands/). Qui ci sono solo le dichiarazioni,
    //  necessarie perché sono metodi della classe Server.
    // =================================================================================
    void handlePass(const IrcMessage &msg, Client *client);     // PASS <password>
    void handleNick(const IrcMessage &msg, Client *client);     // NICK <nick>
    void handleUser(const IrcMessage &msg, Client *client);     // USER <user> 0 * :<realname>
    void handleCap(const IrcMessage &msg, Client *client);      // CAP LS / REQ / END (HexChat, irssi)
    void handlePing(const IrcMessage &msg, Client *client);     // PING <token> -> PONG
    void handlePong(const IrcMessage &msg, Client *client);     // PONG (ignorato)
    void handleQuit(const IrcMessage &msg, Client *client);     // QUIT [:motivo]
    void handleJoin(const IrcMessage &msg, Client *client);     // JOIN <#c>[,<#c2>] [<chiave>]
    void handlePart(const IrcMessage &msg, Client *client);     // PART <#c>[,<#c2>] [:motivo]
    void handleTopic(const IrcMessage &msg, Client *client);    // TOPIC <#c> [:<nuovo topic>]
    void handleKick(const IrcMessage &msg, Client *client);     // KICK <#c> <nick> [:motivo]
    void handleInvite(const IrcMessage &msg, Client *client);   // INVITE <nick> <#c>
    void handleMode(const IrcMessage &msg, Client *client);     // MODE <#c|nick> [modi] [parametri]
    void handleWho(const IrcMessage &msg, Client *client);      // WHO <#c|nick>
    void handlePrivmsg(const IrcMessage &msg, Client *client);  // PRIVMSG <dest>[,<dest>] :<testo>
    void handleNotice(const IrcMessage &msg, Client *client);   // NOTICE <dest> :<testo>
    void handleChannelMode(const IrcMessage &msg, Client *client, Channel *channel);      // parte "canale" di MODE
    void deliverMessage(const IrcMessage &msg, Client *client, const std::string &command,
                        bool reportErrors);                     // logica comune a PRIVMSG e NOTICE
};

#endif                                 // fine include guard
