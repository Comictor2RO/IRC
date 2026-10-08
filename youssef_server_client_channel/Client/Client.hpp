#ifndef CLIENT_HPP                     // include guard: se CLIENT_HPP non è ancora definito...
#define CLIENT_HPP                     // ...lo definisce, così il contenuto viene incluso UNA sola volta

#include <string>                      // std::string: nick, user, buffer di ingresso/uscita
#include <vector>                      // std::vector: lista dei canali a cui il client è iscritto

// Nome del server usato come prefisso di TUTTE le risposte numeriche (":ircserv 001 nick ...").
// È qui perché è il Client che costruisce le risposte (sendReply/sendError) e il Server include Client.hpp.
#define SERVER_NAME "ircserv"

class Channel;                         // forward declaration: ci servono solo puntatori a Channel, non la classe completa

/*
** =====================================================================================
**  CLASSE Client  —  rappresenta UNA connessione TCP (un utente connesso al server)
** =====================================================================================
**  Cosa contiene:
**   - l'fd del socket della connessione (restituito da accept());
**   - l'identità IRC: nickname, username, realname, hostname;
**   - lo stato di registrazione (PASS ok? NICK + USER arrivati?);
**   - DUE buffer:
**       inBuffer  = byte ricevuti con recv() ma non ancora trasformati in righe complete
**                   (TCP è uno stream: un comando può arrivare spezzato o più comandi insieme);
**       outBuffer = byte da spedire al client, che verranno inviati con send() SOLO quando
**                   poll() segnala POLLOUT (regola del subject: niente send() senza poll());
**   - un flag "da eliminare": il Server non cancella mai un client mentre sta scorrendo
**     la lista dei fd, lo segna e lo rimuove a fine iterazione (cleanupClients()).
**
**  Chi crea / distrugge i Client: SOLO il Server (new in acceptNewClient, delete in removeClient).
**  Il distruttore chiude il fd (RAII): nessun altro deve chiamare close() su quel fd.
** =====================================================================================
*/
class Client
{
public:
    // ---------------------------------------------------------------- costruzione / distruzione
    Client(int fd, const std::string &hostname);   // fd del socket accettato + IP del client in formato testo
    ~Client();                                     // chiude il fd (unico punto in cui il fd del client viene chiuso)

    // ---------------------------------------------------------------- identità (getter)
    int                 getFD() const;             // fd del socket: chiave del client nelle map del Server
    const std::string   &getNickname() const;      // nick attuale ("" finché il client non manda NICK)
    const std::string   &getUsername() const;      // username dato con USER
    const std::string   &getRealname() const;      // realname dato con USER (parte dopo ':')
    const std::string   &getHostname() const;      // IP del client (es. "127.0.0.1")
    std::string         getPrefix() const;         // "nick!user@host": il mittente dei messaggi inoltrati

    // ---------------------------------------------------------------- identità (setter)
    void setNickname(const std::string &nickname); // ATTENZIONE: dagli handler usare Server::updateNick(),
                                                   // che aggiorna anche l'indice clientsByNick del Server
    void setUsername(const std::string &username); // salvato da USER
    void setRealname(const std::string &realname); // salvato da USER

    // ---------------------------------------------------------------- registrazione
    bool isAuth() const;                           // true se PASS è arrivato con la password giusta
    void setAuth(bool auth);                       // chiamato da PASS
    bool isRegistered() const;                     // true dopo PASS + NICK + USER (benvenuto 001 già inviato)
    void tryRegister();                            // completa la registrazione se è il momento giusto

    // ---------------------------------------------------------------- buffer di INGRESSO
    void                appendToBuffer(const std::string &data); // aggiunge i byte appena letti con recv()
    bool                extractLine(std::string &line);          // estrae UNA riga completa (senza \r\n)
    const std::string   &getBuffer() const;                      // contenuto non ancora completo (per controllarne la dimensione)
    bool                discardLongLine();                       // scarta una riga troppo lunga fino al suo '\n'

    // ---------------------------------------------------------------- buffer di USCITA
    void send(const std::string &msg);                           // accoda msg + "\r\n" (NON spedisce subito)
    void sendReply(const std::string &code, const std::string &text); // ":ircserv <code> <nick> <text>"
    void sendError(const std::string &code, const std::string &text); // uguale a sendReply, usato per gli errori
    bool hasPendingOutput() const;                               // true se c'è qualcosa da spedire -> POLLOUT
    bool flushOutput();                                          // UNA send() di ciò che è in coda; false = errore
    void discardOutput();                                        // butta l'uscita (connessione già morta)

    // ---------------------------------------------------------------- ciclo di vita
    void                markForDeletion(const std::string &reason = "Connection closed"); // segna da rimuovere
    bool                shouldDelete() const;                    // true se è stato segnato
    const std::string   &getQuitReason() const;                  // motivo mostrato agli altri nel messaggio QUIT

    // ---------------------------------------------------------------- canali
    void                    joinChannel(Channel *channel);       // chiamato da Channel::addClient
    void                    leaveChannel(Channel *channel);      // chiamato da Channel::removeClient
    std::vector<Channel *>  getChannels() const;                 // COPIA della lista (vedi commento nel .cpp)

private:
    // Copia e assegnazione vietate (dichiarate private e NON implementate, stile C++98):
    // due copie dello stesso Client chiuderebbero due volte lo stesso fd nel distruttore.
    Client(const Client &other);
    Client &operator=(const Client &other);

    int                     fd;            // socket della connessione
    std::string             nickname;      // nick IRC
    std::string             username;      // username IRC
    std::string             realname;      // nome reale
    std::string             hostname;      // IP del client
    std::string             inBuffer;      // byte ricevuti in attesa di formare una riga completa
    std::string             outBuffer;     // byte in attesa di essere spediti al prossimo POLLOUT
    bool                    auth;          // PASS corretta ricevuta
    bool                    registered;    // registrazione completata
    bool                    markDeletion;  // da rimuovere a fine iterazione del loop
    bool                    skippingLine;  // true mentre si scarta il resto di una riga troppo lunga
    std::string             quitReason;    // motivo della disconnessione
    std::vector<Channel *>  channels;      // canali a cui è iscritto (puntatori NON posseduti)
};

#endif                                     // fine dell'include guard
