#include "Server.hpp"                  // dichiarazione della classe Server
#include "Client.hpp"                  // classe Client completa (ne chiamiamo i metodi)
#include "Channel.hpp"                 // classe Channel completa
#include "IrcParser.hpp"               // IrcParser::parse() e struct IrcMessage (codice degli altri)
#include <iostream>                    // std::cout / std::cerr per i log
#include <stdexcept>                   // std::runtime_error: errori fatali di avvio
#include <set>                         // std::set: destinatari del QUIT senza duplicati
#include <cstring>                     // std::memset, std::strerror
#include <cerrno>                      // errno (solo dopo socket/bind/listen/poll, MAI dopo recv/send)
#include <cctype>                      // std::tolower
#include <unistd.h>                    // close()
#include <fcntl.h>                     // fcntl(), F_SETFL, O_NONBLOCK
#include <sys/socket.h>                // socket(), setsockopt(), bind(), listen(), accept(), recv()
#include <netinet/in.h>                // struct sockaddr_in, INADDR_ANY, htons(), htonl()
#include <arpa/inet.h>                 // inet_ntoa(): IP binario -> "127.0.0.1"

// Lunghezza massima di una riga IRC: 512 byte compreso "\r\n" (RFC 1459), quindi 510 di testo.
#define MAX_LINE_LENGTH 510

// Definizione del membro statico: deve esistere in UN solo file .cpp. Parte a 0 = "non fermarti".
volatile sig_atomic_t Server::stopSignal = 0;

/* =====================================================================================
**  COSTRUTTORE
**  Salva porta e password e prepara la tabella dei comandi. NON apre il socket: lo fa
**  run(). Così un errore di avvio (es. porta occupata) viene gestito in un solo punto
**  (eccezione catturata nel main) e il distruttore resta sempre sicuro (server_fd = -1).
** ===================================================================================== */
Server::Server(int port, const std::string &password)
    : port(port),                      // porta su cui ascoltare
      password(password),              // password richiesta da PASS
      server_fd(-1)                    // -1 = socket non ancora aperto
{
    initCommands();                    // collega i nomi dei comandi ai rispettivi handler
}

/* =====================================================================================
**  DISTRUTTORE
**  Chiamato all'uscita dal main (dopo Ctrl+C). Libera TUTTO:
**   1. delete di ogni Client (il distruttore di Client chiude il suo fd);
**   2. delete di ogni Channel;
**   3. chiusura del socket di ascolto.
**  Risultato atteso con valgrind: 0 leak e nessun fd rimasto aperto (--track-fds=yes).
**  Non si mandano messaggi QUIT: il server si sta spegnendo e i client vedranno la chiusura.
** ===================================================================================== */
Server::~Server()
{
    for (std::map<int, Client *>::iterator it = clientsByFD.begin(); it != clientsByFD.end(); ++it) // ogni client
        delete it->second;             // libera il Client e chiude il suo fd
    clientsByFD.clear();               // la map ora contiene solo puntatori non validi: svuotala
    clientsByNick.clear();             // stessi puntatori: basta svuotarla (NON fare delete due volte)
    for (std::map<std::string, Channel *>::iterator it = channels.begin(); it != channels.end(); ++it) // ogni canale
        delete it->second;             // libera il Channel
    channels.clear();                  // svuota la map
    if (server_fd >= 0)                // il socket di ascolto era aperto?
        close(server_fd);              // chiudilo
}

/* =====================================================================================
**  signalHandler()  [static]
**  Il sistema operativo la chiama in modo ASINCRONO quando arriva SIGINT (Ctrl+C) o
**  SIGQUIT (Ctrl+\). Dentro un signal handler si possono fare pochissime cose in modo
**  sicuro: niente cout, niente new/delete. Per questo imposta soltanto un flag di tipo
**  volatile sig_atomic_t (l'unico tipo che lo standard garantisce sicuro qui).
**  È static perché il sistema chiama una funzione normale, non un metodo di un oggetto.
**  Cosa succede dopo: poll() viene interrotto e ritorna -1 con errno == EINTR; il loop in
**  run() fa "continue", ricontrolla stopSignal e termina. Poi il distruttore pulisce tutto.
** ===================================================================================== */
void Server::signalHandler(int signum)
{
    (void)signum;                      // il parametro non serve: evita il warning -Wunused-parameter
    stopSignal = 1;                    // chiede al loop di fermarsi
}

