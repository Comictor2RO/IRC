#include "Client.hpp"                  // dichiarazione della classe Client
#include <sys/socket.h>                // ::send()
#include <unistd.h>                    // close()

/* =====================================================================================
**  COSTRUTTORE
**  Riceve il fd restituito da accept() e l'IP del client (già convertito in testo dal Server).
**  Tutti i flag partono a false: il client appena connesso non è né autenticato né registrato.
** ===================================================================================== */
Client::Client(int fd, const std::string &hostname)
    : fd(fd),                          // salva il socket della connessione
      hostname(hostname),              // salva l'IP, usato nel prefisso nick!user@host
      auth(false),                     // PASS non ancora ricevuta
      registered(false),               // registrazione non completata
      markDeletion(false),             // non deve essere rimosso
      skippingLine(false),             // nessuna riga troppo lunga da scartare
      quitReason("Connection closed")  // motivo di default se la connessione cade senza QUIT
{}

/* =====================================================================================
**  DISTRUTTORE
**  È l'UNICO punto che chiude il fd del client (RAII): il Server fa solo "delete client".
**  Così non c'è mai il rischio di chiudere due volte lo stesso fd (double close).
** ===================================================================================== */
Client::~Client()
{
    if (fd >= 0)                       // controllo difensivo: fd valido?
        close(fd);                     // chiude il socket: il client vede la connessione chiusa
}

// ------------------------------------------------------------------------------ GETTER

int Client::getFD() const
{
    return fd;                         // restituisce il socket
}

const std::string &Client::getNickname() const
{
    return nickname;                   // riferimento costante: niente copia, niente modifica
}

const std::string &Client::getUsername() const
{
    return username;                   // riferimento costante allo username
}

const std::string &Client::getRealname() const
{
    return realname;                   // riferimento costante al realname
}

const std::string &Client::getHostname() const
{
    return hostname;                   // riferimento costante all'IP
}

/* =====================================================================================
**  getPrefix()
**  Restituisce l'identità completa dell'utente nel formato IRC "nick!user@host".
**  È ciò che va dopo ':' quando il server INOLTRA un messaggio di questo utente agli altri,
**  ad es. ":alice!al@127.0.0.1 PRIVMSG #chan :ciao". I client IRC usano il prefisso per
**  sapere CHI ha mandato il messaggio.
** ===================================================================================== */
std::string Client::getPrefix() const
{
    std::string nick = nickname.empty() ? "*" : nickname;   // "*" se il nick non c'è ancora
    std::string user = username.empty() ? "*" : username;   // "*" se lo username non c'è ancora
    return nick + "!" + user + "@" + hostname;              // compone "nick!user@host"
}

// ------------------------------------------------------------------------------ SETTER

void Client::setNickname(const std::string &nickname)
{
    this->nickname = nickname;         // this-> distingue il membro dal parametro omonimo
}

void Client::setUsername(const std::string &username)
{
    this->username = username;         // salva lo username
}

void Client::setRealname(const std::string &realname)
{
    this->realname = realname;         // salva il realname
}

// ------------------------------------------------------------------------------ REGISTRAZIONE

bool Client::isAuth() const
{
    return auth;                       // true se la password era corretta
}

void Client::setAuth(bool auth)
{
    this->auth = auth;                 // impostato a true da PASS
}

bool Client::isRegistered() const
{
    return registered;                 // true dopo il benvenuto
}

/* =====================================================================================
**  tryRegister()
**  Viene chiamata dopo ogni NICK e USER. Funzionamento:
**   1. se il client è già registrato, oppure gli manca ancora NICK o USER -> non fa nulla
**      (la registrazione si completa solo quando ci sono entrambi);
**   2. se ha NICK e USER ma NON ha mandato una PASS corretta -> errore 464, messaggio
**      ERROR e disconnessione (il subject richiede la password per connettersi);
**   3. altrimenti segna il client come registrato e invia il "benvenuto":
**        001 RPL_WELCOME   002 RPL_YOURHOST   003 RPL_CREATED   004 RPL_MYINFO
**        005 RPL_ISUPPORT  (dice al client IRC quali canali/modi supportiamo)
**        422 ERR_NOMOTD    (non abbiamo un "messaggio del giorno": irssi/HexChat
**                           aspettano la fine del MOTD o questo errore per dire "connesso")
**  Da questo momento il client può usare tutti i comandi (JOIN, PRIVMSG, ...).
** ===================================================================================== */
void Client::tryRegister()
{
    if (registered || nickname.empty() || username.empty())  // già registrato o mancano dati?
        return;                                                // aspetta il prossimo comando
    if (!auth)                                                 // NICK + USER ma niente PASS valida
    {
        sendError("464", ":Password required");              // ERR_PASSWDMISMATCH
        send("ERROR :Closing Link: " + hostname + " (Password required)"); // avviso di chiusura
        markForDeletion("Password required");                 // verrà chiuso dopo aver spedito i messaggi
        return;                                                // non registrare
    }
    registered = true;                                         // da ora è un utente registrato
    sendReply("001", ":Welcome to the ft_irc Network " + getPrefix());           // benvenuto
    sendReply("002", ":Your host is " SERVER_NAME ", running version 1.0");      // info server
    sendReply("003", ":This server was created for the 42 ft_irc project");      // data creazione
    sendReply("004", SERVER_NAME " 1.0 o itkol");                                // nome, versione, modi utente, modi canale
    sendReply("005", "CHANTYPES=#& PREFIX=(o)@ CHANMODES=,k,l,it NICKLEN=30 "
                     ":are supported by this server");                           // capacità supportate
    sendReply("422", ":MOTD File is missing");                                   // fine sequenza di login
}

