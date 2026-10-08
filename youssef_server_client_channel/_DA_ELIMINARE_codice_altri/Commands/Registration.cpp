/*
** =====================================================================================
**  COMANDI DI CONNESSIONE E REGISTRAZIONE
**  PASS, NICK, USER, CAP, PING, PONG, QUIT
** =====================================================================================
**  Ordine tipico di un client (irssi, HexChat, nc):
**     CAP LS 302            (solo client moderni: chiede le capacità, rispondiamo "nessuna")
**     PASS <password>
**     NICK <nick>
**     USER <user> 0 * :<realname>
**  Quando arrivano PASS (giusta) + NICK + USER, Client::tryRegister() invia 001..005 + 422.
**  Prima di allora dispatchCommand() (nel Server) accetta solo questi comandi.
** =====================================================================================
*/
#include "Server.hpp"                  // la classe di cui questi sono metodi
#include "Client.hpp"                  // metodi del Client
#include "Channel.hpp"                 // broadcast nei canali (cambio nick)
#include "IrcParser.hpp"               // struct IrcMessage
#include <set>                         // destinatari senza duplicati
#include <cctype>                      // std::isalpha, std::isdigit

/* -------------------------------------------------------------------------------------
**  isValidNick()  [funzione di supporto, visibile solo in questo file]
**  Regole (RFC 2812, con lunghezza massima 30 come annunciato in 005 NICKLEN=30):
**   - primo carattere: lettera o uno tra  [ ] \ ` _ ^ { | }
**   - altri caratteri: lettere, cifre, '-' e gli stessi speciali
**  Quindi niente spazi, virgole, '#', ':' ecc. (romperebbero la sintassi dei comandi).
** ------------------------------------------------------------------------------------- */
static bool isSpecialNickChar(char c)
{
    return std::string("[]\\`_^{|}").find(c) != std::string::npos; // c è uno dei caratteri speciali?
}

static bool isValidNick(const std::string &nick)
{
    if (nick.empty() || nick.size() > 30)                                       // vuoto o troppo lungo
        return false;
    if (!std::isalpha(static_cast<unsigned char>(nick[0])) && !isSpecialNickChar(nick[0])) // primo carattere
        return false;
    for (std::string::size_type i = 1; i < nick.size(); ++i)                    // caratteri successivi
    {
        unsigned char c = static_cast<unsigned char>(nick[i]);                  // carattere corrente
        if (!std::isalnum(c) && c != '-' && !isSpecialNickChar(nick[i]))         // non ammesso?
            return false;
    }
    return true;                                                                // nick valido
}

/* =====================================================================================
**  PASS <password>
**   461 ERR_NEEDMOREPARAMS  nessun parametro
**   462 ERR_ALREADYREGISTRED già registrato
**   464 ERR_PASSWDMISMATCH  password sbagliata -> ERROR + disconnessione
**  Se è giusta il client viene segnato "auth"; la registrazione si completa con NICK+USER.
** ===================================================================================== */
void Server::handlePass(const IrcMessage &msg, Client *client)
{
    if (client->isRegistered())                                    // dopo la registrazione non serve
    {
        client->sendError("462", ":You may not reregister");       // ERR_ALREADYREGISTRED
        return;
    }
    std::string given = msg.params.empty() ? msg.trailing : msg.params[0]; // accetta "PASS x" e "PASS :x"
    if (given.empty())                                             // niente password
    {
        client->sendError("461", "PASS :Not enough parameters");   // ERR_NEEDMOREPARAMS
        return;
    }
    if (given != password)                                         // password diversa da av[2]
    {
        client->sendError("464", ":Password incorrect");           // ERR_PASSWDMISMATCH
        client->send("ERROR :Closing Link: " + client->getHostname() + " (Password incorrect)"); // avviso
        client->markForDeletion("Password incorrect");             // verrà chiuso dopo l'invio
        return;
    }
    client->setAuth(true);                                         // password corretta
    client->tryRegister();                                         // se NICK e USER ci sono già -> benvenuto
}

