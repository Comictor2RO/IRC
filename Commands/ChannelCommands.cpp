// JOIN, PART, TOPIC, KICK, INVITE, MODE, WHO
// Operator-only (subject): KICK, INVITE (on +i channels), MODE changes, TOPIC on +t channels
//
// Prima questi handler erano dentro Server/Server.cpp. Differenze rispetto ad allora:
//  - ogni handler controllava da solo la registrazione (ora lo fa dispatchCommand());
//  - JOIN: un solo canale, 479 invece di 476, 443 se eri gia' dentro, controllo isBanned
//    (+b non e' richiesto dal subject), 366 mandato due volte e 331 anche senza topic;
//  - PART non esisteva;
//  - TOPIC: "TOPIC #c :" veniva trattato come lettura invece di cancellare il topic;
//  - KICK: nessun motivo di default e il canale vuoto non veniva cancellato;
//  - INVITE: nessun 443 se l'invitato era gia' nel canale;
//  - MODE: "MODE <proprio nick>" rispondeva 502; +l usava atoi senza controlli; -k
//    consumava un parametro; un errore su +o faceva return lasciando i cambi gia' fatti
//    senza annuncio; nessun 472 per lettere sconosciute; veniva annunciata la stringa
//    grezza anche quando qualche modo falliva;
//  - WHO: 461 senza parametri e host fisso "localhost".
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcParser.hpp"
#include <cstdlib>
#include <sstream>

// "#a,#b,#c" -> ["#a", "#b", "#c"]
static std::vector<std::string> splitComma(const std::string &str)
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

static bool isChannelName(const std::string &name)
{
    return !name.empty() && (name[0] == '#' || name[0] == '&');
}

// '#' or '&' + at least one char, max 50, no space, comma, BEL, \r, \n, \0 (RFC 2812)
static bool isValidChannelName(const std::string &name)
{
    static const std::string forbidden(" ,\a\r\n\0", 6);
    if (name.size() < 2 || name.size() > 50 || !isChannelName(name))
        return false;
    return name.find_first_of(forbidden) == std::string::npos;
}


// JOIN <#c1>[,<#c2>...] [<key1>[,<key2>...]]
void Server::handleJoin(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())
    {
        client->sendError("461", "JOIN :Not enough parameters");
        return;
    }

    std::vector<std::string> names = splitComma(msg.params[0]);
    std::vector<std::string> keys;
    if (msg.params.size() > 1)
        keys = splitComma(msg.params[1]);

    for (size_t i = 0; i < names.size(); i++)
    {
        const std::string &channelName = names[i];
        std::string key = (i < keys.size()) ? keys[i] : "";

        if (!isValidChannelName(channelName))
        {
            client->sendError("476", channelName + " :Bad Channel Mask");
            continue;
        }

        Channel *channel = getChannel(channelName);
        if (channel)
        {
            if (channel->hasClient(*client))
                continue; // already inside: ignored

            if (channel->isInviteOnly() && !channel->isInvited(*client))
            {
                client->sendError("473", channelName + " :Cannot join channel (+i)");
                continue;
            }

            if (channel->hasKey() && key != channel->getKey())
            {
                client->sendError("475", channelName + " :Cannot join channel (+k)");
                continue;
            }

            if (channel->isFull())
            {
                client->sendError("471", channelName + " :Cannot join channel (+l)");
                continue;
            }
        }
        else
            channel = createChannel(channelName); // the first member becomes operator

        channel->addClient(*client);
        channel->broadcast(":" + client->getPrefix() + " JOIN " + channel->getName());
        if (!channel->getTopic().empty())
            channel->sendTopic(*client);
        channel->sendNames(*client); // 353 + 366
    }
}


// PART <#c1>[,<#c2>...] [:<reason>]
void Server::handlePart(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())
    {
        client->sendError("461", "PART :Not enough parameters");
        return;
    }

    std::vector<std::string> names = splitComma(msg.params[0]);
    for (size_t i = 0; i < names.size(); i++)
    {
        Channel *channel = getChannel(names[i]);
        if (!channel)
        {
            client->sendError("403", names[i] + " :No such channel");
            continue;
        }

        if (!channel->hasClient(*client))
        {
            client->sendError("442", names[i] + " :You're not on that channel");
            continue;
        }

        std::string partMsg = ":" + client->getPrefix() + " PART " + channel->getName();
        if (msg.hasTrailing && !msg.trailing.empty())
            partMsg += " :" + msg.trailing;
        channel->broadcast(partMsg);
        channel->removeClient(*client);
        removeChannelIfEmpty(channel);
    }
}


