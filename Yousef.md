First week

1. Client Class (priority)
    - Constructor with fd
    - Getters/Setters for nickname, user, password
    - isAuth() → return if it has PASS
    - isRegistered() → returns if it has PASS + NICK + USER
    - tryRegister() → if it has PASS + NICK + USER, sets registered = true and send a welcome message.
    - send() → send a message to the client with ::send(fd, ...)
    - sendError() → sends IRC Errors (ex: "461 * PASS:Not enough parameters...")
    - sendReply() → send reply IRC (ex: "001 Nick:Welcome...")
    - Buffer handling: appendToBuffer(), clearBuffer().
    - markForDeletion → sets a flag for a user that needs to be deleted.

2. Channel Class (priority)
    - Constructor with channel name
    - Getters/setters for topic, key, limit
    - Mode flags: inviteOnly, topicRestricted
    - addClient() / removeClient() → member management
    - addOperator() / removeOperator() → first client is becoming automaticly Operator(admin)
    - addInvite() / isInvited() → for invite-only channels
    - addBan() / isBanned() → ban list
    - broadcast() → sends a message to all members in that channel
    - sendTopic() → sends RPL_TOPIC (332) or RPL_NOTOPIC (331)
    - sendNames() → sends RPL_NAMREPLY (353) with the user list

3. Server Class (priotity)
    - Socket + Poll(ask Vlad for the example)
    - Socket creation, bind, listen
    - Poll loop with poll()
    - New connection → Client *(new connections are creating new objects of type Client *)
    - Non-blocking I/O with fcntl() + O_NONBLOCK
    - Management POLLHUP, POLLERR, EAGAIN


TIPS & TRICKS
- Some references to make it easy!

1. Data structures:
    -std::map<int, Client*> clientsByFD → for Client declared in Server(private)
    -std::map<std::string, Client*> clientsByNick → for Client declared in Server(private)
    -std::map<std::string, Channel*> channels → for Channel declared in Server(private)

2. Client management:
    -addClient() → adds a client in maps
    -removeClient() → removes from maps, close fd, delete
    -getClientByFd() → finds a client after fd
    -getClientByNick() → finds a client after nick
    -isNickTaken() → checks if the nickname is used

3. Channel management:
    -getChannel() → finds a channel after the name
    -createChannel() → creates a channel if it does not exists
    -removeChannel() → deletes the channel



=====================================================================
SERVER.HPP — GUIDA PASSO PASSO (cosa, come, perché)
=====================================================================

0. PREMESSA: DA DOVE PARTI
--------------------------
Nel repo ci sono già due situazioni da conoscere prima di scrivere una riga:

a) irc_youssef/Server/Server.hpp (il tuo file in lavorazione) contiene per
   errore una classe chiamata `Client` (con _serverFd, _pollFds, _clients...).
   È un mix tra Client e Server: la classe deve chiamarsi `Server`, e NON deve
   ridefinire `Client`, perché Client esiste già in Client/Client.hpp.
   Se due file dichiarano `class Client` diversa → errore di redefinition.

b) Server/Server.hpp (versione "principale" del repo) usa già
   std::vector<Client*> clients e std::vector<Channel*> channels. Funziona, ma
   è diverso da quanto pianificato qui sopra (TIPS & TRICKS: le map). Le map
   danno lookup O(log n) per fd/nick/canale invece di cicli lineari, e
   soprattutto rendono impossibile avere due client con lo stesso nick o due
   canali con lo stesso nome (la chiave è unica per costruzione).

Le API che Client e Channel ESPONGONO già (le userai dal Server):
  Client : getFD(), getNickname(), getPrefix(), isAuth(), isRegistered(),
           appendToBuffer(), getBuffer(), clearBuffer(), send(), sendError(),
           sendReply(), markForDeletion(), shouldDelete(), getChannels() [copia!]
  Channel: getName(), addClient(), removeClient(), getClientCount(),
           broadcast(), isOperator(), ...
  IrcParser::parse(line) → IrcMessage {prefix, command, params, trailing}