// ------------------------------------------------------------------------------ BUFFER DI INGRESSO

/* =====================================================================================
**  appendToBuffer()
**  Il Server legge con recv() un pezzo di dati (fino a 512 byte) e lo aggiunge qui.
**  NON si elabora subito: il pezzo potrebbe contenere mezza riga ("JO") oppure più righe
**  ("NICK a\r\nUSER a 0 * :A\r\n"). Le righe complete si estraggono con extractLine().
** ===================================================================================== */
void Client::appendToBuffer(const std::string &data)
{
    inBuffer += data;                  // accoda i nuovi byte a quelli rimasti dal giro precedente
}

/* =====================================================================================
**  extractLine()
**  Cerca nel buffer di ingresso il primo '\n'. Se lo trova:
**    - copia in `line` tutto ciò che c'è prima del '\n';
**    - lo rimuove dal buffer (insieme al '\n');
**    - se la riga finisce con '\r' lo toglie (IRC standard usa "\r\n", nc senza -C manda solo "\n");
**    - restituisce true.
**  Se nel buffer non c'è ancora un '\n' restituisce false e lascia i dati dove sono:
**  il resto della riga arriverà con un prossimo recv(). È esattamente il test del subject
**  "com^Dman^Dd\n": tre recv() diverse che formano un solo comando.
**  Il Server la chiama in un ciclo while finché restituisce true (più comandi in un pacchetto).
**  Se si stava scartando una riga troppo lunga (skippingLine, vedi discardLongLine), il
**  pezzo fino al '\n' è la CODA di quella riga: viene restituita una riga vuota, che il
**  Server ignora, e da lì in poi si torna a leggere normalmente.
** ===================================================================================== */
bool Client::extractLine(std::string &line)
{
    std::string::size_type pos = inBuffer.find('\n');          // posizione del primo fine riga
    if (pos == std::string::npos)                              // nessuna riga completa
        return false;                                          // aspetta altri dati
    line = inBuffer.substr(0, pos);                            // riga senza il '\n'
    inBuffer.erase(0, pos + 1);                                // rimuove riga + '\n' dal buffer
    if (skippingLine)                                          // era la coda di una riga troppo lunga?
    {
        skippingLine = false;                                  // da qui si torna a leggere normalmente
        line.clear();                                          // riga vuota: il Server la ignora
        return true;
    }
    if (!line.empty() && line[line.size() - 1] == '\r')        // finiva con "\r\n"?
        line.erase(line.size() - 1);                           // toglie anche il '\r'
    return true;                                               // una riga è pronta
}

const std::string &Client::getBuffer() const
{
    return inBuffer;                   // usato dal Server per controllare righe troppo lunghe
}

/* =====================================================================================
**  discardLongLine()
**  Chiamata dal Server quando nel buffer ci sono più di 510 byte senza '\n': quella riga
**  è oltre il limite IRC. Svuota il buffer (altrimenti crescerebbe senza fine) e attiva
**  skippingLine: anche il resto della riga, che arriverà con i prossimi recv(), sarà
**  scartato fino al suo '\n' invece di essere letto come un comando a sé.
**  Ritorna true solo la PRIMA volta, così il Server manda l'errore 417 una volta sola.
** ===================================================================================== */
bool Client::discardLongLine()
{
    bool first = !skippingLine;        // era già in corso uno scarto?
    inBuffer.clear();                  // butta i dati incompleti
    skippingLine = true;               // scarta fino al prossimo '\n'
    return first;                      // true = primo scarto di questa riga
}

// ------------------------------------------------------------------------------ BUFFER DI USCITA

