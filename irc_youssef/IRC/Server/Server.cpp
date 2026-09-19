#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#include <signal.h>

static volatile sig_atomic_t stopRequested = 0;

static void handleSignal(int signalNumber)
{
    if (signalNumber == SIGINT)
        stopRequested = 1;
}

Server::Server(int port, std::string pass)
    : port(port), pass(pass), server_fd(-1), running(false)
{}


void Server::start()
{
    signal(SIGINT, handleSignal);
    stopRequested = 0;

    server_fd = socket(PF_INET, SOCK_STREAM, 0);
    
    if(server_fd < 0)
    {
        std::cout << "Socket creation failed.\n";
        return;
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cout << "Error: setsockopt failed.\n";
        close(server_fd);
        return;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if(bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        std::cout << "Errror: bind failed.\n";
        close(server_fd);
        return;
    }

    if(listen(server_fd, 10) < 0)
    {
        std::cout << "Error: listen failed.\n";
        close(server_fd);
        return;
    }

    std::cout << "Server listening on port: " << port << '\n';

    std::vector<pollfd> fds;
    pollfd pfd = {};
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);

    running = true;
    while(running && !stopRequested)
    {
        int ret = poll(&fds[0], fds.size(), -1);
        if(ret < 0)
        {
            if (errno == EINTR && stopRequested)
                break;
            std::cout << "Error: poll failed.\n";
            break;
        }

        if(fds[0].revents & POLLIN)
        {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
            
            if(client_fd < 0)
            {
                std::cout << "Error: accept failed.\n";
                continue;
            }

            int flag = fcntl(client_fd, F_GETFL, 0);
            fcntl(client_fd, F_SETFL, flag | O_NONBLOCK);

            pollfd new_client = {};
            new_client.fd = client_fd;
            new_client.events = POLLIN;
            fds.push_back(new_client);

            Client *newClient = new Client(client_fd);
            clients.push_back(newClient);

            std::cout << "New client connected: " << client_fd << '\n';
        }

        for(size_t i = 1; i < fds.size(); ++i)
        {
            if(fds[i].revents & POLLIN)
            {
                char buffer[1024];
                int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

                std::cout << "=== RECV ===" << std::endl;
                std::cout << "bytes = " << bytes << std::endl;

                if(bytes <= 0)
                {
                    std::cout << "Client disconnected: " << fds[i].fd << '\n';
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                buffer[bytes] = 0;
                std::cout << "Received: [" << std::string(buffer, bytes) << "]" << std::endl;
                std::string rawData(buffer, bytes);

                Client *client = getClientByFd(fds[i].fd);
                std::cout << "DEBUG: getClientByFd returned: " << (client ? "OK" : "NULL") << std::endl;
                if(!client)
                {
                    std::cout << "DEBUG: Client not found!" << std::endl;
                    continue;
                }

                client->appendToBuffer(rawData);

                std::cout << "DEBUG: Buffer content: [" << client->getBuffer() << "]" << std::endl;
                std::cout << "DEBUG: Buffer size: " << client->getBuffer().size() << std::endl;

                std::string &buf = const_cast<std::string&>(client->getBuffer());

                std::cout << "DEBUG: Buffer size: " << buf.size() << std::endl;
                std::cout << "DEBUG: Buffer content: [" << buf << "]" << std::endl;
                std::cout << "DEBUG: Looking for newline..." << std::endl;

                size_t pos;

                while((pos = buf.find("\r\n")) != std::string::npos || (pos = buf.find("\n")) != std::string::npos)
                {
                    std::string line = buf.substr(0, pos);
                    if (!line.empty() && line[line.length()-1] == '\r')
                        line.erase(line.length()-1);
                    buf.erase(0, pos + (buf[pos+1] == '\n' ? 2 : 1));

                    if (line.empty())
                        continue;
                    
                    IrcMessage msg = IrcParser::parse(line);

                    std::cout << "DEBUG: Raw line: [" << line << "]" << std::endl;
                    std::cout << "DEBUG: Command: [" << msg.command << "]" << std::endl;
                    std::cout << "DEBUG: Params size: " << msg.params.size() << std::endl;

                    if (msg.command == "PASS")
                    {
                        std::cout << "CALLING PASS" << std::endl;
                        handlePass(msg, client);
                    }
                    else if (msg.command == "NICK")
                        handleNick(msg, client);
                    else if (msg.command == "USER")
                        handleUser(msg, client);
                    else if (msg.command == "JOIN")
                        handleJoin(msg, client);
                    else if (msg.command == "WHO")
                        handleWho(msg, client);
                    else if (msg.command == "PRIVMSG")
                        handlePrivmsg(msg, client);
                    else if (msg.command == "KICK")
                        handleKick(msg, client);
                    else if (msg.command == "INVITE")
                        handleInvite(msg, client);
                    else if (msg.command == "TOPIC")
                        handleTopic(msg, client);
                    else if (msg.command == "MODE")
                        handleMode(msg, client);
                    else if (msg.command == "QUIT")
                        handleQuit(msg, client);
                    else if (msg.command == "CAP")
                    {
                        if (!msg.params.empty() && msg.params[0] == "LS")
                            client->send(":localhost CAP * LS :");
                        else if (!msg.params.empty() && msg.params[0] == "REQ")
                            client->send(":localhost CAP * ACK :");
                    }
                    else if (!msg.command.empty())
                        client->sendError("421", msg.command + " :Unknown command");
                }
            }
        }

        for (size_t i = 1; i < fds.size(); i++) {
            Client* client = getClientByFd(fds[i].fd);
            if (client && client->shouldDelete()) {
                close(fds[i].fd);
                fds.erase(fds.begin() + i);
                removeClient(client);
                i--;
            }
        }
    }
    close(server_fd);
}


void Server::stop()
{
    running = false;
    std::cout << "Server stopping...\n";
}


Client *Server::getClientByFd(int fd)
{
    for(size_t i = 0; i < clients.size(); i++)
    {
        if(clients[i]->getFD() == fd)
            return clients[i];
    }
    return NULL;
}


Client *Server::getClientByNick(const std::string &nick)
{
    for(size_t i = 0; i < clients.size(); i++)
    {
        if(clients[i]->getNickname() == nick)
            return clients[i];
    }
    return NULL;
}


void Server::removeClient(Client *client)
{
    for(size_t i = 0; i < clients.size(); i++)
    {
        if(clients[i] == client)
        {
            clients.erase(clients.begin() + i);
            break;
        }
    }

    std::vector<Channel*> clientChannels = client->getChannels();
    for (size_t i = 0; i < clientChannels.size(); i++)
    {
        clientChannels[i]->removeClient(*client);
        
        if (clientChannels[i]->getClientCount() == 0)
        {
            for (size_t j = 0; j < channels.size(); j++)
            {
                if (channels[j] == clientChannels[i])
                {
                    delete channels[j];
                    channels.erase(channels.begin() + j);
                    break;
                }
            }
        }
    }
    
    delete client;
}


Channel* Server::getChannel(const std::string& name)
{
    for(size_t i = 0; i < channels.size(); ++i)
    {
        if(channels[i]->getName() == name)
            return channels[i];
    }
    return NULL;
}


Channel* Server::createChannel(const std::string& name)
{
    Channel *existing = getChannel(name);
    if(existing)
        return existing;

    Channel *channel = new Channel(name);
    channels.push_back(channel);
    return channel;
}


bool Server::isNickTaken(const std::string& nick, Client* exclude) const
{
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == exclude)
            continue;
        
        if (clients[i]->getNickname() == nick)
            return true;
    }
    return false;
}


