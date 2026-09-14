#include "Server.hpp"
#include <sys/socket.h> //socketaddr, bind, connect, socket, accept, listen, send, recv, setsockopt, SO_REUSEADDR
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>

Server::Server(int port, std::string pass)
    : port(port), pass(pass), server_fd(-1), running(false)
{}

void Server::start()
{
    // Creating socket
    server_fd = socket(PF_INET, SOCK_STREAM, 0);
    
    if(server_fd < 0) //If it fails
    {
        std::cout << "Socket creation failed.\n";
        return;
    }

    // Set SO_REUSEADDR
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cout << "Error: setsockopt failed.\n";
        close(server_fd);
        return;
    }

    // Bind
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

    // Listen
    if(listen(server_fd, 10) < 0)
    {
        std::cout << "Error: listen failed.\n";
        close(server_fd);
        return;
    }

    std::cout << "Server listening on port: " << port << '\n';

    // Setup
    std::vector<pollfd> fds;
    pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);

    // Main Loop
    running = true;
    while(running)
    {
        // Waiting for events
        int ret = poll(&fds[0], fds.size(), -1);
        if(ret < 0)
        {
            std::cout << "Error: poll failed.\n";
            break;
        }

        // Checking for connections
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


            // Set non-blocking
            int flag = fcntl(client_fd, F_GETFL, 0);
            fcntl(client_fd, F_SETFL, flag | O_NONBLOCK);

            pollfd new_client;
            new_client.fd = client_fd;
            new_client.events = POLLIN;
            fds.push_back(new_client);

            std::cout << "New client connected: " << client_fd << '\n';
        }

        //Check clients
        for(size_t i = 1; i < fds.size(); ++i)
        {
            if(fds[i].revents & POLLIN)
            {
                char buffer[1024];
                int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

                if(bytes <= 0)
                {
                    std::cout << "Client disconnected: " << fds[i].fd << '\n';
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                buffer[bytes] = 0;
                std::string rawData(buffer, bytes);

                // Founds the client
                Client *client = getClientByFd(fds[i].fd);
                if(!client)
                    continue;

                client->appendToBuffer(rawData);

                std::string &buf = const_cast<std::string&>(client->getBuffer());
                size_t pos;

                while((pos = buf.find("\r\n")) != std::string::npos)
                {
                    std::string line = buf.substr(0, pos);
                    buf.erase(0, pos + 2);
                    
                    IrcMessage msg = IrcParser::parse(line);

                    if (msg.command == "PASS")
                        handlePass(msg, client);
                    else if (msg.command == "NICK")
                        handleNick(msg, client);
                    else if (msg.command == "USER")
                        handleUser(msg, client);
                    else if (msg.command == "JOIN")
                        handleJoin(msg, client);
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

Client *Server::getClientByNick(std::string &nick)
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

    // TO DO: Remove from all channels


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

bool Server::isNickTaken(const std::string& nick, Client* exclude = NULL) const
{
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == exclude)
            continue;
        
        if (clients[i]->getNickname() == nick)
            return true;
    }
    return false;
}

void Server::handlePass(const IrcMessage &msg,Client *client)
{
    if(client->isRegistered())
    {
        client->sendError("462", ":You may not reregister");
        return;
    }

    if(msg.params.empty())
    {
        client->sendError("461", "PASS :Not enough parameters");
        return;
    }

    if(msg.params[0] != pass)
    {
        client->sendError("464", ":Password incorrect");
        client->markForDeletion();
        return;
    }

    client->setPassword(msg.params[0]);
    client->setAuth(true);
    client->tryRegister();
}

void Server::handleNick(const IrcMessage &msg,Client *client)
{
    if(msg.params.empty())
    {
        client->sendError("461", "NICK :Not enough parameters");
        return;
    }

    std::string newNick = msg.params[0];
    
    if(isNickTaken(newNick, client))
    {
        client->sendError("433", newNick + ":Nickname is already used");
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

void Server::handleUser(const IrcMessage &msg,Client *client)
{
    if(client->isRegistered())
    {
        client->sendError("462", ":You may not reregister");
        return;
    }

    if(msg.params.size() < 4)
    {
        client->sendError("461", "USER :Not enough parameters");
        return;
    }

    client->setUsername(msg.params[0]);
    client->setRealname(msg.trailing);

    client->tryRegister();
}
void Server::handleJoin(const IrcMessage &msg,Client *client)
{
    if(!client->isRegistered())
    {
        client->sendError("451", ":You have to register");
        return;
    }

    if(msg.params.empty())
    {
        client->sendError("461", "JOIN :Not enough parameters");
        return;
    }

    std::string channelName = msg.params[0];
    std::string key = (msg.params.size() > 1) ? msg.params[1] : "";

    Channel *channel = getChannel(channelName);
    if(!channel)
    {
        channel = createChannel(channelName);
    }

    if(channel->hasClient(*client))
    {
        client->sendError("443", client->getNickname() + " " + channelName + ":is already on channel");
        return;
    }

    if(channel->isInviteOnly() && !channel->isInvited(*client))
    {
        client->sendError("473", channelName + " :Cannot join channel (+i)");
        return;
    }

    if(channel->hasKey() && key != channel->getKey())
    {
        client->sendError("475", channelName + " :Cannot join channel (+k)");
        return;
    }

    if(channel->isFull())
    {
        client->sendError("471", channelName + " :Cannot join channel (+l)");
        return;
    }

    if(channel->isBanned(*client))
    {
        client->sendError("474", channelName + " :Cannot join channel (+b)");
        return;
    }

    channel->addClient(*client);
    channel->sendTopic(*client);
    channel->sendNames(*client);
    client->sendReply("366", channelName + " :End of NAMES list");
    channel->broadcast(":" + client->getPrefix() + " JOIN " + channelName, client);
}

void Server::handlePrivmsg(const IrcMessage &msg,Client *client)
{
    if(!client->isRegistered())
    {
        client->sendError("451", ":You have not registered");
        return;
    }

    if(msg.params.empty() && msg.trailing.empty())
    {
        client->sendError("461", "PRIVMSG :Not enough parametes");
        return;
    }

    std::string target = msg.params[0];
    std::string message = msg.trailing;

    if(target == "#")
    {
        Channel *channel = new Channel(target);
        if(!channel)
        {
            client->sendError("401", target + " :No such nick/channel");
            return;
        }

        if(!channel->hasClient(*client))
        {
            client->sendError("404", target + " :Cannot send to the channel");
            return;
        }

        channel->broadcast(":" + client->getPrefix() + " PRIVMSG " + target + " :" + message, NULL);
    }
    else
    {
        Client *targetClient = getClientByNick(target);
        if(!targetClient)
        {
            client->sendError("401", target + ":No such nick/channel");
            return;
        }

        targetClient->send(":" + client->getPrefix() + " PRIVMSG " + target + " :" + message);
    }
}

void Server::handleKick(const IrcMessage &msg,Client *client)
{
    if(!client->isRegistered())
    {
        client->sendError("451", ":You have not registered");
        return;
    }
    if(msg.params.size() < 2)
    {
        client->sendError("461", "KICK :Not enough parametes");
        return;
    }
    
}

void Server::handleInvite(const IrcMessage &msg,Client *client)
{
    
}

void Server::handleTopic(const IrcMessage &msg,Client *client)
{}
void Server::handleMode(const IrcMessage &msg,Client *client)
{}
void Server::handleChannelMode(const IrcMessage &msg, Client *client, const std::string &channelName)
{}

Server::~Server()
{
    if(running)
        stop();
}