/* =====================================================================================
**  send()
**  NON chiama ::send(). Aggiunge il messaggio (con il terminatore IRC "\r\n") alla coda
**  di uscita. Il Server, in buildPollFds(), vede che la coda non è vuota e chiede a poll()
**  anche POLLOUT su questo fd; quando poll() dice "puoi scrivere" chiama flushOutput().
**  Perché: il subject vieta read/recv/write/send che non passino da poll() (voto 0), e
**  con un socket non bloccante una ::send() può spedire solo una parte dei byte: con la
**  coda il resto non va perso, verrà spedito al POLLOUT successivo.
** ===================================================================================== */
void Client::send(const std::string &msg)
{
    outBuffer += msg + "\r\n";         // accoda il messaggio con il terminatore IRC
}

/* =====================================================================================
**  sendReply() / sendError()
**  Formattano una risposta numerica del server: ":ircserv <code> <destinatario> <testo>".
**  Il destinatario è il nick del client, oppure "*" se non ne ha ancora uno.
**  Esempio: sendError("433", "bob :Nickname is already in use")
**        -> ":ircserv 433 * bob :Nickname is already in use"
**  Sono identiche: esistono entrambe solo per rendere più leggibile il codice degli handler.
** ===================================================================================== */
void Client::sendReply(const std::string &code, const std::string &text)
{
    std::string target = nickname.empty() ? "*" : nickname;               // destinatario
    send(":" SERVER_NAME " " + code + " " + target + " " + text);         // accoda la risposta
}

void Client::sendError(const std::string &code, const std::string &text)
{
    sendReply(code, text);             // stesso formato delle risposte
}

bool Client::hasPendingOutput() const
{
    return !outBuffer.empty();         // true se ci sono byte da spedire
}

/* =====================================================================================
**  flushOutput()
**  Chiamata dal Server SOLO quando poll() ha segnalato POLLOUT su questo fd.
**  Fa UNA sola ::send() con tutto il contenuto della coda:
**   - ritorna un numero >= 0 di byte spediti -> li toglie dalla coda (se ne ha spediti
**     solo una parte, il resto resta in coda e partirà al prossimo POLLOUT);
**   - ritorna -1 -> errore (es. il client ha chiuso): restituisce false e sarà il Server
**     a segnare il client da eliminare.
**  Non si guarda errno dopo send(): il subject lo vieta.
** ===================================================================================== */
bool Client::flushOutput()
{
    if (outBuffer.empty())                                             // niente da spedire
        return true;                                                   // nessun errore
    ssize_t sent = ::send(fd, outBuffer.c_str(), outBuffer.size(), 0); // :: = funzione di sistema, non il nostro metodo
    if (sent < 0)                                                      // errore sul socket
        return false;                                                  // il Server lo eliminerà
    outBuffer.erase(0, static_cast<std::string::size_type>(sent));     // toglie i byte già spediti
    return true;                                                       // tutto ok
}

void Client::discardOutput()
{
    outBuffer.clear();                 // la connessione è morta: inutile tentare di spedire
}

// ------------------------------------------------------------------------------ CICLO DI VITA

/* =====================================================================================
**  markForDeletion()
**  Segna il client come "da rimuovere" e ne salva il motivo (usato nel messaggio QUIT
**  che il Server manda agli altri membri dei suoi canali).
**  Il client NON viene cancellato subito: il Server lo rimuove in cleanupClients(), alla
**  fine dell'iterazione del loop, e solo dopo aver spedito ciò che era in coda (es. ERROR).
**  Se viene chiamata più volte vale il PRIMO motivo (quello vero).
** ===================================================================================== */
void Client::markForDeletion(const std::string &reason)
{
    if (markDeletion)                  // già segnato?
        return;                        // mantieni il motivo originale
    markDeletion = true;               // segna da eliminare
    quitReason = reason;               // salva il motivo
}

bool Client::shouldDelete() const
{
    return markDeletion;               // true se va rimosso
}

const std::string &Client::getQuitReason() const
{
    return quitReason;                 // motivo della disconnessione
}

// ------------------------------------------------------------------------------ CANALI

void Client::joinChannel(Channel *channel)
{
    channels.push_back(channel);       // ricorda che il client è in questo canale
}

void Client::leaveChannel(Channel *channel)
{
    for (std::vector<Channel *>::iterator it = channels.begin(); it != channels.end(); ++it) // scorre i canali
    {
        if (*it == channel)            // trovato il canale da lasciare
        {
            channels.erase(it);        // lo toglie dalla lista
            return;                    // l'iteratore ora non è più valido: esci subito
        }
    }
}

/* =====================================================================================
**  getChannels()
**  Restituisce una COPIA del vettore, non un riferimento. Motivo: chi la usa spesso fa
**  channel->removeClient(*client) dentro il ciclo, e removeClient chiama leaveChannel()
**  che modifica il vettore originale. Scorrere l'originale mentre viene modificato
**  invaliderebbe gli iteratori (comportamento indefinito).
** ===================================================================================== */
std::vector<Channel *> Client::getChannels() const
{
    return channels;                   // copia della lista dei canali
}