/* =====================================================================================
**  setupSignals()
**   - SIGINT  (Ctrl+C)  e SIGQUIT (Ctrl+\) -> signalHandler: spegnimento pulito;
**   - SIGPIPE -> SIG_IGN (ignorato). IMPORTANTE: se si scrive su un socket che il client ha
**     già chiuso, il kernel manda SIGPIPE, che di default UCCIDE il processo. Un client che
**     chiude nel momento sbagliato farebbe crashare tutto il server. Ignorandolo, send()
**     ritorna semplicemente -1 e il client viene rimosso.
** ===================================================================================== */
void Server::setupSignals()
{
    stopSignal = 0;                        // riparte "da acceso"
    signal(SIGINT, Server::signalHandler); // Ctrl+C
    signal(SIGQUIT, Server::signalHandler);// Ctrl+'\'
    signal(SIGPIPE, SIG_IGN);              // scrittura su socket chiuso: non morire
}

/* =====================================================================================
**  setupSocket()
**  Crea il socket di ascolto. Ogni passo può fallire: in quel caso si lancia un'eccezione
**  (il main la stampa ed esce; il distruttore chiude server_fd se già aperto).
**   1. socket(AF_INET, SOCK_STREAM, 0): socket TCP su IPv4;
**   2. setsockopt(SO_REUSEADDR): permette di riavviare subito il server sulla stessa porta
**      (senza, dopo un riavvio la porta resta "occupata" per circa un minuto: TIME_WAIT);
**   3. fcntl(fd, F_SETFL, O_NONBLOCK): non bloccante (forma richiesta dal subject per macOS);
**      così accept() non può mai bloccare il server;
**   4. bind() su 0.0.0.0:<porta> (INADDR_ANY = tutte le interfacce di rete). htons/htonl
**      convertono i numeri nel "network byte order" (big endian) richiesto dalla rete;
**   5. listen(SOMAXCONN): il socket inizia ad accettare connessioni; SOMAXCONN è la lunghezza
**      massima della coda delle connessioni non ancora accettate.
** ===================================================================================== */
void Server::setupSocket()
{
    server_fd = socket(AF_INET, SOCK_STREAM, 0);                        // 1. crea il socket TCP IPv4
    if (server_fd < 0)                                                  // errore?
        throw std::runtime_error(std::string("socket: ") + std::strerror(errno)); // messaggio chiaro

    int opt = 1;                                                        // 1 = attiva l'opzione
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) // 2. riuso della porta
        throw std::runtime_error(std::string("setsockopt: ") + std::strerror(errno));

    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0)                      // 3. modalità non bloccante
        throw std::runtime_error(std::string("fcntl: ") + std::strerror(errno));

    sockaddr_in addr;                                                   // indirizzo IPv4 + porta
    std::memset(&addr, 0, sizeof(addr));                                // azzera tutti i campi
    addr.sin_family = AF_INET;                                          // famiglia IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);                           // 0.0.0.0: ogni interfaccia
    addr.sin_port = htons(static_cast<unsigned short>(port));           // porta in network byte order
    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) // 4. lega socket e porta
        throw std::runtime_error(std::string("bind: ") + std::strerror(errno)); // es. porta già in uso

    if (listen(server_fd, SOMAXCONN) < 0)                               // 5. inizia ad ascoltare
        throw std::runtime_error(std::string("listen: ") + std::strerror(errno));
}

