/*
** =====================================================================================
**  COMANDI DI CANALE
**  JOIN, PART, TOPIC, KICK, INVITE, MODE, WHO
** =====================================================================================
**  Quando arrivano qui il client è SICURAMENTE registrato: lo garantisce
**  Server::dispatchCommand() (altrimenti risponde 451).
**  Comandi riservati agli operatori (richiesta del subject):
**     KICK, INVITE, MODE (modifica), TOPIC (solo se il canale è +t)
** =====================================================================================
*/
#include "Server.hpp"                  // classe di cui questi sono metodi
#include "Client.hpp"                  // metodi del Client
#include "Channel.hpp"                 // metodi del Channel
#include "IrcParser.hpp"               // struct IrcMessage
#include <cstdlib>                     // std::strtol: converte il limite di +l
#include <sstream>                     // std::ostringstream: numero -> testo

/* -------------------------------------------------------------------------------------
**  splitComma()  [supporto]  "#a,#b,#c" -> ["#a", "#b", "#c"]
**  JOIN e PART accettano più canali separati da virgole.
** ------------------------------------------------------------------------------------- */
static std::vector<std::string> splitComma(const std::string &str)
{
    std::vector<std::string> parts;                        // risultato
    std::string::size_type start = 0;                      // inizio del pezzo corrente
    while (start <= str.size())                            // finché non abbiamo letto tutto
    {
        std::string::size_type end = str.find(',', start); // prossima virgola
        if (end == std::string::npos)                      // nessuna: ultimo pezzo
            end = str.size();                              // arriva a fine stringa
        if (end > start)                                   // salta i pezzi vuoti (",,")
            parts.push_back(str.substr(start, end - start)); // aggiunge il pezzo
        start = end + 1;                                   // dopo la virgola
    }
    return parts;                                          // lista dei pezzi
}

/* -------------------------------------------------------------------------------------
**  isValidChannelName()  [supporto]
**  Inizia con '#' o '&', ha almeno un carattere dopo, al massimo 50 caratteri, e non
**  contiene spazi, virgole, BEL (\a), \r, \n o \0 (regole RFC 2812).
**  Il '\r' conta: con "JOIN #a\r\r\n" senza questo controllo nascerebbe un canale "#a\r",
**  diverso da "#a" e invisibile a chi lo cerca.
** ------------------------------------------------------------------------------------- */
static bool isValidChannelName(const std::string &name)
{
    static const std::string forbidden(" ,\a\r\n\0", 6);  // lunghezza esplicita: include anche '\0'
    if (name.size() < 2 || name.size() > 50)               // troppo corto o troppo lungo
        return false;
    if (name[0] != '#' && name[0] != '&')                  // prefisso obbligatorio
        return false;
    return name.find_first_of(forbidden) == std::string::npos; // nessun carattere vietato
}

/* -------------------------------------------------------------------------------------
**  isChannelName()  [supporto]  true se il target di MODE/WHO è un canale e non un nick
** ------------------------------------------------------------------------------------- */
static bool isChannelName(const std::string &target)
{
    return !target.empty() && (target[0] == '#' || target[0] == '&'); // inizia con # o &
}