// TOPIC <#c>            -> read (332 / 331)
// TOPIC <#c> :<topic>   -> set (empty = clear)
void Server::handleTopic(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())
    {
        client->sendError("461", "TOPIC :Not enough parameters");
        return;
    }

    Channel *channel = getChannel(msg.params[0]);
    if (!channel)
    {
        client->sendError("403", msg.params[0] + " :No such channel");
        return;
    }

    if (!channel->hasClient(*client))
    {
        client->sendError("442", channel->getName() + " :You're not on that channel");
        return;
    }

    if (!msg.hasTrailing && msg.params.size() < 2)
    {
        channel->sendTopic(*client);
        return;
    }

    if (channel->isTopicRestricted() && !channel->isOperator(*client))
    {
        client->sendError("482", channel->getName() + " :You're not channel operator");
        return;
    }

    std::string topic = msg.hasTrailing ? msg.trailing : msg.params[1];
    channel->setTopic(topic);
    channel->broadcast(":" + client->getPrefix() + " TOPIC " + channel->getName() + " :" + topic);
}


// KICK <#c> <nick> [:<reason>]
void Server::handleKick(const IrcMessage &msg, Client *client)
{
    if (msg.params.size() < 2)
    {
        client->sendError("461", "KICK :Not enough parameters");
        return;
    }

    Channel *channel = getChannel(msg.params[0]);
    if (!channel)
    {
        client->sendError("403", msg.params[0] + " :No such channel");
        return;
    }

    if (!channel->hasClient(*client))
    {
        client->sendError("442", channel->getName() + " :You're not on that channel");
        return;
    }

    if (!channel->isOperator(*client))
    {
        client->sendError("482", channel->getName() + " :You're not channel operator");
        return;
    }

    Client *targetClient = getClientByNick(msg.params[1]);
    if (!targetClient)
    {
        client->sendError("401", msg.params[1] + " :No such nick/channel");
        return;
    }

    if (!channel->hasClient(*targetClient))
    {
        client->sendError("441", targetClient->getNickname() + " " + channel->getName()
                          + " :They aren't on that channel");
        return;
    }

    // default reason: the kicker's nick
    std::string reason = (msg.hasTrailing && !msg.trailing.empty()) ? msg.trailing : client->getNickname();
    channel->broadcast(":" + client->getPrefix() + " KICK " + channel->getName() + " "
                       + targetClient->getNickname() + " :" + reason);
    channel->removeClient(*targetClient);
    removeChannelIfEmpty(channel); // an operator kicking himself as last member
}


// INVITE <nick> <#c>
// The channel may not exist (RFC 2812); if it exists you must be in it, and operator if +i.
void Server::handleInvite(const IrcMessage &msg, Client *client)
{
    if (msg.params.size() < 2)
    {
        client->sendError("461", "INVITE :Not enough parameters");
        return;
    }

    std::string nickname = msg.params[0];
    std::string channelName = msg.params[1];

    Client *targetClient = getClientByNick(nickname);
    if (!targetClient)
    {
        client->sendError("401", nickname + " :No such nick/channel");
        return;
    }

    Channel *channel = getChannel(channelName);
    if (channel)
    {
        channelName = channel->getName();

        if (!channel->hasClient(*client))
        {
            client->sendError("442", channelName + " :You're not on that channel");
            return;
        }

        if (channel->isInviteOnly() && !channel->isOperator(*client))
        {
            client->sendError("482", channelName + " :You're not channel operator");
            return;
        }

        if (channel->hasClient(*targetClient))
        {
            client->sendError("443", targetClient->getNickname() + " " + channelName
                              + " :is already on channel");
            return;
        }

        channel->addInvite(*targetClient);
    }

    client->sendReply("341", targetClient->getNickname() + " " + channelName);
    targetClient->send(":" + client->getPrefix() + " INVITE " + targetClient->getNickname()
                       + " :" + channelName);
}


// MODE <#c> [modes [params...]]  or  MODE <nick> [...]
// User modes are not required by the subject: we only answer enough for irssi/HexChat,
// which send "MODE <own nick> +i" right after login.
void Server::handleMode(const IrcMessage &msg, Client *client)
{
    if (msg.params.empty())
    {
        client->sendError("461", "MODE :Not enough parameters");
        return;
    }

    const std::string &target = msg.params[0];
    if (isChannelName(target))
    {
        Channel *channel = getChannel(target);
        if (!channel)
        {
            client->sendError("403", target + " :No such channel");
            return;
        }
        handleChannelMode(msg, client, channel);
        return;
    }

    Client *user = getClientByNick(target);
    if (!user)
        client->sendError("401", target + " :No such nick/channel");
    else if (user != client)
        client->sendError("502", ":Cant change mode for other users");
    else if (msg.params.size() == 1)
        client->sendReply("221", "+");
    // "MODE <own nick> +i": silently ignored
}


// Builds the string of the modes really changed, writing the sign only when it changes:
// (+i)(+t)(-k) -> "+it-k"
static void addAppliedMode(std::string &applied, char &lastSign, bool add, char mode)
{
    char sign = add ? '+' : '-';
    if (sign != lastSign)
    {
        applied += sign;
        lastSign = sign;
    }
    applied += mode;
}