/* =====================================================================================
**  initCommands()
**  Riempie la tabella "nome comando -> metodo handler". dispatchCommand() la usa per
**  trovare l'handler con una ricerca nella map invece di una lunga catena di if/else.
**  &Server::handlePass è un "puntatore a metodo": si chiama con (this->*handler)(msg, c).
**  Per aggiungere un comando nuovo basta una riga qui + il suo handler.
** ===================================================================================== */
void Server::initCommands()
{
    commands["PASS"] = &Server::handlePass;        // autenticazione
    commands["NICK"] = &Server::handleNick;        // nickname
    commands["USER"] = &Server::handleUser;        // username / realname
    commands["CAP"] = &Server::handleCap;          // negoziazione capacità (client moderni)
    commands["PING"] = &Server::handlePing;        // keep-alive
    commands["PONG"] = &Server::handlePong;        // risposta al keep-alive
    commands["QUIT"] = &Server::handleQuit;        // disconnessione
    commands["JOIN"] = &Server::handleJoin;        // entra in un canale
    commands["PART"] = &Server::handlePart;        // esce da un canale
    commands["TOPIC"] = &Server::handleTopic;      // argomento del canale
    commands["KICK"] = &Server::handleKick;        // espulsione (operatori)
    commands["INVITE"] = &Server::handleInvite;    // invito (operatori)
    commands["MODE"] = &Server::handleMode;        // modi i/t/k/o/l (operatori)
    commands["WHO"] = &Server::handleWho;          // lista utenti (usato da HexChat dopo JOIN)
    commands["PRIVMSG"] = &Server::handlePrivmsg;  // messaggi
    commands["NOTICE"] = &Server::handleNotice;    // messaggi senza risposte d'errore
}

/* =====================================================================================
**  run()  —  IL LOOP PRINCIPALE
**  Schema di ogni iterazione:
**    1. buildPollFds(): prepara l'array di pollfd (ascolto + tutti i client);
**    2. poll(..., -1): il processo DORME (0% CPU) finché almeno un fd è pronto;
**    3. handlePollEvents(): nuove connessioni, letture, scritture, errori;
**    4. cleanupClients(): rimuove i client segnati durante questa iterazione.
**  poll() è l'UNICA chiamata bloccante del programma ed è una sola per iterazione:
**  esattamente quanto richiede il subject.
**  Se poll() ritorna -1 con errno == EINTR significa che è stato interrotto da un segnale:
**  non è un errore vero, si ricomincia e il while ricontrolla stopSignal.
** ===================================================================================== */
void Server::run()
{
    setupSignals();                                                    // gestione Ctrl+C / SIGPIPE
    setupSocket();                                                     // apre il socket (può lanciare eccezioni)
    std::cout << "[server] in ascolto sulla porta " << port << std::endl; // log di avvio
    while (!stopSignal)                                                // finché non arriva Ctrl+C
    {
        buildPollFds();                                                // 1. prepara i pollfd
        int ready = poll(&pollFds[0], pollFds.size(), -1);             // 2. attende eventi (timeout infinito)
        if (ready < 0)                                                 // poll fallito o interrotto
        {
            if (errno == EINTR)                                        // interrotto da un segnale
                continue;                                              // il while ricontrolla stopSignal
            std::cerr << "[server] poll: " << std::strerror(errno) << std::endl; // errore vero
            break;                                                     // esci dal loop
        }
        handlePollEvents();                                            // 3. gestisce gli eventi
        cleanupClients();                                              // 4. rimuove i client segnati
    }
    std::cout << "\n[server] spegnimento in corso..." << std::endl;   // il distruttore farà il resto
}

