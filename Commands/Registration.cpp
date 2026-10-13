// PASS, NICK, USER, CAP, PING, PONG, QUIT
//
// Prima questi handler erano dentro Server/Server.cpp (vecchia classe Server con
// std::vector<Client*>). Differenze rispetto ad allora:
//  - PASS: stampava la password nei log di debug, chiamava client->setPassword() (che il
//    nuovo Client non ha) e chiudeva senza mandare "ERROR :Closing Link";
//  - NICK: nick vuoto -> 461 invece di 431, nessuna validazione dei caratteri (432),
//    client->setNickname() diretto (ora updateNick() tiene allineato clientsByNick),
//    l'annuncio partiva una volta per ogni canale (doppioni) e non al client stesso;
//  - USER: senza hasTrailing non si capiva se il realname era stato mandato;
//  - CAP: era un if dentro start() e rispondeva ACK con lista vuota a REQ;
//  - PING/PONG non esistevano;
//  - QUIT: mandava lui il broadcast ai canali; ora lo fa Server::removeClient(),
//    cosi' vale anche per chi si disconnette senza QUIT.
// The registration check (451) is done once in Server::dispatchCommand().
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcParser.hpp"
#include <set>
#include <cctype>

// First char: letter or one of [ ] \ ` _ ^ { | }
// Other chars: letters, digits, '-' and the same specials. Max 30 (NICKLEN=30 in 005).
static bool isSpecialNickChar(char c)
{
    return std::string("[]\\`_^{|}").find(c) != std::string::npos;
}

static bool isValidNick(const std::string &nick)
{
    if (nick.empty() || nick.size() > 30)
        return false;
    if (!std::isalpha(static_cast<unsigned char>(nick[0])) && !isSpecialNickChar(nick[0]))
        return false;
    for (size_t i = 1; i < nick.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(nick[i]);
        if (!std::isalnum(c) && c != '-' && !isSpecialNickChar(nick[i]))
            return false;
    }
    return true;
}

// "PASS x" and "PASS :x" are both valid
static std::string firstArg(const IrcMessage &msg)
{
    return msg.params.empty() ? msg.trailing : msg.params[0];
}


void Server::handlePass(const IrcMessage &msg, Client *client)
{
    if (client->isRegistered())
    {
        client->sendError("462", ":You may not reregister");
        return;
    }

    std::string given = firstArg(msg);
    if (given.empty())
    {
        client->sendError("461", "PASS :Not enough parameters");
        return;
    }

    if (given != password)
    {
        client->sendError("464", ":Password incorrect");
        client->send("ERROR :Closing Link: " + client->getHostname() + " (Password incorrect)");
        client->markForDeletion("Password incorrect");
        return;
    }

    client->setAuth(true);
    client->tryRegister();
}


void Server::handleNick(const IrcMessage &msg, Client *client)
{
    std::string newNick = firstArg(msg);
    if (newNick.empty())
    {
        client->sendError("431", ":No nickname given");
        return;
    }

    if (!isValidNick(newNick))
    {
        client->sendError("432", newNick + " :Erroneous nickname");
        return;
    }

    if (newNick == client->getNickname())
        return;

    if (isNickTaken(newNick, client))
    {
        client->sendError("433", newNick + " :Nickname is already in use");
        return;
    }

    // Live nick change: tell the client and everyone sharing a channel, once each
    if (client->isRegistered())
    {
        std::string notice = ":" + client->getPrefix() + " NICK :" + newNick;
        std::set<Client *> recipients;
        recipients.insert(client);
        std::vector<Channel *> chans = client->getChannels();
        for (size_t i = 0; i < chans.size(); i++)
        {
            std::vector<Client *> members = chans[i]->getClients();
            recipients.insert(members.begin(), members.end());
        }
        for (std::set<Client *>::iterator it = recipients.begin(); it != recipients.end(); ++it)
            (*it)->send(notice);
    }

    // updateNick (not setNickname) keeps the clientsByNick index in sync
    updateNick(client, newNick);
    client->tryRegister();
}


// USER <username> <mode> <unused> :<realname>
void Server::handleUser(const IrcMessage &msg, Client *client)
{
    if (client->isRegistered())
    {
        client->sendError("462", ":You may not reregister");
        return;
    }

    if (msg.params.size() < 3 || (!msg.hasTrailing && msg.params.size() < 4))
    {
        client->sendError("461", "USER :Not enough parameters");
        return;
    }

    client->setUsername(msg.params[0]);
    client->setRealname(msg.hasTrailing ? msg.trailing : msg.params[3]);
    client->tryRegister();
}


// No IRCv3 capability is supported: LS -> empty list, REQ -> NAK, END -> nothing
void Server::handleCap(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())
        return;

    std::string target = client->getNickname().empty() ? "*" : client->getNickname();
    if (msg.params[0] == "LS")
        client->send(":" SERVER_NAME " CAP " + target + " LS :");
    else if (msg.params[0] == "REQ")
        client->send(":" SERVER_NAME " CAP " + target + " NAK :" + msg.trailing);
}


void Server::handlePing(const IrcMessage &msg, Client *client)
{
    std::string token = firstArg(msg);
    if (token.empty())
    {
        client->sendError("409", ":No origin specified");
        return;
    }
    client->send(":" SERVER_NAME " PONG " SERVER_NAME " :" + token);
}


// The server never sends PING, so PONG is ignored
void Server::handlePong(const IrcMessage &msg, Client *client)
{
    (void)msg;
    (void)client;
}


// The QUIT broadcast to the channels is sent by Server::removeClient(),
// so it also works when a client disconnects without sending QUIT.
void Server::handleQuit(const IrcMessage &msg, Client *client)
{
    std::string reason = msg.hasTrailing ? msg.trailing
                       : (msg.params.empty() ? "Client Quit" : msg.params[0]);
    client->send("ERROR :Closing Link: " + client->getHostname() + " (Quit: " + reason + ")");
    client->markForDeletion("Quit: " + reason);
}