void Server::handlePass(const IrcMessage &msg, Client *client) {
    std::cout << "=== handlePass CALLED ===" << std::endl;
    
    if(client->isRegistered()) {
        std::cout << "DEBUG: Already registered" << std::endl;
        client->sendError("462", ":You may not reregister");
        return;
    }

    if(msg.params.empty()) {
        std::cout << "DEBUG: PASS params empty" << std::endl;
        client->sendError("461", "PASS :Not enough parameters");
        return;
    }

    std::cout << "DEBUG: Received password: " << msg.params[0] << std::endl;
    std::cout << "DEBUG: Server password: " << pass << std::endl;

    if(msg.params[0] != pass) {
        std::cout << "DEBUG: Password mismatch" << std::endl;
        client->sendError("464", ":Password incorrect");
        client->markForDeletion();
        return;
    }

    std::cout << "DEBUG: Password OK, setting auth" << std::endl;
    client->setPassword(msg.params[0]);
    client->setAuth(true);
    client->tryRegister();
    
    std::cout << "DEBUG: handlePass DONE" << std::endl;
}


void Server::handleNick(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleNick CALLED ===" << std::endl;
    
    if(msg.params.empty())
    {
        client->sendError("461", "NICK :Not enough parameters");
        return;
    }

    std::string newNick = msg.params[0];

    if (newNick == client->getNickname())
        return;
    
    if(isNickTaken(newNick, client))
    {
        client->sendError("433", newNick + " :Nickname is already in use");
        return;
    }

    std::string oldNick = client->getNickname();
    client->setNickname(newNick);

    if(!oldNick.empty())
    {
        std::vector<Channel *> clientChannel = client->getChannels();
        for(size_t i = 0; i < clientChannel.size(); i++)
        {
            clientChannel[i]->broadcast(":" + oldNick + " NICK :" + newNick);
        }
    }

    client->tryRegister();
}