/* =====================================================================================
**  buildPollFds()
**  Ricostruisce DA ZERO il vettore di pollfd a ogni iterazione. In questo modo c'è una sola
**  fonte di verità (clientsByFD) e non si fanno mai erase() sul vettore mentre lo si scorre
**  (era un bug della versione precedente: indici sfasati e client "fantasma").
**  Per ogni fd si sceglie a quali eventi siamo interessati:
**   - server_fd: POLLIN = "c'è una connessione da accettare";
**   - client normale: POLLIN = "ci sono dati da leggere";
**   - client con coda di uscita non vuota: anche POLLOUT = "si può scrivere senza bloccare";
**   - client segnato da eliminare: NON si legge più nulla (solo POLLOUT, per spedire gli
**     ultimi messaggi come "ERROR :Closing Link"). POLLERR/POLLHUP arrivano comunque.
**  Il vector garantisce memoria contigua: &pollFds[0] è un array C valido per poll().
** ===================================================================================== */
void Server::buildPollFds()
{
    pollFds.clear();                                       // riparte da vuoto
    pollfd listener;                                       // elemento per il socket di ascolto
    listener.fd = server_fd;                               // fd da controllare
    listener.events = POLLIN;                              // interessa: nuove connessioni
    listener.revents = 0;                                  // lo riempie poll()
    pollFds.push_back(listener);                           // sempre in posizione 0

    for (std::map<int, Client *>::iterator it = clientsByFD.begin(); it != clientsByFD.end(); ++it) // ogni client
    {
        pollfd pfd;                                        // elemento per questo client
        pfd.fd = it->first;                                // il suo fd
        pfd.events = 0;                                    // si parte senza eventi
        if (!it->second->shouldDelete())                   // client attivo?
            pfd.events |= POLLIN;                          // leggi i suoi comandi
        if (it->second->hasPendingOutput())                // ha messaggi in coda?
            pfd.events |= POLLOUT;                         // avvisami quando posso spedirli
        pfd.revents = 0;                                   // lo riempie poll()
        pollFds.push_back(pfd);                            // aggiunge all'array
    }
}

/* =====================================================================================
**  handlePollEvents()
**  Dopo poll(), il campo revents di ogni pollfd dice cosa è successo:
**   POLLIN   dati da leggere (per server_fd: connessione in arrivo; per un client che ha
**            chiuso: recv() restituirà 0)
**   POLLOUT  si può scrivere
**   POLLHUP  l'altro lato ha chiuso la connessione
**   POLLERR  errore sul socket          POLLNVAL  fd non valido
**  Ordine scelto: prima si legge (POLLIN), poi si scrive (POLLOUT), infine si gestisce
**  POLLHUP solo se non c'erano dati da leggere, così non si perde l'ultimo comando
**  (es. "QUIT" mandato appena prima di chiudere).
**  Durante il ciclo NESSUN client viene cancellato (solo segnato): il vettore pollFds e le
**  map restano validi. La pulizia avviene dopo, in cleanupClients().
** ===================================================================================== */
void Server::handlePollEvents()
{
    if (pollFds[0].revents & POLLIN)                       // connessione in arrivo sul socket di ascolto?
        acceptNewClient();                                 // accettala (il nuovo client entra in poll dal prossimo giro)

    for (size_t i = 1; i < pollFds.size(); ++i)            // i client partono dall'indice 1
    {
        short revents = pollFds[i].revents;                // eventi accaduti su questo fd
        if (revents == 0)                                  // niente di nuovo
            continue;                                      // passa al prossimo
        Client *client = getClientByFd(pollFds[i].fd);     // risale al Client dal fd
        if (!client)                                       // non dovrebbe succedere (difesa)
            continue;                                      // ignora
        if (revents & (POLLERR | POLLNVAL))                // errore sul socket
        {
            client->discardOutput();                       // inutile provare a spedire
            client->markForDeletion("Connection error");   // da rimuovere
            continue;                                      // passa al prossimo client
        }
        if (revents & POLLIN)                              // dati da leggere
            readFromClient(client);                        // legge ed esegue i comandi completi
        if ((revents & POLLOUT) && client->hasPendingOutput()) // si può scrivere e c'è qualcosa da spedire
            writeToClient(client);                         // spedisce la coda
        if ((revents & POLLHUP) && !(revents & POLLIN))    // chiuso dall'altro lato e niente da leggere
        {
            client->discardOutput();                       // non c'è più nessuno a cui spedire
            client->markForDeletion("Connection closed");  // da rimuovere
        }
    }
}

