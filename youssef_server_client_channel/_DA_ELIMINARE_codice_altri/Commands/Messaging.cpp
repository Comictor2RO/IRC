/*
** =====================================================================================
**  MESSAGGI
**  PRIVMSG e NOTICE (stessa logica: NOTICE non genera mai risposte d'errore, per regola
**  IRC, così due bot non possono rispondersi all'infinito con errori)
** =====================================================================================
*/
#include "Server.hpp"                  // classe di cui questi sono metodi
#include "Client.hpp"                  // metodi del Client
#include "Channel.hpp"                 // broadcast nel canale
#include "IrcParser.hpp"               // struct IrcMessage

/* -------------------------------------------------------------------------------------
**  splitTargets()  [supporto]  "alice,#chan" -> ["alice", "#chan"]
** ------------------------------------------------------------------------------------- */
static std::vector<std::string> splitTargets(const std::string &str)
{
    std::vector<std::string> parts;                        // risultato
    std::string::size_type start = 0;                      // inizio del pezzo
    while (start <= str.size())                            // finché resta testo
    {
        std::string::size_type end = str.find(',', start); // prossima virgola
        if (end == std::string::npos)                      // ultimo pezzo
            end = str.size();
        if (end > start)                                   // ignora i pezzi vuoti
            parts.push_back(str.substr(start, end - start));
        start = end + 1;                                   // dopo la virgola
    }
    return parts;                                          // destinatari
}

void Server::handlePrivmsg(const IrcMessage &msg, Client *client)
{
    deliverMessage(msg, client, "PRIVMSG", true);          // con risposte d'errore
}

void Server::handleNotice(const IrcMessage &msg, Client *client)
{
    deliverMessage(msg, client, "NOTICE", false);          // senza risposte d'errore
}

/* =====================================================================================
**  deliverMessage()  —  PRIVMSG/NOTICE <dest1>[,<dest2>...] :<testo>
**   411 ERR_NORECIPIENT     nessun destinatario
**   412 ERR_NOTEXTTOSEND    nessun testo
**  Per ogni destinatario:
**   - canale (#/&): deve esistere (403) e il mittente deve esserne membro (404);
**     il messaggio va a tutti i membri TRANNE il mittente (il suo client lo mostra già);
**   - nick: deve esistere (401); il messaggio va solo a lui.
**  Formato inoltrato: ":mittente!user@host PRIVMSG <dest> :<testo>"
** ===================================================================================== */
void Server::deliverMessage(const IrcMessage &msg, Client *client, const std::string &command,
                            bool reportErrors)
{
    if (msg.params.empty())                                            // nessun destinatario
    {
        if (reportErrors)                                              // solo per PRIVMSG
            client->sendError("411", ":No recipient given (" + command + ")"); // ERR_NORECIPIENT
        return;
    }
    std::string text = msg.hasTrailing ? msg.trailing
                     : (msg.params.size() > 1 ? msg.params[1] : "");   // testo (anche senza ':' se è una parola)
    if (text.empty())                                                  // niente da mandare
    {
        if (reportErrors)
            client->sendError("412", ":No text to send");              // ERR_NOTEXTTOSEND
        return;
    }
    std::vector<std::string> targets = splitTargets(msg.params[0]);    // destinatari
    for (size_t i = 0; i < targets.size(); ++i)                        // ogni destinatario
    {
        const std::string &target = targets[i];                        // destinatario corrente
        if (target[0] == '#' || target[0] == '&')                      // è un canale
        {
            Channel *channel = getChannel(target);                     // cercalo
            if (!channel)                                              // non esiste
            {
                if (reportErrors)
                    client->sendError("403", target + " :No such channel"); // ERR_NOSUCHCHANNEL
                continue;
            }
            if (!channel->hasClient(*client))                          // il mittente non è dentro
            {
                if (reportErrors)
                    client->sendError("404", target + " :Cannot send to channel"); // ERR_CANNOTSENDTOCHAN
                continue;
            }
            channel->broadcast(":" + client->getPrefix() + " " + command + " "
                               + channel->getName() + " :" + text, client); // a tutti tranne il mittente
        }
        else                                                           // è un nick
        {
            Client *dest = getClientByNick(target);                    // cercalo
            if (!dest)                                                 // non esiste
            {
                if (reportErrors)
                    client->sendError("401", target + " :No such nick/channel"); // ERR_NOSUCHNICK
                continue;
            }
            dest->send(":" + client->getPrefix() + " " + command + " "
                       + dest->getNickname() + " :" + text);           // messaggio privato
        }
    }
}