void Server::handleUser(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleUser CALLED ===" << std::endl;
    
    if(client->isRegistered())
    {
        client->sendError("462", ":You may not reregister");
        return;
    }

    if(msg.params.size() < 3)
    {
        client->sendError("461", "USER :Not enough parameters");
        return;
    }

    client->setUsername(msg.params[0]);
    
    std::string realname = msg.trailing;
    if (realname.empty() && msg.params.size() >= 4)
        realname = msg.params[3];
    
    client->setRealname(realname);
    std::cout << "DEBUG: Username=" << client->getUsername() << ", Realname=" << client->getRealname() << std::endl;

    client->tryRegister();
}


void Server::handleJoin(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleJoin CALLED ===" << std::endl;
    
    if(!client->isRegistered())
    {
        std::cout << "DEBUG: Client not registered" << std::endl;
        client->sendError("451", ":You have to register");
        return;
    }

    if(msg.params.empty())
    {
        std::cout << "DEBUG: JOIN params empty" << std::endl;
        client->sendError("461", "JOIN :Not enough parameters");
        return;
    }

    std::string channelName = msg.params[0];
    if (channelName.size() < 2 || (channelName[0] != '#' && channelName[0] != '&'))
    {
        std::cout << "DEBUG: Invalid channel name" << std::endl;
        client->sendError("479", channelName + " :Illegal channel name");
        return;
    }

    std::string key = (msg.params.size() > 1) ? msg.params[1] : "";
    
    std::cout << "DEBUG: Channel name: " << channelName << std::endl;
    std::cout << "DEBUG: Channel key: " << key << std::endl;

    Channel *channel = getChannel(channelName);
    if(!channel)
    {
        std::cout << "DEBUG: Creating new channel: " << channelName << std::endl;
        channel = createChannel(channelName);
    }
    else
    {
        std::cout << "DEBUG: Channel exists: " << channelName << std::endl;
    }

    if(channel->hasClient(*client))
    {
        std::cout << "DEBUG: Client already in channel" << std::endl;
        client->sendError("443", client->getNickname() + " " + channelName + ":is already on channel");
        return;
    }

    if(channel->isInviteOnly() && !channel->isInvited(*client))
    {
        std::cout << "DEBUG: Channel is invite-only" << std::endl;
        client->sendError("473", channelName + " :Cannot join channel (+i)");
        return;
    }

    if(channel->hasKey() && key != channel->getKey())
    {
        std::cout << "DEBUG: Wrong channel key" << std::endl;
        client->sendError("475", channelName + " :Cannot join channel (+k)");
        return;
    }

    if(channel->isFull())
    {
        std::cout << "DEBUG: Channel is full" << std::endl;
        client->sendError("471", channelName + " :Cannot join channel (+l)");
        return;
    }

    if(channel->isBanned(*client))
    {
        std::cout << "DEBUG: Client is banned" << std::endl;
        client->sendError("474", channelName + " :Cannot join channel (+b)");
        return;
    }

    std::cout << "DEBUG: Adding client to channel" << std::endl;
    channel->addClient(*client);
    channel->broadcast(":" + client->getPrefix() + " JOIN " + channelName);

    channel->sendTopic(*client);
    channel->sendNames(*client);
    client->sendReply("366", channelName + " :End of NAMES list");
    
    std::cout << "DEBUG: handleJoin DONE" << std::endl;
}