/* =====================================================================================
**  NICK <nick>
**   431 ERR_NONICKNAMEGIVEN  nessun nick
**   432 ERR_ERRONEUSNICKNAME caratteri non validi
**   433 ERR_NICKNAMEINUSE    già usato da un altro (maiuscole/minuscole ignorate)
**  Se il client è già registrato, il cambio viene annunciato a lui e a chi condivide
**  un canale con lui: ":vecchioNick!user@host NICK :nuovoNick".
**  Il nick si cambia SEMPRE con Server::updateNick() (tiene aggiornato clientsByNick).
** ===================================================================================== */
void Server::handleNick(const IrcMessage &msg, Client *client)
{
    std::string newNick = msg.params.empty() ? msg.trailing : msg.params[0]; // "NICK x" o "NICK :x"
    if (newNick.empty())                                           // niente nick
    {
        client->sendError("431", ":No nickname given");            // ERR_NONICKNAMEGIVEN
        return;
    }
    if (!isValidNick(newNick))                                     // caratteri vietati
    {
        client->sendError("432", newNick + " :Erroneous nickname");// ERR_ERRONEUSNICKNAME
        return;
    }
    if (newNick == client->getNickname())                          // identico a quello attuale
        return;                                                    // nulla da fare
    if (isNickTaken(newNick, client))                              // usato da un altro client
    {
        client->sendError("433", newNick + " :Nickname is already in use"); // ERR_NICKNAMEINUSE
        return;
    }
    if (client->isRegistered())                                    // cambio nick "in diretta"
    {
        std::string notice = ":" + client->getPrefix() + " NICK :" + newNick; // prefisso col VECCHIO nick
        std::set<Client *> recipients;                             // chi deve saperlo
        recipients.insert(client);                                 // il client stesso
        std::vector<Channel *> chans = client->getChannels();      // i suoi canali
        for (size_t i = 0; i < chans.size(); ++i)                  // ogni canale
        {
            std::vector<Client *> members = chans[i]->getClients();// i membri
            recipients.insert(members.begin(), members.end());     // tutti (il set evita i doppioni)
        }
        for (std::set<Client *>::iterator it = recipients.begin(); it != recipients.end(); ++it) // ogni destinatario
            (*it)->send(notice);                                   // accoda l'annuncio
    }
    updateNick(client, newNick);                                   // aggiorna Client + indice del Server
    client->tryRegister();                                         // forse ora la registrazione è completa
}

/* =====================================================================================
**  USER <username> <mode> <unused> :<realname>
**   461 ERR_NEEDMOREPARAMS   meno di 4 parametri (contando il trailing)
**   462 ERR_ALREADYREGISTRED già registrato
** ===================================================================================== */
void Server::handleUser(const IrcMessage &msg, Client *client)
{
    if (client->isRegistered())                                    // non si può ripetere
    {
        client->sendError("462", ":You may not reregister");       // ERR_ALREADYREGISTRED
        return;
    }
    if (msg.params.size() < 3 || (!msg.hasTrailing && msg.params.size() < 4)) // mancano parametri
    {
        client->sendError("461", "USER :Not enough parameters");   // ERR_NEEDMOREPARAMS
        return;
    }
    client->setUsername(msg.params[0]);                            // primo parametro = username
    client->setRealname(msg.hasTrailing ? msg.trailing : msg.params[3]); // realname
    client->tryRegister();                                         // forse ora è completa
}

/* =====================================================================================
**  CAP LS | CAP REQ :<capacità> | CAP END
**  irssi e HexChat iniziano con "CAP LS 302". Non supportiamo nessuna estensione IRCv3:
**   LS  -> lista vuota  ":ircserv CAP * LS :"
**   REQ -> rifiuto       ":ircserv CAP * NAK :<capacità richieste>"
**   END -> nulla (il client prosegue con PASS/NICK/USER)
** ===================================================================================== */
void Server::handleCap(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())                                        // "CAP" da solo
        return;                                                    // ignora
    std::string target = client->getNickname().empty() ? "*" : client->getNickname(); // destinatario
    if (msg.params[0] == "LS")                                     // richiesta della lista
        client->send(":" SERVER_NAME " CAP " + target + " LS :");  // nessuna capacità
    else if (msg.params[0] == "REQ")                               // richiesta di attivarne qualcuna
        client->send(":" SERVER_NAME " CAP " + target + " NAK :" + msg.trailing); // rifiuta
}

/* =====================================================================================
**  PING <token>   ->   ":ircserv PONG ircserv :<token>"
**  I client mandano PING per verificare che il server sia vivo: senza risposta, dopo un
**  po' di tempo chiudono la connessione.
**   409 ERR_NOORIGIN se manca il token
** ===================================================================================== */
void Server::handlePing(const IrcMessage &msg, Client *client)
{
    std::string token = msg.params.empty() ? msg.trailing : msg.params[0]; // "PING x" o "PING :x"
    if (token.empty())                                             // nessun token
    {
        client->sendError("409", ":No origin specified");          // ERR_NOORIGIN
        return;
    }
    client->send(":" SERVER_NAME " PONG " SERVER_NAME " :" + token); // risposta
}

void Server::handlePong(const IrcMessage &msg, Client *client)
{
    (void)msg;                         // il server non manda PING, quindi i PONG si ignorano
    (void)client;                      // (void) evita i warning per parametri inutilizzati
}

/* =====================================================================================
**  QUIT [:<motivo>]
**  Il client vuole uscire:
**   1. gli si manda "ERROR :Closing Link: ..." (conferma standard di chiusura);
**   2. lo si segna con il motivo "Quit: <motivo>".
**  Il messaggio ":nick!user@host QUIT :Quit: <motivo>" agli altri membri dei suoi canali
**  lo manda Server::removeClient() quando lo elimina davvero (così vale anche per chi si
**  disconnette senza QUIT, ad es. chiudendo nc).
** ===================================================================================== */
void Server::handleQuit(const IrcMessage &msg, Client *client)
{
    std::string reason = msg.hasTrailing ? msg.trailing
                       : (msg.params.empty() ? "Client Quit" : msg.params[0]); // motivo dato dal client
    client->send("ERROR :Closing Link: " + client->getHostname() + " (Quit: " + reason + ")"); // conferma
    client->markForDeletion("Quit: " + reason);                    // rimosso dopo aver spedito l'ERROR
}