1. RUOLO DEL SERVER (perché esiste)
-----------------------------------
Il Server è l'ORCHESTRATORE: possiede tutte le risorse (socket di ascolto,
Client, Channel) e le fa vivere dentro UN SOLO ciclo poll(). Il subject lo
impone: un solo poll(), niente fork, niente thread, I/O non bloccante.
- Client  = stato di UNA connessione (nick, buffer, flag registrazione).
- Channel = stato di UN canale (membri, operatori, modi).
- Server  = colui che sa "chi esiste", riceve i byte, li passa al parser e
            smista i comandi. Se un oggetto deve "trovare" un altro oggetto
            (nick → Client, nome → Channel) ci vuole il Server.
Regola d'oro: il Server è l'UNICO owner (chi fa new/delete) di Client e
Channel. Gli altri si scambiano solo puntatori NON posseduti.


2. STRUTTURA DEL FILE E INCLUDE
-------------------------------
Passo 2.1 — include guard: `#ifndef SERVER_HPP / #define SERVER_HPP / #endif`
            (oppure #pragma once come in Client.hpp). Perché: Server.hpp è
            incluso da main.cpp e Server.cpp; senza guard → redefinition.

Passo 2.2 — include SOLO quello che serve nell'header:
    #include <string>
    #include <vector>
    #include <map>
    #include <poll.h>            // per struct pollfd (è un membro)
    #include "../IrcParser/IrcParser.hpp"   // IrcMessage è passato nei handle*()
  e forward declaration:
    class Client;
    class Channel;
  Perché: nell'header usi solo PUNTATORI a Client/Channel, quindi basta dire
  al compilatore "esistono". Gli #include completi ("Client.hpp",
  "Channel.hpp") vanno nel Server.cpp. Vantaggi: compilazione più veloce,
  niente dipendenze circolari (Client ↔ Channel ↔ Server).
  (Nota: la versione attuale include <cmath> — non serve, toglilo.)

Passo 2.3 — C++98: il Makefile usa -std=c++98 -Wall -Wextra -Werror.
  Quindi: niente `nullptr` (usa NULL), niente `auto`, niente range-for, niente
  `std::to_string`, niente `override`. `pollfd pfd = {};` funziona ma
  preferisci memset o un piccolo costruttore/helper.


3. MEMBRI PRIVATI (i dati)
--------------------------
    int                              port;
    std::string                      password;
    int                              server_fd;      // socket d'ascolto, -1 se non aperto
    bool                             running;
    std::vector<pollfd>              pollFds;        // pollFds[0] = server_fd, il resto i client
    std::map<int, Client*>           clientsByFD;
    std::map<std::string, Client*>   clientsByNick;
    std::map<std::string, Channel*>  channels;

Perché ciascuno:
- port/password: parametri da av[1]/av[2]; la password serve in handlePass().
- server_fd = -1 all'inizio: così il distruttore sa se c'è qualcosa da chiudere.
- running: permette a stop() e al segnale di far uscire il loop.
- pollFds: poll() vuole un ARRAY contiguo di struct pollfd; il vector<pollfd>
  garantisce contiguità (&pollFds[0]). Tieni sempre l'indice 0 per il listener.
- clientsByFD: è l'owner principale. Chiave = fd, perché poll() ti dice quale
  fd è pronto e tu devi risalire al Client in O(log n).
- clientsByNick: indice SECONDARIO (non possiede nulla; stessi puntatori di
  clientsByFD). Serve per PRIVMSG/INVITE/KICK/WHO su un nick e per isNickTaken.
  Attenzione: un client senza NICK non ha ancora una entry qui (nick vuoto →
  NON inserirlo, altrimenti ne avresti una sola per tutti i "senza nick").
- channels: chiave = nome canale (es. "#general"); owner dei Channel.

Perché puntatori (Client*) e non oggetti nella map:
  Client ha un destructor che fa close(fd). Se lo copi (e std::map copia
  quando inserisci/riallochi) hai DUE oggetti che chiudono lo stesso fd →
  double close. In C++98 non c'è move → puntatori allocati con new.
  (È anche il motivo per cui `std::map<int, Client>` nel tuo file WIP è un
  errore.)