void Server::handleWho(const IrcMessage &msg, Client *client)
{
    if (!client->isRegistered())
    {
        client->sendError("451", ":You have to register");
        return;
    }

    if (msg.params.empty())
    {
        client->sendError("461", "WHO :Not enough parameters");
        return;
    }

    std::string target = msg.params[0];
    Channel *channel = getChannel(target);
    if (channel)
    {
        std::vector<Client *> members = channel->getClients();
        for (size_t i = 0; i < members.size(); ++i)
        {
            std::string status = channel->isOperator(*members[i]) ? "H@" : "H";
            client->sendReply("352", target + " " + members[i]->getUsername() +
                " localhost localhost " + members[i]->getNickname() + " " + status +
                " :0 " + members[i]->getRealname());
        }
    }

    client->sendReply("315", target + " :End of WHO list");
}


void Server::handlePrivmsg(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handlePrivmsg CALLED ===" << std::endl;
    
    if(!client->isRegistered())
    {
        std::cout << "DEBUG: Client not registered" << std::endl;
        client->sendError("451", ":You have not registered");
        return;
    }

    if(msg.params.empty() || msg.trailing.empty())
    {
        std::cout << "DEBUG: PRIVMSG params or trailing empty" << std::endl;
        client->sendError("461", "PRIVMSG :Not enough parametes");
        return;
    }

    std::string target = msg.params[0];
    std::string message = msg.trailing;
    
    std::cout << "DEBUG: Target: " << target << std::endl;
    std::cout << "DEBUG: Message: " << message << std::endl;

    if(target[0] == '#')
    {
        Channel *channel = getChannel(target);
        if(!channel)
        {
            std::cout << "DEBUG: Channel not found" << std::endl;
            client->sendError("401", target + " :No such nick/channel");
            return;
        }

        if(!channel->hasClient(*client))
        {
            std::cout << "DEBUG: Client not in channel" << std::endl;
            client->sendError("404", target + " :Cannot send to the channel");
            return;
        }

        channel->broadcast(":" + client->getPrefix() + " PRIVMSG " + target + " :" + message, client);
    }
    else
    {
        Client *targetClient = getClientByNick(target);
        if(!targetClient)
        {
            std::cout << "DEBUG: Target client not found" << std::endl;
            client->sendError("401", target + ":No such nick/channel");
            return;
        }

        targetClient->send(":" + client->getPrefix() + " PRIVMSG " + target + " :" + message);
    }
}