// MODE #c [+/-itkol [params...]]
// Params are consumed in the order of the letters that need them: +k <key>, +l <n>, +/-o <nick>
// (-k and -l take no parameter). An error on one letter does not stop the others;
// at the end only the changes really applied are broadcast.
void Server::handleChannelMode(const IrcMessage &msg, Client *client, Channel *channel)
{
    const std::string &channelName = channel->getName();

    if (!channel->hasClient(*client))
    {
        client->sendError("442", channelName + " :You're not on that channel");
        return;
    }

    if (msg.params.size() == 1)
    {
        client->sendReply("324", channelName + " " + channel->getModes());
        return;
    }

    const std::string &modeStr = msg.params[1];

    // ban list request (irssi sends it after JOIN): +b is not supported, list is empty
    if ((modeStr == "b" || modeStr == "+b") && msg.params.size() == 2)
    {
        client->sendReply("368", channelName + " :End of channel ban list");
        return;
    }

    if (!channel->isOperator(*client))
    {
        client->sendError("482", channelName + " :You're not channel operator");
        return;
    }

    bool add = true;
    size_t paramIdx = 2;
    std::string applied;
    std::string appliedParams;
    char lastSign = 0;

    for (size_t i = 0; i < modeStr.length(); i++)
    {
        char c = modeStr[i];

        if (c == '+' || c == '-')
        {
            add = (c == '+');
            continue;
        }

        switch (c)
        {
            case 'i':
                if (channel->isInviteOnly() != add)
                {
                    channel->setInviteOnly(add);
                    addAppliedMode(applied, lastSign, add, 'i');
                }
                break;

            case 't':
                if (channel->isTopicRestricted() != add)
                {
                    channel->setTopicRestricted(add);
                    addAppliedMode(applied, lastSign, add, 't');
                }
                break;

            case 'k':
                if (add)
                {
                    if (paramIdx >= msg.params.size())
                    {
                        client->sendError("461", "MODE :Not enough parameters");
                        break;
                    }
                    std::string key = msg.params[paramIdx++];
                    channel->setKey(key);
                    addAppliedMode(applied, lastSign, true, 'k');
                    appliedParams += " " + key;
                }
                else if (channel->hasKey())
                {
                    channel->setKey("");
                    addAppliedMode(applied, lastSign, false, 'k');
                }
                break;

            case 'l':
                if (add)
                {
                    if (paramIdx >= msg.params.size())
                    {
                        client->sendError("461", "MODE :Not enough parameters");
                        break;
                    }
                    const std::string &arg = msg.params[paramIdx++];
                    char *end = NULL;
                    long limit = std::strtol(arg.c_str(), &end, 10);
                    if (*end != '\0' || limit <= 0 || limit > 10000)
                        break; // invalid limit: ignored
                    channel->setLimit(static_cast<int>(limit));
                    std::ostringstream oss;
                    oss << limit;
                    addAppliedMode(applied, lastSign, true, 'l');
                    appliedParams += " " + oss.str();
                }
                else if (channel->getLimit() > 0)
                {
                    channel->setLimit(0);
                    addAppliedMode(applied, lastSign, false, 'l');
                }
                break;

            case 'o':
            {
                if (paramIdx >= msg.params.size())
                {
                    client->sendError("461", "MODE :Not enough parameters");
                    break;
                }
                const std::string &nick = msg.params[paramIdx++];
                Client *target = getClientByNick(nick);
                if (!target)
                {
                    client->sendError("401", nick + " :No such nick/channel");
                    break;
                }
                if (!channel->hasClient(*target))
                {
                    client->sendError("441", nick + " " + channelName + " :They aren't on that channel");
                    break;
                }
                if (channel->isOperator(*target) == add)
                    break; // nothing changes
                if (add)
                    channel->addOperator(*target);
                else
                    channel->removeOperator(*target);
                addAppliedMode(applied, lastSign, add, 'o');
                appliedParams += " " + target->getNickname();
                break;
            }

            default:
                client->sendError("472", std::string(1, c) + " :is unknown mode char to me");
                break;
        }
    }

    if (!applied.empty())
        channel->broadcast(":" + client->getPrefix() + " MODE " + channelName + " " + applied + appliedParams);
}


// WHO <#c | nick>: 352 for each user, then 315
// 352 format: "<channel> <user> <host> <server> <nick> H[@] :0 <realname>"
void Server::handleWho(const IrcMessage &msg, Client *client)
{
    std::string mask = msg.params.empty() ? "*" : msg.params[0];

    if (isChannelName(mask))
    {
        Channel *channel = getChannel(mask);
        if (channel)
        {
            std::vector<Client *> members = channel->getClients();
            for (size_t i = 0; i < members.size(); ++i)
            {
                std::string status = channel->isOperator(*members[i]) ? "H@" : "H";
                client->sendReply("352", channel->getName() + " " + members[i]->getUsername() + " "
                    + members[i]->getHostname() + " " SERVER_NAME " " + members[i]->getNickname()
                    + " " + status + " :0 " + members[i]->getRealname());
            }
        }
    }
    else
    {
        Client *user = getClientByNick(mask);
        if (user)
            client->sendReply("352", "* " + user->getUsername() + " " + user->getHostname()
                + " " SERVER_NAME " " + user->getNickname() + " H :0 " + user->getRealname());
    }

    client->sendReply("315", mask + " :End of WHO list");
}