Perché nomi senza underscore: il resto del progetto (Server.cpp attuale,
Client.hpp) usa `port`, `pass`, `server_fd`... Mantieni lo stile coerente.


4. FORMA CANONICA (costruttore, copia, distruttore)
---------------------------------------------------
    Server(int port, const std::string &password);
    ~Server();
  private:
    Server(const Server &);              // vietata la copia
    Server &operator=(const Server &);   // vietata l'assegnazione

Perché: Server possiede risorse (fd, memoria). Una copia superficiale
duplicherebbe i puntatori → double delete. Dichiarare copia/assegnazione
privati e NON implementarli (C++98) rende impossibile copiarlo: il compilatore
ti ferma. `const std::string &` evita una copia inutile della password.

Il distruttore (in .cpp) deve:
  1. delete di ogni Client in clientsByFD (il ~Client fa già close(fd)!).
  2. delete di ogni Channel in channels.
  3. close(server_fd) se != -1.
  4. svuotare le map. → valgrind pulito, zero fd aperti alla chiusura.
ATTENZIONE double close: il tuo Server.cpp attuale fa close(fds[i].fd) E poi
removeClient() → delete client → ~Client() → close(fd) di nuovo. Decidi UN
solo posto che chiude: consiglio quello del ~Client (RAII), quindi il Server
NON chiama close() sui fd dei client, fa solo delete.


5. INTERFACCIA PUBBLICA
-----------------------
    void start();     // init socket + loop principale (bloccante fino a stop/SIGINT)
    void stop();      // running = false

  Handler dei comandi (già presenti, restano public o private, a te la scelta;
  meglio PRIVATE perché sono chiamati solo dal dispatch interno):
    void handlePass / handleNick / handleUser / handleJoin / handleWho /
         handlePrivmsg / handleKick / handleInvite / handleTopic / handleMode /
         handleChannelMode / handleQuit (const IrcMessage &msg, Client *client)
  Perché `const IrcMessage &`: il messaggio non va modificato né copiato.
  Perché `Client *`: chi ha inviato il comando (può essere segnato da cancellare).
  Consiglio: aggiungi handlePing (PING → PONG) e handlePart (PART) — sono
  richiesti nella pratica da HexChat/irssi (senza PONG vieni disconnesso
  dal client, senza PART non si può uscire da un canale).


6. METODI PRIVATI DI SUPPORTO (spezzare start() è l'obiettivo)
--------------------------------------------------------------
Il tuo start() attuale è ~200 righe: socket, poll, accept, recv, parsing e
dispatch tutti insieme. Divide et impera — dichiara in Server.hpp:

  // --- setup ---
  void setupSocket();                  // socket → setsockopt(SO_REUSEADDR) → fcntl(O_NONBLOCK)
                                       //   → bind → listen. Lancia eccezione o ritorna false.
  // --- loop ---
  void run();                          // il while(running) con poll()
  void buildPollFds();                 // (ri)costruisce pollFds da server_fd + clientsByFD
  void acceptNewClient();              // accept() in loop finché EAGAIN, crea Client
  void readFromClient(Client *c);      // recv() → appendToBuffer → processBuffer
  void writeToClient(Client *c);       // solo se implementi la coda di output (vedi §9)
  void processBuffer(Client *c);       // estrae le righe complete, parse, dispatch
  void dispatchCommand(const IrcMessage &msg, Client *c);   // if/else o map<string, fn-ptr>
  void cleanupClients();               // rimuove i client con shouldDelete() a fine iterazione

  // --- gestione client (da Yousef.md) ---
  void   addClient(int fd);
  void   removeClient(Client *client);
  Client *getClientByFd(int fd);
  Client *getClientByNick(const std::string &nick);
  bool   isNickTaken(const std::string &nick, Client *exclude = NULL) const;
  void   updateNick(Client *c, const std::string &oldNick, const std::string &newNick);

  // --- gestione canali (da Yousef.md) ---
  Channel *getChannel(const std::string &name);
  Channel *createChannel(const std::string &name);
  void     removeChannel(const std::string &name);


7. IMPLEMENTAZIONE — ORDINE CONSIGLIATO (uno step alla volta, compila ogni volta)
-------------------------------------------------------------------------------
STEP 1 — Scheletro compilabile
  Scrivi Server.hpp con solo membri + ctor + dtor + start/stop. Nel .cpp lascia
  i corpi vuoti. Verifica: `make` compila senza warning.
  Perché: ogni errore di include/forward declaration lo scopri subito, non
  dopo 500 righe.

STEP 2 — setupSocket()
  a. server_fd = socket(AF_INET, SOCK_STREAM, 0);          // TCP IPv4
  b. setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &1, sizeof(int));
        → riavvii il server subito senza "Address already in use" (TIME_WAIT).
  c. fcntl(server_fd, F_SETFL, O_NONBLOCK);
        → OBBLIGATORIO anche sul listener: senza, accept() potrebbe bloccare
          se il client chiude tra il poll e l'accept.
  d. sockaddr_in addr: memset a 0; sin_family = AF_INET;
        sin_addr.s_addr = htonl(INADDR_ANY); sin_port = htons(port);
        → htons/htonl: converte in network byte order (big endian).
  e. bind(...)   → se fallisce (porta occupata/privilegiata) stampa strerror e esci.
  f. listen(server_fd, SOMAXCONN);   // backlog: coda di connessioni non ancora accettate
  g. pollFds.push_back(listener con events = POLLIN).
  Perché ogni passo può fallire: controlla SEMPRE il ritorno (< 0), chiudi
  server_fd e segnala l'errore. Nel main valida anche la porta (1024–65535)
  e la password non vuota: std::atoi("abc") dà 0 senza avvisarti.