/* =====================================================================================
**  JOIN <#c1>[,<#c2>...] [<chiave1>[,<chiave2>...]]
**  Per ogni canale della lista:
**   476 ERR_BADCHANMASK    nome non valido
**   473 ERR_INVITEONLYCHAN canale +i e non sei stato invitato
**   475 ERR_BADCHANNELKEY  canale +k e chiave sbagliata o assente
**   471 ERR_CHANNELISFULL  canale +l pieno
**  Se è tutto ok (o se il canale non esiste e viene creato, e allora sei operatore):
**   - ":nick!user@host JOIN #c" a TUTTI i membri (anche a chi entra: è la conferma);
**   - 332 con il topic, se esiste;
**   - 353 + 366 con la lista dei membri.
**  Se sei già nel canale la richiesta viene ignorata.
** ===================================================================================== */
void Server::handleJoin(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())                                            // manca il canale
    {
        client->sendError("461", "JOIN :Not enough parameters");       // ERR_NEEDMOREPARAMS
        return;
    }
    std::vector<std::string> names = splitComma(msg.params[0]);        // canali richiesti
    std::vector<std::string> keys;                                     // chiavi (facoltative)
    if (msg.params.size() > 1)                                         // ci sono chiavi?
        keys = splitComma(msg.params[1]);                              // una per canale, nello stesso ordine

    for (size_t i = 0; i < names.size(); ++i)                          // ogni canale richiesto
    {
        const std::string &name = names[i];                            // nome del canale
        std::string key = i < keys.size() ? keys[i] : "";              // chiave corrispondente
        if (!isValidChannelName(name))                                 // nome non valido
        {
            client->sendError("476", name + " :Bad Channel Mask");     // ERR_BADCHANMASK
            continue;                                                  // passa al prossimo
        }
        Channel *channel = getChannel(name);                           // esiste già?
        if (channel)                                                   // sì: controlla i modi
        {
            if (channel->hasClient(*client))                           // già dentro
                continue;                                              // ignora
            if (channel->isInviteOnly() && !channel->isInvited(*client)) // +i senza invito
            {
                client->sendError("473", name + " :Cannot join channel (+i)"); // ERR_INVITEONLYCHAN
                continue;
            }
            if (channel->hasKey() && key != channel->getKey())         // +k e chiave sbagliata
            {
                client->sendError("475", name + " :Cannot join channel (+k)"); // ERR_BADCHANNELKEY
                continue;
            }
            if (channel->isFull())                                     // +l pieno
            {
                client->sendError("471", name + " :Cannot join channel (+l)"); // ERR_CHANNELISFULL
                continue;
            }
        }
        else                                                           // non esiste
            channel = createChannel(name);                             // crealo (chi entra sarà operatore)

        channel->addClient(*client);                                   // entra (il primo diventa operatore)
        channel->broadcast(":" + client->getPrefix() + " JOIN " + channel->getName()); // annuncio a tutti
        if (!channel->getTopic().empty())                              // c'è un topic?
            channel->sendTopic(*client);                               // 332
        channel->sendNames(*client);                                   // 353 + 366
    }
}

/* =====================================================================================
**  PART <#c1>[,<#c2>...] [:<motivo>]
**   403 ERR_NOSUCHCHANNEL  il canale non esiste
**   442 ERR_NOTONCHANNEL   non sei nel canale
**  Altrimenti ":nick!user@host PART #c :motivo" a tutti (anche a chi esce), poi l'uscita.
**  Se il canale resta vuoto viene distrutto.
** ===================================================================================== */
void Server::handlePart(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())                                            // manca il canale
    {
        client->sendError("461", "PART :Not enough parameters");       // ERR_NEEDMOREPARAMS
        return;
    }
    std::vector<std::string> names = splitComma(msg.params[0]);        // canali da lasciare
    for (size_t i = 0; i < names.size(); ++i)                          // ogni canale
    {
        Channel *channel = getChannel(names[i]);                       // cerca il canale
        if (!channel)                                                  // non esiste
        {
            client->sendError("403", names[i] + " :No such channel");  // ERR_NOSUCHCHANNEL
            continue;
        }
        if (!channel->hasClient(*client))                              // non ne fa parte
        {
            client->sendError("442", names[i] + " :You're not on that channel"); // ERR_NOTONCHANNEL
            continue;
        }
        std::string partMsg = ":" + client->getPrefix() + " PART " + channel->getName(); // messaggio base
        if (msg.hasTrailing && !msg.trailing.empty())                  // c'è un motivo?
            partMsg += " :" + msg.trailing;                            // aggiungilo
        channel->broadcast(partMsg);                                   // a tutti, compreso chi esce
        channel->removeClient(*client);                                // esce dal canale
        removeChannelIfEmpty(channel);                                 // canale vuoto -> distrutto
    }
}