/* =====================================================================================
**  acceptNewClient()
**  Chiamata quando poll() segnala POLLIN su server_fd.
**   1. accept() crea un NUOVO socket dedicato a questo client e riempie addr con il suo IP;
**   2. il nuovo socket viene reso non bloccante (obbligatorio per il subject);
**   3. si crea il Client con new (il Server ne è il proprietario) e lo si registra in
**      clientsByFD. NON va in clientsByNick: non ha ancora un nick (lo inserirà updateNick).
**  Una sola accept() per POLLIN: poll() è "level-triggered", se ci sono altre connessioni
**  in coda segnalerà di nuovo POLLIN al prossimo giro.
** ===================================================================================== */
void Server::acceptNewClient()
{
    sockaddr_in addr;                                                  // qui accept() scrive l'indirizzo del client
    socklen_t len = sizeof(addr);                                      // dimensione della struttura (in/out)
    int fd = accept(server_fd, reinterpret_cast<sockaddr *>(&addr), &len); // accetta la connessione
    if (fd < 0)                                                        // nessuna connessione pronta o errore
        return;                                                        // riproverà poll()
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)                            // rende il socket non bloccante
    {
        close(fd);                                                     // impossibile: chiudi e rinuncia
        return;
    }
    Client *client = new Client(fd, inet_ntoa(addr.sin_addr));         // crea il client con il suo IP
    clientsByFD[fd] = client;                                          // il Server ora lo possiede
    std::cout << "[server] nuova connessione fd=" << fd
              << " da " << client->getHostname() << std::endl;         // log
}

/* =====================================================================================
**  readFromClient()
**  Chiamata quando poll() segnala POLLIN su un client. Una sola recv() (mai senza poll()):
**   - ritorna 0  -> il client ha chiuso la connessione (es. nc terminato);
**   - ritorna <0 -> errore; NON si controlla errno (vietato dal subject dopo recv):
**     dato che poll() ha appena detto "pronto", un errore qui è un errore vero;
**   - ritorna n>0 -> i byte vanno in fondo al buffer del client (std::string(buf, n):
**     nessun terminatore '\0' necessario, quindi nessun rischio di scrivere oltre l'array).
**  Poi si estraggono TUTTE le righe complete (extractLine) e per ciascuna:
**   riga vuota -> ignorata | troppo lunga -> errore 417 | altrimenti parse + dispatch.
**  Ci si ferma subito se il client viene segnato (es. QUIT o password errata): i comandi
**  successivi nello stesso pacchetto non devono essere eseguiti.
**  Infine, se nel buffer restano più di 510 byte senza '\n', il client sta mandando una
**  riga troppo lunga: discardLongLine() svuota il buffer (altrimenti crescerebbe senza
**  limite) e fa scartare anche il resto della riga quando arriverà; errore 417 una volta.
** ===================================================================================== */
void Server::readFromClient(Client *client)
{
    char buf[512];                                                     // buffer temporaneo di lettura
    ssize_t n = recv(client->getFD(), buf, sizeof(buf), 0);            // legge al massimo 512 byte
    if (n <= 0)                                                        // chiusura o errore
    {
        client->discardOutput();                                       // il client non c'è più
        client->markForDeletion(n == 0 ? "Connection closed" : "Read error"); // da rimuovere
        return;                                                        // nient'altro da fare
    }
    client->appendToBuffer(std::string(buf, static_cast<size_t>(n)));  // accoda esattamente n byte

    std::string line;                                                  // riga estratta
    while (!client->shouldDelete() && client->extractLine(line))       // finché ci sono righe complete
    {
        if (line.empty())                                              // riga vuota ("\r\n" da solo)
            continue;                                                  // ignorala
        std::cout << "[fd " << client->getFD() << "] >> " << line << std::endl; // log del comando ricevuto
        if (line.size() > MAX_LINE_LENGTH)                             // oltre il limite IRC
        {
            client->sendError("417", ":Input line was too long");      // ERR_INPUTTOOLONG
            continue;                                                  // passa alla riga successiva
        }
        IrcMessage msg = IrcParser::parse(line);                       // divide in prefix/command/params/trailing
        if (!msg.command.empty())                                      // c'è un comando?
            dispatchCommand(msg, client);                              // eseguilo
    }
    if (client->getBuffer().size() > MAX_LINE_LENGTH)                  // riga incompleta troppo lunga
        if (client->discardLongLine())                                 // scarta (true solo la prima volta)
            client->sendError("417", ":Input line was too long");      // avvisa il client una volta
}