STEP 3 — Segnali
  static volatile sig_atomic_t g_stop = 0;  (già presente come stopRequested)
  static void onSignal(int) { g_stop = 1; }
  signal(SIGINT, onSignal); signal(SIGQUIT, onSignal);  (Ctrl+C / Ctrl+\)
  signal(SIGPIPE, SIG_IGN);
  Perché SIGPIPE: se scrivi su un socket già chiuso dal peer, di default il
  kernel UCCIDE il processo. Un client che fa Ctrl+C mentre gli mandi un
  broadcast farebbe crashare TUTTO il server. Con SIG_IGN send() ritorna -1
  (EPIPE) e gestisci l'errore. (In alternativa flag MSG_NOSIGNAL in send()).
  Perché volatile sig_atomic_t: l'handler gira in modo asincrono; è l'unico
  tipo che lo standard garantisce sicuro da scrivere/leggere da un handler.
  Nell'handler non fare cout/free/delete: solo settare il flag.

STEP 4 — Loop poll() (run())
      while (running && !g_stop) {
          buildPollFds();
          int ret = poll(&pollFds[0], pollFds.size(), -1);
          if (ret < 0) { if (errno == EINTR) continue; break; }
          ... gestisci eventi ...
          cleanupClients();
      }
  Perché -1 (timeout infinito): il processo DORME finché non succede qualcosa
  → 0% CPU. Se un giorno vuoi timeout/ping periodici metti 1000 ms.
  Perché EINTR: SIGINT interrompe poll() con errno=EINTR; non è un errore vero.

  Come gestire gli eventi (revents) dopo poll():
    - pollFds[0].revents & POLLIN   → acceptNewClient()
    - per ogni client fd:
        revents & (POLLERR|POLLHUP|POLLNVAL) → client->markForDeletion()
        revents & POLLIN                     → readFromClient()
        revents & POLLOUT                    → writeToClient()   (se usato)
  Significato: POLLIN = dati da leggere (o peer chiuso: recv → 0);
               POLLHUP = peer ha chiuso; POLLERR = errore sul socket;
               POLLNVAL = fd non valido (bug tuo: fd già chiuso ma ancora in lista).
  Ordine: controlla prima POLLIN e poi HUP: un client può mandare "QUIT"
  e chiudere subito; se elimini per HUP prima di leggere perdi l'ultimo dato.

  !!! BUG da evitare (presente nel Server.cpp attuale):
  Modificare pollFds (erase) mentre ci iteri sopra invalida indici/iteratori
  e crea disallineamento tra pollFds e clientsByFD (es. il ramo
  "bytes <= 0" fa erase dal vector ma NON rimuove il Client → memory leak
  + client "fantasma" in lista).
  SOLUZIONE consigliata: ricostruisci pollFds a inizio di ogni iterazione
  (buildPollFds) partendo da server_fd + clientsByFD. Durante l'iterazione
  NON cancellare nulla: chiama solo client->markForDeletion(); a FINE giro
  cleanupClients() rimuove tutto in un colpo. Un'unica fonte di verità
  (clientsByFD), niente indici che si sfalsano.