/* =====================================================================================
**  TOPIC <#c>               -> mostra il topic (332 / 331)
**  TOPIC <#c> :<testo>      -> imposta il topic (vuoto = lo cancella)
**   403 canale inesistente | 442 non sei nel canale
**   482 ERR_CHANOPRIVSNEEDED canale +t e non sei operatore
**  Il cambio viene annunciato a tutti: ":nick!user@host TOPIC #c :<testo>"
** ===================================================================================== */
void Server::handleTopic(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())                                            // manca il canale
    {
        client->sendError("461", "TOPIC :Not enough parameters");      // ERR_NEEDMOREPARAMS
        return;
    }
    Channel *channel = getChannel(msg.params[0]);                      // cerca il canale
    if (!channel)                                                      // non esiste
    {
        client->sendError("403", msg.params[0] + " :No such channel"); // ERR_NOSUCHCHANNEL
        return;
    }
    if (!channel->hasClient(*client))                                  // non ne fa parte
    {
        client->sendError("442", channel->getName() + " :You're not on that channel"); // ERR_NOTONCHANNEL
        return;
    }
    if (!msg.hasTrailing && msg.params.size() < 2)                     // nessun nuovo topic: è una lettura
    {
        channel->sendTopic(*client);                                   // 332 o 331
        return;
    }
    if (channel->isTopicRestricted() && !channel->isOperator(*client)) // +t e non operatore
    {
        client->sendError("482", channel->getName() + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }
    std::string topic = msg.hasTrailing ? msg.trailing : msg.params[1]; // nuovo topic
    channel->setTopic(topic);                                          // salvalo
    channel->broadcast(":" + client->getPrefix() + " TOPIC " + channel->getName() + " :" + topic); // annuncio
}