/* =====================================================================================
**  writeToClient()
**  Chiamata quando poll() segnala POLLOUT e il client ha messaggi in coda.
**  flushOutput() fa una sola send(); se fallisce la connessione è persa e il client va
**  rimosso. Se send() ha spedito solo una parte, il resto rimane in coda: buildPollFds()
**  chiederà di nuovo POLLOUT al prossimo giro.
** ===================================================================================== */
void Server::writeToClient(Client *client)
{
    if (!client->flushOutput())                    // send() fallita?
    {
        client->discardOutput();                   // inutile riprovare
        client->markForDeletion("Write error");    // da rimuovere
    }
}

/* =====================================================================================
**  dispatchCommand()
**  Decide QUALE handler eseguire per un messaggio già analizzato dal parser:
**   1. cerca il comando nella tabella; se non c'è:
**        client registrato     -> 421 ERR_UNKNOWNCOMMAND
**        client non registrato -> 451 ERR_NOTREGISTERED
**   2. prima della registrazione sono permessi SOLO PASS, NICK, USER, CAP, PING, PONG,
**      QUIT. Tutto il resto -> 451. Così gli handler di JOIN, PRIVMSG, ... possono dare
**      per scontato che il client sia registrato;
**   3. chiama l'handler tramite il puntatore a metodo: (this->*handler)(msg, client).
** ===================================================================================== */
void Server::dispatchCommand(const IrcMessage &msg, Client *client)
{
    std::map<std::string, CommandHandler>::iterator it = commands.find(msg.command); // cerca il comando
    if (it == commands.end())                                          // comando sconosciuto
    {
        if (client->isRegistered())                                    // utente registrato
            client->sendError("421", msg.command + " :Unknown command"); // ERR_UNKNOWNCOMMAND
        else                                                           // non ancora registrato
            client->sendError("451", ":You have not registered");      // ERR_NOTREGISTERED
        return;                                                        // fine
    }
    bool allowedBeforeRegistration = msg.command == "PASS" || msg.command == "NICK"
        || msg.command == "USER" || msg.command == "CAP" || msg.command == "PING"
        || msg.command == "PONG" || msg.command == "QUIT";             // comandi della fase di login
    if (!client->isRegistered() && !allowedBeforeRegistration)         // comando "da registrati"
    {
        client->sendError("451", ":You have not registered");          // ERR_NOTREGISTERED
        return;                                                        // non eseguirlo
    }
    CommandHandler handler = it->second;                               // puntatore al metodo
    (this->*handler)(msg, client);                                     // chiama l'handler su questo Server
}

/* =====================================================================================
**  cleanupClients()
**  A fine iterazione rimuove i client segnati con markForDeletion(), in due passate:
**   1. raccoglie in un vettore i client da rimuovere (non si può fare erase su una map
**      mentre la si scorre: l'iteratore diventerebbe non valido);
**   2. chiama removeClient() su ciascuno.
**  Un client segnato che ha ancora messaggi in coda (es. "ERROR :Closing Link" dopo una
**  password sbagliata) NON viene rimosso subito: aspetta il POLLOUT che li spedisce.
**  Se nel frattempo la connessione cade, POLLHUP/POLLERR svuotano la coda e al giro dopo
**  viene rimosso.
** ===================================================================================== */
void Server::cleanupClients()
{
    std::vector<Client *> toRemove;                                    // client da eliminare
    for (std::map<int, Client *>::iterator it = clientsByFD.begin(); it != clientsByFD.end(); ++it) // ogni client
        if (it->second->shouldDelete() && !it->second->hasPendingOutput()) // segnato e coda vuota
            toRemove.push_back(it->second);                            // eliminalo dopo
    for (size_t i = 0; i < toRemove.size(); ++i)                       // ora che non scorriamo più la map
        removeClient(toRemove[i]);                                     // rimuovi davvero
}