STEP 5 — acceptNewClient()
      while (true) {
          int fd = accept(server_fd, NULL, NULL);
          if (fd < 0) break;                // niente altro da accettare (EAGAIN) o errore
          fcntl(fd, F_SETFL, O_NONBLOCK);   // OBBLIGATORIO: anche sui client
          addClient(fd);
      }
  Perché il while: con poll level-triggered basterebbe un accept per giro,
  ma svuotare la coda evita latenza se arrivano più connessioni insieme.
  Perché non guardare errno dopo accept: vedi "REGOLA errno" più sotto; se
  ritorna <0 esci dal ciclo e poll ti richiamerà.
  addClient(fd): `Client *c = new Client(fd); clientsByFD[fd] = c;`
  (il nick entra in clientsByNick solo quando arriva un NICK valido).
  Opzionale: limite massimo di client (es. 1000) e chiudi il fd in eccesso.

STEP 6 — readFromClient()
      char buf[512];                                   // o 1024, ma...
      ssize_t n = recv(fd, buf, sizeof(buf), 0);       // ...NON scrivere buf[n] = 0 se n == sizeof(buf)!
      if (n <= 0) { c->markForDeletion(); return; }    // 0 = chiusura ordinata, <0 = errore/EAGAIN
      c->appendToBuffer(std::string(buf, n));
      processBuffer(c);
  Perché appendToBuffer e non processare subito: TCP è uno STREAM, non
  messaggi. Un comando può arrivare spezzato ("JO" + "IN #a\r\n") o due
  comandi insieme ("NICK a\r\nUSER ...\r\n"). Il subject lo testa
  esplicitamente con nc + Ctrl+D (comando spedito in 3 pezzi).
  BUG attuale: `char buffer[1024]; buffer[bytes] = 0;` con bytes == 1024
  scrive fuori dall'array (buffer overflow). Costruisci direttamente la
  std::string(buf, n): non serve il terminatore.

STEP 7 — processBuffer(Client *c)
      std::string &buf = <buffer del client>;
      size_t pos;
      while ((pos = buf.find('\n')) != std::string::npos) {
          std::string line = buf.substr(0, pos);
          buf.erase(0, pos + 1);
          if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
          if (line.empty()) continue;
          IrcMessage msg = IrcParser::parse(line);
          dispatchCommand(msg, c);
          if (c->shouldDelete()) break;      // dopo QUIT/errore non processare altro
      }
  Perché cercare solo '\n': accetta sia "\r\n" (IRC standard/HexChat) sia
  "\n" (nc). Poi togli l'eventuale '\r'. È molto più semplice e corretto del
  doppio find attuale: la riga `buf.erase(0, pos + (buf[pos+1] == '\n' ? 2 : 1))`
  è sbagliata (guarda il carattere DOPO e può leggere fuori stringa;
  con "\r\n" pos punta a '\r', quindi funziona per caso, con "\n" no).
  Difesa: se buf supera 512 byte senza '\n' → client malevolo/rotto:
  markForDeletion() (evita memoria illimitata).
  Nota: getBuffer() ritorna const&, quindi oggi il Server usa un const_cast
  (brutto). Meglio aggiungere in Client un metodo `bool extractLine(std::string &line)`
  che fa tutto ciò sopra, oppure `std::string &getBufferRef()`.