void Server::handleKick(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleKick CALLED ===" << std::endl;
    
    if (!client->isRegistered()) {
        std::cout << "DEBUG: Client not registered" << std::endl;
        client->sendError("451", ":You have not registered");
        return;
    }
    
    if (msg.params.size() < 2) {
        std::cout << "DEBUG: KICK params < 2" << std::endl;
        client->sendError("461", "KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = msg.params[0];
    std::string userNick = msg.params[1];
    std::string reason = msg.trailing;
    
    std::cout << "DEBUG: Channel: " << channelName << std::endl;
    std::cout << "DEBUG: User to kick: " << userNick << std::endl;
    std::cout << "DEBUG: Reason: " << reason << std::endl;
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        std::cout << "DEBUG: Channel not found" << std::endl;
        client->sendError("403", channelName + ":No such channel");
        return;
    }
    
    if (!channel->hasClient(*client)) {
        std::cout << "DEBUG: Client not in channel" << std::endl;
        client->sendError("442", channelName + ":You're not on that channel");
        return;
    }
    
    if (!channel->isOperator(*client)) {
        std::cout << "DEBUG: Client is not operator" << std::endl;
        client->sendError("482", channelName + ":You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNick(userNick);
    if (!targetClient) {
        std::cout << "DEBUG: Target client not found" << std::endl;
        client->sendError("401", userNick + ":No such nick/channel");
        return;
    }
    
    if (!channel->hasClient(*targetClient)) {
        std::cout << "DEBUG: Target not in channel" << std::endl;
        client->sendError("441", userNick + " " + channelName + ":They aren't on that channel");
        return;
    }
    
    std::string kickMsg = ":" + client->getPrefix() + " KICK " + channelName + " " + userNick;
    if (!reason.empty())
        kickMsg += " :" + reason;
    
    std::cout << "DEBUG: Kicking client" << std::endl;
    channel->broadcast(kickMsg);
    channel->removeClient(*targetClient);
}


void Server::handleInvite(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleInvite CALLED ===" << std::endl;
    
    if(!client->isRegistered())
    {
        std::cout << "DEBUG: Client not registered" << std::endl;
        client->sendError("451", ":You have not registered");
        return;
    }

    if(msg.params.size() < 2)
    {
        std::cout << "DEBUG: INVITE params < 2" << std::endl;
        client->sendError("461", "INVITE :Not enough parameters");
        return;
    }

    std::string nickname = msg.params[0];
    std::string channelName = msg.params[1];
    
    std::cout << "DEBUG: Nickname: " << nickname << std::endl;
    std::cout << "DEBUG: Channel: " << channelName << std::endl;

    Channel *channel = getChannel(channelName);

    if(channel)
    {
        if(!channel->hasClient(*client))
        {
            std::cout << "DEBUG: Client not in channel" << std::endl;
            client->sendError("442", channelName + " :You are not on that channel");
            return;
        }

        if(channel->isInviteOnly() && !channel->isOperator(*client))
        {
            std::cout << "DEBUG: Client is not operator" << std::endl;
            client->sendError("482", channelName + " :You are not channel operator");
            return;
        }
    }

    Client *targetClient = getClientByNick(nickname);
    if(!targetClient)
    {
        std::cout << "DEBUG: Target client not found" << std::endl;
        client->sendError("401", nickname + " :No such nick/channel");
        return;
    }

    if(channel)
        channel->addInvite(*targetClient);

    std::cout << "DEBUG: Sending invite" << std::endl;
    client->sendReply("341", nickname + " " + channelName);
    targetClient->send(":" + client->getPrefix() + " INVITE " + nickname + " " + channelName);
}


void Server::handleTopic(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleTopic CALLED ===" << std::endl;
    
    if(!client->isRegistered())
    {
        std::cout << "DEBUG: Client not registered" << std::endl;
        client->sendError("451", ":You have not registered");
        return;
    }

    if(msg.params.empty())
    {
        std::cout << "DEBUG: TOPIC params empty" << std::endl;
        client->sendError("461", "TOPIC :Not enough parameters");
        return;
    }

    std::string channelName = msg.params[0];
    std::cout << "DEBUG: Channel name: " << channelName << std::endl;

    Channel *channel = getChannel(channelName);
    if(!channel)
    {
        std::cout << "DEBUG: Channel not found" << std::endl;
        client->sendError("403", channelName + " :No such channel");
        return;
    }

    if(!channel->hasClient(*client))
    {
        std::cout << "DEBUG: Client not in channel" << std::endl;
        client->sendError("442", channelName + " :You are not in that channel");
        return;
    }

    if(msg.trailing.empty() && msg.params.size() == 1)
    {
        std::cout << "DEBUG: Sending topic" << std::endl;
        channel->sendTopic(*client);
    }
    else
    {
        if(channel->isTopicRestricted() && !channel->isOperator(*client))
        {
            std::cout << "DEBUG: Client cannot set topic" << std::endl;
            client->sendError("482", channelName + " :You are not channel operator");
            return;
        }

        std::cout << "DEBUG: Setting topic: " << msg.trailing << std::endl;
        channel->setTopic(msg.trailing);
        channel->broadcast(":" + client->getPrefix() + " TOPIC " + channelName + " :" + msg.trailing);
    }
}


void Server::handleMode(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleMode CALLED ===" << std::endl;
    
    if (!client->isRegistered())
    {
        std::cout << "DEBUG: Client not registered" << std::endl;
        client->sendError("451", ":You have not registered");
        return;
    }
    
    if (msg.params.empty())
    {
        std::cout << "DEBUG: MODE params empty" << std::endl;
        client->sendError("461", "MODE :Not enough parameters");
        return;
    }
    
    std::string target = msg.params[0];
    std::cout << "DEBUG: Target: " << target << std::endl;
    
    if (target[0] == '#')
    {
        handleChannelMode(msg, client, target);
    }
    else
    {
        client->sendError("502", ":Cant change mode for other users");
    }
}


void Server::handleChannelMode(const IrcMessage &msg, Client *client, const std::string &channelName)
{
    std::cout << "=== handleChannelMode CALLED ===" << std::endl;
    
    Channel* channel = getChannel(channelName);
    if (!channel)
    {
        std::cout << "DEBUG: Channel not found" << std::endl;
        client->sendError("403", channelName + ":No such channel");
        return;
    }
    
    if (!channel->hasClient(*client))
    {
        std::cout << "DEBUG: Client not in channel" << std::endl;
        client->sendError("442", channelName + ":You're not on that channel");
        return;
    }
    
    if (msg.params.size() == 1)
    {
        std::cout << "DEBUG: Sending channel modes" << std::endl;
        client->sendReply("324", channelName + " +" + channel->getModes());
        return;
    }
    
    if (!channel->isOperator(*client))
    {
        std::cout << "DEBUG: Client is not operator" << std::endl;
        client->sendError("482", channelName + ":You're not channel operator");
        return;
    }
    
    std::string modeStr = msg.params[1];
    bool add = true;
    size_t paramIdx = 2;
    
    std::cout << "DEBUG: Mode string: " << modeStr << std::endl;
    
    for (size_t i = 0; i < modeStr.length(); i++)
    {
        char c = modeStr[i];
        
        if (c == '+') { add = true; continue; }
        if (c == '-') { add = false; continue; }
        
        std::cout << "DEBUG: Processing mode: " << c << " (add=" << add << ")" << std::endl;
        
        switch (c)
        {
            case 'i':
                channel->setInviteOnly(add);
                break;
            
            case 't':
                channel->setTopicRestricted(add);
                break;
            
            case 'k':
                if (add && paramIdx < msg.params.size())
                    channel->setKey(msg.params[paramIdx++]);
                else
                    channel->setKey("");
                break;
            
            case 'l':
                if (add && paramIdx < msg.params.size())
                    channel->setLimit(atoi(msg.params[paramIdx++].c_str()));
                else
                    channel->setLimit(0);
                break;
            
            case 'o':
                if (paramIdx < msg.params.size())
                {
                    Client* target = getClientByNick(msg.params[paramIdx++]);
                    if (!target || !channel->hasClient(*target))
                    {
                        std::string targetName = target ? target->getNickname() : "unknown";
                        client->sendError("441", targetName + " " + channelName + " :They aren't on that channel");
                        return;
                    }
                    if (add)
                    {
                        std::cout << "DEBUG: Adding operator: " << target->getNickname() << std::endl;
                        channel->addOperator(*target);
                    }
                    else
                    {
                        std::cout << "DEBUG: Removing operator: " << target->getNickname() << std::endl;
                        channel->removeOperator(*target);
                    }
                }
                break;
        }
    }
    
    std::string modeMsg = ":" + client->getPrefix() + " MODE " + channelName + " " + modeStr;
    for (size_t i = 2; i < msg.params.size(); i++)
        modeMsg += " " + msg.params[i];
    
    std::cout << "DEBUG: Broadcasting mode: " << modeMsg << std::endl;
    channel->broadcast(modeMsg);
}

void Server::handleQuit(const IrcMessage &msg, Client *client)
{
    std::cout << "=== handleQuit CALLED ===" << std::endl;
    std::cout << "DEBUG: Reason: " << msg.trailing << std::endl;
    
    std::vector<Channel *> channels = client->getChannels();
    for (size_t i = 0; i < channels.size(); i++)
    {
        channels[i]->broadcast(":" + client->getPrefix() + " QUIT :" + msg.trailing, client);
    }
    
    client->markForDeletion();
}


Server::~Server()
{
    while(!clients.empty())
        removeClient(clients.back());

    for(size_t i = 0; i < channels.size(); ++i)
        delete channels[i];

    channels.clear();
}