/* =====================================================================================
**  KICK <#c> <nick> [:<motivo>]          (solo operatori)
**   461 parametri mancanti | 403 canale inesistente | 442 non sei nel canale
**   482 non sei operatore  | 401 nick inesistente   | 441 il bersaglio non è nel canale
**  Altrimenti ":op!user@host KICK #c nick :motivo" a TUTTI (anche all'espulso, prima che
**  esca), poi l'espulso viene tolto. Il motivo di default è il nick di chi espelle.
** ===================================================================================== */
void Server::handleKick(const IrcMessage &msg, Client *client)
{
    if (msg.params.size() < 2)                                         // mancano canale o nick
    {
        client->sendError("461", "KICK :Not enough parameters");       // ERR_NEEDMOREPARAMS
        return;
    }
    Channel *channel = getChannel(msg.params[0]);                      // cerca il canale
    if (!channel)                                                      // non esiste
    {
        client->sendError("403", msg.params[0] + " :No such channel"); // ERR_NOSUCHCHANNEL
        return;
    }
    if (!channel->hasClient(*client))                                  // chi espelle non è nel canale
    {
        client->sendError("442", channel->getName() + " :You're not on that channel"); // ERR_NOTONCHANNEL
        return;
    }
    if (!channel->isOperator(*client))                                 // non è operatore
    {
        client->sendError("482", channel->getName() + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }
    Client *target = getClientByNick(msg.params[1]);                   // chi va espulso
    if (!target)                                                       // nick inesistente
    {
        client->sendError("401", msg.params[1] + " :No such nick/channel"); // ERR_NOSUCHNICK
        return;
    }
    if (!channel->hasClient(*target))                                  // non è nel canale
    {
        client->sendError("441", target->getNickname() + " " + channel->getName()
                          + " :They aren't on that channel");          // ERR_USERNOTINCHANNEL
        return;
    }
    std::string reason = (msg.hasTrailing && !msg.trailing.empty())
                       ? msg.trailing : client->getNickname();         // motivo o nick dell'operatore
    channel->broadcast(":" + client->getPrefix() + " KICK " + channel->getName() + " "
                       + target->getNickname() + " :" + reason);       // annuncio a tutti
    channel->removeClient(*target);                                    // espulsione
    removeChannelIfEmpty(channel);                                     // se un op si è espulso da solo ed era l'ultimo
}

/* =====================================================================================
**  INVITE <nick> <#c>                      (solo operatori)
**   461 parametri mancanti | 401 nick inesistente | 403 canale inesistente
**   442 non sei nel canale | 482 non sei operatore | 443 il bersaglio è già nel canale
**  Altrimenti: l'invito viene registrato nel canale (permette il JOIN anche se +i),
**   341 RPL_INVITING a chi invita, ":op!user@host INVITE nick #c" all'invitato.
** ===================================================================================== */
void Server::handleInvite(const IrcMessage &msg, Client *client)
{
    if (msg.params.size() < 2)                                         // mancano nick o canale
    {
        client->sendError("461", "INVITE :Not enough parameters");     // ERR_NEEDMOREPARAMS
        return;
    }
    Client *target = getClientByNick(msg.params[0]);                   // chi invitare
    if (!target)                                                       // nick inesistente
    {
        client->sendError("401", msg.params[0] + " :No such nick/channel"); // ERR_NOSUCHNICK
        return;
    }
    Channel *channel = getChannel(msg.params[1]);                      // dove invitarlo
    if (!channel)                                                      // canale inesistente
    {
        client->sendError("403", msg.params[1] + " :No such channel"); // ERR_NOSUCHCHANNEL
        return;
    }
    if (!channel->hasClient(*client))                                  // chi invita non è nel canale
    {
        client->sendError("442", channel->getName() + " :You're not on that channel"); // ERR_NOTONCHANNEL
        return;
    }
    if (!channel->isOperator(*client))                                 // non è operatore
    {
        client->sendError("482", channel->getName() + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }
    if (channel->hasClient(*target))                                   // è già dentro
    {
        client->sendError("443", target->getNickname() + " " + channel->getName()
                          + " :is already on channel");                // ERR_USERONCHANNEL
        return;
    }
    channel->addInvite(*target);                                       // registra l'invito
    client->sendReply("341", target->getNickname() + " " + channel->getName()); // RPL_INVITING
    target->send(":" + client->getPrefix() + " INVITE " + target->getNickname()
                 + " :" + channel->getName());                         // notifica all'invitato
}

/* =====================================================================================
**  MODE <#c> [<modi> [<parametri>...]]   oppure   MODE <nick> [...]
**  Canale -> handleChannelMode().
**  Nick   -> non gestiamo modi utente (non richiesti dal subject): rispondiamo solo quanto
**            basta perché irssi/HexChat non mostrino errori (dopo il login mandano
**            "MODE <proprio nick> +i").
**   401 nick/canale inesistente | 403 canale inesistente | 502 modi di un altro utente
** ===================================================================================== */
void Server::handleMode(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())                                            // manca il target
    {
        client->sendError("461", "MODE :Not enough parameters");       // ERR_NEEDMOREPARAMS
        return;
    }
    const std::string &target = msg.params[0];                         // canale o nick
    if (isChannelName(target))                                         // è un canale
    {
        Channel *channel = getChannel(target);                         // cercalo
        if (!channel)                                                  // non esiste
        {
            client->sendError("403", target + " :No such channel");    // ERR_NOSUCHCHANNEL
            return;
        }
        handleChannelMode(msg, client, channel);                       // logica dei modi di canale
        return;
    }
    Client *user = getClientByNick(target);                            // è un nick
    if (!user)                                                         // inesistente
        client->sendError("401", target + " :No such nick/channel");   // ERR_NOSUCHNICK
    else if (user != client)                                           // modi di un altro
        client->sendError("502", ":Cant change mode for other users"); // ERR_USERSDONTMATCH
    else if (msg.params.size() == 1)                                   // "MODE ilmionick": lettura
        client->sendReply("221", "+");                                 // RPL_UMODEIS: nessun modo attivo
    // "MODE ilmionick +i": modi utente non supportati, ignorato in silenzio
}

/* -------------------------------------------------------------------------------------
**  addAppliedMode()  [supporto]
**  Costruisce la stringa dei modi EFFETTIVAMENTE cambiati, da annunciare al canale,
**  aggiungendo il segno solo quando cambia: (+i)(+t)(-k) -> "+it-k".
** ------------------------------------------------------------------------------------- */
static void addAppliedMode(std::string &applied, char &lastSign, bool adding, char mode)
{
    char sign = adding ? '+' : '-';    // segno di questo modo
    if (sign != lastSign)              // diverso dal precedente?
    {
        applied += sign;               // scrivi il segno
        lastSign = sign;               // ricordalo
    }
    applied += mode;                   // scrivi la lettera
}

/* =====================================================================================
**  handleChannelMode()  —  MODE #c [<+/-modi> <parametri>]
**   "MODE #c"          -> 324 RPL_CHANNELMODEIS con i modi attuali (es. "+itk segreto")
**   "MODE #c b"        -> 368 lista ban vuota (irssi la chiede dopo il JOIN)
**   altrimenti servono: essere nel canale (442) ed essere operatore (482).
**  Le lettere si leggono da sinistra a destra; '+' e '-' cambiano il segno corrente.
**  I parametri si consumano nell'ordine delle lettere che li richiedono:
**     +k <chiave>  -k  +l <numero>  -l  +o <nick>  -o <nick>  +i -i +t -t
**  Esempio: "MODE #c +kl-i segreto 5" -> chiave "segreto", limite 5, toglie +i.
**  Errori per singola lettera (le altre vengono comunque applicate):
**     461 manca il parametro | 401 nick inesistente | 441 nick non nel canale
**     472 ERR_UNKNOWNMODE lettera sconosciuta
**  Alla fine i soli cambi effettivi sono annunciati a tutti:
**     ":op!user@host MODE #c +kl-i segreto 5"
** ===================================================================================== */
void Server::handleChannelMode(const IrcMessage &msg, Client *client, Channel *channel)
{
    const std::string &name = channel->getName();                      // nome del canale
    if (msg.params.size() == 1)                                        // nessun modo: lettura
    {
        client->sendReply("324", name + " " + channel->getModes());    // RPL_CHANNELMODEIS
        return;
    }
    const std::string &modeStr = msg.params[1];                        // es. "+kl-i"
    if ((modeStr == "b" || modeStr == "+b") && msg.params.size() == 2) // richiesta lista ban
    {
        client->sendReply("368", name + " :End of channel ban list");  // RPL_ENDOFBANLIST
        return;
    }
    if (!channel->hasClient(*client))                                  // non è nel canale
    {
        client->sendError("442", name + " :You're not on that channel"); // ERR_NOTONCHANNEL
        return;
    }
    if (!channel->isOperator(*client))                                 // non è operatore
    {
        client->sendError("482", name + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }

    bool adding = true;                                                // segno corrente (+ di default)
    size_t paramIdx = 2;                                               // primo parametro dopo i modi
    std::string applied;                                               // modi cambiati davvero
    std::string appliedParams;                                         // loro parametri
    char lastSign = 0;                                                 // ultimo segno scritto in applied

    for (std::string::size_type i = 0; i < modeStr.size(); ++i)        // ogni carattere
    {
        char mode = modeStr[i];                                        // lettera corrente
        if (mode == '+' || mode == '-')                                // cambio di segno
        {
            adding = (mode == '+');                                    // aggiorna il segno
            continue;
        }
        if (mode == 'i')                                               // invite-only
        {
            if (channel->isInviteOnly() != adding)                     // cambia davvero qualcosa?
            {
                channel->setInviteOnly(adding);                        // applica
                addAppliedMode(applied, lastSign, adding, 'i');        // da annunciare
            }
        }
        else if (mode == 't')                                          // topic solo per operatori
        {
            if (channel->isTopicRestricted() != adding)                // cambia davvero?
            {
                channel->setTopicRestricted(adding);                   // applica
                addAppliedMode(applied, lastSign, adding, 't');        // da annunciare
            }
        }
        else if (mode == 'k')                                          // chiave
        {
            if (adding)                                                // +k <chiave>
            {
                if (paramIdx >= msg.params.size())                     // manca la chiave
                {
                    client->sendError("461", "MODE :Not enough parameters"); // ERR_NEEDMOREPARAMS
                    continue;
                }
                std::string key = msg.params[paramIdx++];              // consuma il parametro
                channel->setKey(key);                                  // imposta la chiave
                addAppliedMode(applied, lastSign, true, 'k');          // da annunciare
                appliedParams += " " + key;                            // con il suo parametro
            }
            else                                                       // -k: NON consuma parametri
            {                                                          // (così "-k+l 5" assegna 5 a +l)
                if (channel->hasKey())                                 // c'era una chiave?
                {
                    channel->setKey("");                               // toglila
                    addAppliedMode(applied, lastSign, false, 'k');     // da annunciare
                }
            }
        }
        else if (mode == 'l')                                          // limite utenti
        {
            if (adding)                                                // +l <numero>
            {
                if (paramIdx >= msg.params.size())                     // manca il numero
                {
                    client->sendError("461", "MODE :Not enough parameters"); // ERR_NEEDMOREPARAMS
                    continue;
                }
                const std::string &arg = msg.params[paramIdx++];       // consuma il parametro
                char *end = NULL;                                      // strtol indica dove si è fermato
                long value = std::strtol(arg.c_str(), &end, 10);       // converte in base 10
                if (*end != '\0' || value <= 0 || value > 10000)       // non numerico o fuori range
                    continue;                                          // ignorato
                channel->setLimit(static_cast<int>(value));            // imposta il limite
                std::ostringstream oss;                                // per riconvertire in testo
                oss << value;                                          // numero "pulito" (es. "007" -> "7")
                addAppliedMode(applied, lastSign, true, 'l');          // da annunciare
                appliedParams += " " + oss.str();                      // con il suo parametro
            }
            else if (channel->getLimit() > 0)                          // -l e c'era un limite
            {
                channel->setLimit(0);                                  // toglilo
                addAppliedMode(applied, lastSign, false, 'l');         // da annunciare
            }
        }
        else if (mode == 'o')                                          // privilegi di operatore
        {
            if (paramIdx >= msg.params.size())                         // manca il nick
            {
                client->sendError("461", "MODE :Not enough parameters"); // ERR_NEEDMOREPARAMS
                continue;
            }
            const std::string &nick = msg.params[paramIdx++];          // consuma il parametro
            Client *target = getClientByNick(nick);                    // cerca l'utente
            if (!target)                                               // inesistente
            {
                client->sendError("401", nick + " :No such nick/channel"); // ERR_NOSUCHNICK
                continue;
            }
            if (!channel->hasClient(*target))                          // non è nel canale
            {
                client->sendError("441", nick + " " + name + " :They aren't on that channel"); // ERR_USERNOTINCHANNEL
                continue;
            }
            if (channel->isOperator(*target) == adding)                // nessun cambiamento
                continue;
            if (adding)                                                // +o
                channel->addOperator(*target);                         // promuovi
            else                                                       // -o
                channel->removeOperator(*target);                      // togli i privilegi
            addAppliedMode(applied, lastSign, adding, 'o');            // da annunciare
            appliedParams += " " + target->getNickname();              // con il nick
        }
        else                                                           // lettera non supportata
            client->sendError("472", std::string(1, mode) + " :is unknown mode char to me"); // ERR_UNKNOWNMODE
    }
    if (!applied.empty())                                              // qualcosa è cambiato?
        channel->broadcast(":" + client->getPrefix() + " MODE " + name + " " + applied + appliedParams); // annuncio
}

/* =====================================================================================
**  WHO <#c | nick>
**  HexChat lo manda dopo ogni JOIN per riempire la lista utenti. Per ogni utente:
**   352 RPL_WHOREPLY "<canale> <user> <host> <server> <nick> H[@] :0 <realname>"
**       (H = presente, @ = operatore, 0 = distanza in "hop" tra server)
**  e alla fine 315 RPL_ENDOFWHO.
** ===================================================================================== */
void Server::handleWho(const IrcMessage &msg, Client *client)
{
    std::string mask = msg.params.empty() ? "*" : msg.params[0];       // cosa cercare
    Channel *channel = isChannelName(mask) ? getChannel(mask) : NULL;  // è un canale esistente?
    if (channel)                                                       // sì: elenca i membri
    {
        std::vector<Client *> members = channel->getClients();         // membri
        for (size_t i = 0; i < members.size(); ++i)                    // ogni membro
        {
            std::string flags = channel->isOperator(*members[i]) ? "H@" : "H"; // stato
            client->sendReply("352", channel->getName() + " " + members[i]->getUsername() + " "
                + members[i]->getHostname() + " " SERVER_NAME " " + members[i]->getNickname()
                + " " + flags + " :0 " + members[i]->getRealname());   // RPL_WHOREPLY
        }
    }
    else if (!isChannelName(mask))                                     // forse è un nick
    {
        Client *user = getClientByNick(mask);                          // cercalo
        if (user)                                                      // esiste
            client->sendReply("352", "* " + user->getUsername() + " " + user->getHostname()
                + " " SERVER_NAME " " + user->getNickname() + " H :0 " + user->getRealname()); // RPL_WHOREPLY
    }
    client->sendReply("315", mask + " :End of WHO list");              // RPL_ENDOFWHO
}