STEP 8 — dispatchCommand()
  Confronta msg.command (già in MAIUSCOLO? Verifica: se IrcParser non fa
  toupper, i comandi "nick"/"join" minuscoli non verrebbero riconosciuti;
  fallo nel parser).
  Regole di registrazione (perché: RFC 2812):
    - Prima della registrazione sono ammessi SOLO: PASS, NICK, USER, CAP, QUIT (e PING).
    - Ogni altro comando da un client non registrato → 451 ":You have not registered".
    - PASS deve arrivare PRIMA di NICK/USER.
  Comando sconosciuto → 421 "<cmd> :Unknown command".
  Con C++98 le opzioni sono: catena if/else (semplice, va bene) oppure
  std::map<std::string, void (Server::*)(const IrcMessage&, Client*)> (più
  pulito, sintassi puntatori a metodo membro più ostica).

STEP 9 — Uscita client: removeClient() / cleanupClients()
  Ordine ESATTO (tutto sbagliato se invertito):
    1. Se il client aveva un nick: erase da clientsByNick (solo se la entry
       punta proprio a questo client).
    2. Copia `std::vector<Channel*> chs = client->getChannels();` — COPIA,
       perché Channel::removeClient chiama client.leaveChannel(this), che
       modifica il vettore originale mentre lo scorri.
    3. Per ogni canale: (a) broadcast del QUIT ai membri PRIMA di rimuoverlo
       (altrimenti nessuno lo riceve), (b) channel->removeClient(*client),
       (c) se getClientCount() == 0 → removeChannel(nome) (delete + erase).
       Il passaggio di operatore al primo membro è già fatto in
       Channel::removeClient.
    4. clientsByFD.erase(fd);
    5. delete client;   // ~Client fa close(fd). NON fare close() prima.
  cleanupClients(): raccogli prima in un vector<Client*> tutti quelli con
  shouldDelete() e SOLO dopo chiama removeClient su ciascuno. Motivo: non
  puoi fare erase su una map mentre ci iteri (in C++98 erase(it) invalida it).

STEP 10 — Nick: isNickTaken() e updateNick()
  isNickTaken(nick, exclude): `it = clientsByNick.find(nick); return it != end && it->second != exclude;`
  (exclude serve quando un utente rifà NICK con il nome che già ha).
  updateNick(c, old, new): erase(old) se non vuoto, poi clientsByNick[new] = c.
  Sempre PRIMA di client->setNickname(new) o subito dopo, ma coerentemente.
  Extra: i nick IRC sono case-insensitive ("Mario" == "mario"). Se vuoi
  farlo bene, usa come chiave la versione lowercase (scandinavian chars
  {}|^ esclusi per semplicità) sia in insert che in find.

STEP 11 — Channel management
  getChannel(name): find nella map → puntatore o NULL.
  createChannel(name): se esiste ritorna quello, altrimenti `new Channel(name)`
  e channels[name] = ch. (Il nome deve iniziare con # o & — già gestito nel
  commit "Channels can start only with # or &"; controlla nel handleJoin.)
  removeChannel(name): find → delete → erase. Dopo la delete NESSUN altro
  deve avere il puntatore (per questo si copia getChannels() e si fa
  leaveChannel prima).

STEP 12 — Test di ciascuno step (senza aspettare gli handler)
  make && ./ircserv 6667 pass
  nc -C localhost 6667            # -C invia \r\n
     PASS pass / NICK a / USER a 0 * :A   → deve arrivare 001
  Frammentazione (test del subject): `nc -C localhost 6667`, scrivi "NI",
  Ctrl+D, "CK x", Ctrl+D, "\n"→ deve funzionare come "NICK x".
  Due client insieme: JOIN #a da entrambi, PRIVMSG #a :ciao.
  Chiusura brutale: Ctrl+C sul nc mentre l'altro riceve → il server NON deve
  crashare (SIGPIPE) né lasciare fd (`ls /proc/$(pidof ircserv)/fd | wc -l`).
  Valgrind: `valgrind --leak-check=full --track-fds=yes ./ircserv 6667 pass`,
  poi Ctrl+C → 0 leak, solo 0/1/2 fd aperti.
  Sovraccarico: `for i in $(seq 100); do nc localhost 6667 & done`.
  HexChat: connetti, verifica CAP / PING / WHO / MODE dopo il JOIN.


