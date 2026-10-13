// PRIVMSG and NOTICE: same logic, but NOTICE never sends error replies
//
// Prima: solo PRIVMSG dentro Server/Server.cpp, con un solo destinatario, solo canali '#',
// errore 461 invece di 411/412, 401 invece di 403 per un canale inesistente e alcuni
// messaggi senza lo spazio prima di ':' (es. "bob:No such nick/channel"). NOTICE non c'era.
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcParser.hpp"

// "alice,#chan" -> ["alice", "#chan"]
static std::vector<std::string> splitTargets(const std::string &str)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= str.size())
    {
        size_t end = str.find(',', start);
        if (end == std::string::npos)
            end = str.size();
        if (end > start)
            parts.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}


void Server::handlePrivmsg(const IrcMessage &msg, Client *client)
{
    deliverMessage(msg, client, "PRIVMSG", true);
}


void Server::handleNotice(const IrcMessage &msg, Client *client)
{
    deliverMessage(msg, client, "NOTICE", false);
}


// <command> <target>[,<target>...] :<text>
void Server::deliverMessage(const IrcMessage &msg, Client *client, const std::string &command,
                            bool reportErrors)
{
    if (msg.params.empty())
    {
        if (reportErrors)
            client->sendError("411", ":No recipient given (" + command + ")");
        return;
    }

    // the text may also be a single word without ':'
    std::string text = msg.hasTrailing ? msg.trailing
                     : (msg.params.size() > 1 ? msg.params[1] : "");
    if (text.empty())
    {
        if (reportErrors)
            client->sendError("412", ":No text to send");
        return;
    }

    std::vector<std::string> targets = splitTargets(msg.params[0]);
    for (size_t i = 0; i < targets.size(); i++)
    {
        const std::string &target = targets[i];

        if (target[0] == '#' || target[0] == '&')
        {
            Channel *channel = getChannel(target);
            if (!channel)
            {
                if (reportErrors)
                    client->sendError("403", target + " :No such channel");
                continue;
            }

            if (!channel->hasClient(*client))
            {
                if (reportErrors)
                    client->sendError("404", target + " :Cannot send to channel");
                continue;
            }

            channel->broadcast(":" + client->getPrefix() + " " + command + " "
                               + channel->getName() + " :" + text, client);
        }
        else
        {
            Client *targetClient = getClientByNick(target);
            if (!targetClient)
            {
                if (reportErrors)
                    client->sendError("401", target + " :No such nick/channel");
                continue;
            }

            targetClient->send(":" + client->getPrefix() + " " + command + " "
                               + targetClient->getNickname() + " :" + text);
        }
    }
}