/* =====================================================================================
**  removeClient()
**  Elimina definitivamente un client. L'ORDINE è importante:
**   1. se era registrato, avvisa con ":nick!user@host QUIT :<motivo>" tutti quelli che
**      condividono almeno un canale con lui. Si usa un std::set così chi condivide più
**      canali riceve il QUIT una volta sola. Va fatto PRIMA di toglierlo dai canali,
**      altrimenti non sapremmo più chi avvisare;
**   2. lo toglie da ogni canale (su una COPIA della lista, perché removeClient del canale
**      modifica la lista originale) e cancella i canali rimasti vuoti;
**   3. toglie il nick da clientsByNick (solo se la voce punta proprio a lui);
**   4. toglie il fd da clientsByFD;
**   5. delete: il distruttore del Client chiude il fd. Nessun close() qui (no double close).
** ===================================================================================== */
void Server::removeClient(Client *client)
{
    std::vector<Channel *> chans = client->getChannels();              // copia dei suoi canali
    if (client->isRegistered())                                        // solo chi era registrato è "visibile"
    {
        std::set<Client *> recipients;                                 // destinatari del QUIT, senza duplicati
        for (size_t i = 0; i < chans.size(); ++i)                      // ogni canale del client
        {
            std::vector<Client *> members = chans[i]->getClients();    // membri del canale
            for (size_t j = 0; j < members.size(); ++j)                // ogni membro
                if (members[j] != client)                              // tranne lui stesso
                    recipients.insert(members[j]);                     // da avvisare
        }
        std::string quitMsg = ":" + client->getPrefix() + " QUIT :" + client->getQuitReason(); // messaggio IRC
        for (std::set<Client *>::iterator it = recipients.begin(); it != recipients.end(); ++it) // ogni destinatario
            (*it)->send(quitMsg);                                      // accoda il QUIT
    }
    for (size_t i = 0; i < chans.size(); ++i)                          // ogni canale del client
    {
        chans[i]->removeClient(*client);                               // esce dal canale
        removeChannelIfEmpty(chans[i]);                                // se vuoto, il canale sparisce
    }
    if (!client->getNickname().empty())                                // aveva un nick?
    {
        std::map<std::string, Client *>::iterator it = clientsByNick.find(ircLower(client->getNickname())); // cerca la voce
        if (it != clientsByNick.end() && it->second == client)         // esiste ed è proprio lui
            clientsByNick.erase(it);                                   // libera il nick
    }
    clientsByFD.erase(client->getFD());                                // toglie dall'indice principale
    std::cout << "[server] disconnesso fd=" << client->getFD()
              << " (" << client->getQuitReason() << ")" << std::endl;  // log
    delete client;                                                     // libera la memoria e chiude il fd
}

// ------------------------------------------------------------------------------ RICERCHE CLIENT

Client *Server::getClientByFd(int fd)
{
    std::map<int, Client *>::iterator it = clientsByFD.find(fd);       // ricerca O(log n)
    return it == clientsByFD.end() ? NULL : it->second;                // NULL se non esiste
}

/* =====================================================================================
**  getClientByNick()
**  Cerca un client per nick (usata da PRIVMSG, KICK, INVITE, MODE +o, WHO).
**  In IRC i nick non distinguono maiuscole e minuscole ("Alice" == "alice"): per questo
**  la chiave della map è sempre il nick in minuscolo (ircLower).
** ===================================================================================== */
Client *Server::getClientByNick(const std::string &nick)
{
    std::map<std::string, Client *>::iterator it = clientsByNick.find(ircLower(nick)); // ricerca
    return it == clientsByNick.end() ? NULL : it->second;              // NULL se non esiste
}