8. LA REGOLA errno (attenzione, il subject è severo)
---------------------------------------------------
Il subject vieta di controllare errno DOPO read/recv/write/send per decidere
cosa fare (per capire se era EAGAIN). In pratica:
  - recv() ritorna 0  → il peer ha chiuso   → markForDeletion.
  - recv() ritorna <0 → trattalo come "niente da leggere ora / errore": con
    poll() che ti ha appena detto POLLIN, un <0 vuol dire errore → elimina
    il client (o semplicemente ignora e aspetta il prossimo poll).
  - NON scrivere `if (errno == EAGAIN)` dopo recv/send.
  errno dopo poll() (EINTR) è invece permesso: non è una read/send.
  Perciò nel piano "Management POLLHUP, POLLERR, EAGAIN" la gestione di
  EAGAIN si ottiene implicitamente: non chiami mai recv/send senza poll che
  ti dica che l'fd è pronto.


9. INVIO DATI: send() SOLO DOPO poll (POLLOUT) — PARTE AVANZATA MA IMPORTANTE
----------------------------------------------------------------------------
Oggi Client::send() chiama ::send() subito, quando lo decide un handler.
Problemi:
  1. Il subject dice che read/recv/write/send su un fd DEVE passare da poll()
     (violazione = voto 0, v. SPIEGAZIONE_SUBJECT.md §5.3).
  2. Con socket non bloccante ::send() può inviare SOLO una parte (o niente,
     buffer pieno): i byte rimanenti oggi vanno persi → messaggi troncati.
Soluzione standard (poi la fai quando il resto funziona):
  - Client: aggiungi std::string outBuffer; send(msg) fa solo
    `outBuffer += msg + "\r\n"`; hasPendingOutput(); flushOutput()
    che fa UNA ::send() e cancella dal buffer i byte inviati.
  - Server::buildPollFds(): events = POLLIN | (client->hasPendingOutput() ? POLLOUT : 0).
  - Nel loop: if (revents & POLLOUT) client->flushOutput().
  - Se un client disconnette (QUIT/kick) con output pendente: prova un flush
    finale prima del delete, altrimenti l'ERROR/QUIT non arriva.
  Perché: il server non si blocca mai, non perde byte e rispetta il subject.


10. TABELLA "COSA / PERCHÉ" RIASSUNTIVA
---------------------------------------
  poll() unico            → richiesto dal subject; dorme finché serve
  O_NONBLOCK ovunque      → nessuna chiamata può bloccare l'intero server
  SO_REUSEADDR            → riavvio immediato sulla stessa porta
  SIG_IGN su SIGPIPE      → un client che sparisce non uccide il server
  map<int,Client*>        → lookup per fd, owner, niente copie/double-close
  map<string,Client*>     → lookup per nick, unicità garantita
  map<string,Channel*>    → lookup per canale, unicità garantita
  buffer per client       → TCP è uno stream: comandi spezzati/incollati
  cleanup a fine giro     → mai cancellare mentre si itera
  ~Server pulito          → valgrind senza leak, nessun fd rimasto aperto


11. CHECKLIST FINALE (per la valutazione)
-----------------------------------------
  [ ] Server.hpp non definisce `class Client` (solo forward declaration)
  [ ] Compila con -Wall -Wextra -Werror -std=c++98, senza warning
  [ ] Un solo poll() nel programma; nessun recv/send fuori da un giro di poll
  [ ] Tutti gli fd (listener + client) sono O_NONBLOCK
  [ ] Nessun controllo di errno dopo read/recv/send/write
  [ ] Nessun buffer overflow su recv (niente buf[n]=0 con n == sizeof buf)
  [ ] Comandi frammentati e multipli nello stesso pacchetto gestiti
  [ ] Nessun double close, nessun leak (valgrind), nessun fd residuo
  [ ] Ctrl+C chiude tutto in modo pulito; SIGPIPE ignorato
  [ ] Client non registrato può usare solo PASS/NICK/USER/CAP/PING/QUIT
  [ ] Nick e nomi canale unici; clientsByNick sempre coerente con i nick reali
  [ ] Canale vuoto → cancellato; uscita client → QUIT broadcast ai canali