/* =====================================================================================
**  isNickTaken()
**  true se il nick è già usato da un client DIVERSO da `exclude`.
**  `exclude` serve per permettere a un utente di cambiare solo le maiuscole del proprio
**  nick (es. "alice" -> "Alice"): la voce trovata è lui stesso, quindi non è "occupato".
** ===================================================================================== */
bool Server::isNickTaken(const std::string &nick, Client *exclude) const
{
    std::map<std::string, Client *>::const_iterator it = clientsByNick.find(ircLower(nick)); // ricerca
    return it != clientsByNick.end() && it->second != exclude;         // trovato e non è lui
}

/* =====================================================================================
**  updateNick()
**  UNICO modo corretto di cambiare il nick di un client, perché tiene allineati Client e
**  indice clientsByNick:
**   1. rimuove la vecchia chiave (se c'era e puntava a lui);
**   2. imposta il nuovo nick nel Client;
**   3. inserisce la nuova chiave.
**  Se gli handler chiamassero direttamente client->setNickname(), la map conterrebbe
**  ancora il vecchio nick: PRIVMSG al nuovo nick fallirebbe e il vecchio resterebbe "occupato".
** ===================================================================================== */
void Server::updateNick(Client *client, const std::string &newNick)
{
    if (!client->getNickname().empty())                                // aveva già un nick?
    {
        std::map<std::string, Client *>::iterator it = clientsByNick.find(ircLower(client->getNickname())); // vecchia voce
        if (it != clientsByNick.end() && it->second == client)         // è proprio lui
            clientsByNick.erase(it);                                   // rimuovila
    }
    client->setNickname(newNick);                                      // aggiorna il Client
    clientsByNick[ircLower(newNick)] = client;                         // nuova voce dell'indice
}

// ------------------------------------------------------------------------------ CANALI

Channel *Server::getChannel(const std::string &name)
{
    std::map<std::string, Channel *>::iterator it = channels.find(ircLower(name)); // "#Chan" == "#chan"
    return it == channels.end() ? NULL : it->second;                   // NULL se non esiste
}

/* =====================================================================================
**  createChannel()
**  Restituisce il canale con quel nome, creandolo se non esiste ancora. La validità del
**  nome (deve iniziare con # o &) la controlla l'handler di JOIN prima di chiamarla.
** ===================================================================================== */
Channel *Server::createChannel(const std::string &name)
{
    Channel *channel = getChannel(name);           // esiste già?
    if (channel)                                   // sì
        return channel;                            // restituiscilo
    channel = new Channel(name);                   // no: crealo (il Server ne è il proprietario)
    channels[ircLower(name)] = channel;            // registralo nell'indice
    return channel;                                // restituisci il nuovo canale
}

/* =====================================================================================
**  removeChannelIfEmpty()
**  Da chiamare ogni volta che qualcuno esce da un canale (PART, KICK, QUIT). Se il canale
**  non ha più membri viene tolto dall'indice e distrutto: un canale vuoto non esiste più
**  in IRC (il prossimo JOIN lo ricreerà da zero, senza topic né modi).
** ===================================================================================== */
void Server::removeChannelIfEmpty(Channel *channel)
{
    if (!channel || !channel->isEmpty())           // puntatore nullo o canale con membri
        return;                                    // niente da fare
    channels.erase(ircLower(channel->getName()));  // togli dall'indice
    delete channel;                                // libera la memoria
}

// ------------------------------------------------------------------------------ UTILITÀ

/* =====================================================================================
**  ircLower()  [static]
**  Restituisce una copia della stringa in minuscolo. Serve per le chiavi delle map
**  (nick e canali), così "Alice", "ALICE" e "alice" sono considerati lo stesso nick.
**  Il cast a unsigned char evita comportamento indefinito con caratteri non ASCII.
** ===================================================================================== */
std::string Server::ircLower(const std::string &str)
{
    std::string result = str;                                          // copia modificabile
    for (std::string::size_type i = 0; i < result.size(); ++i)         // ogni carattere
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i]))); // in minuscolo
    return result;                                                     // la copia convertita
}
