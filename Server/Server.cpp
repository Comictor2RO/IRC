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

void Server::handlePass(IrcMessage &msg,Client *client)
{}
void Server::handleNick(IrcMessage &msg,Client *client)
{}
void Server::handleUser(IrcMessage &msg,Client *client)
{}
void Server::handleJoin(IrcMessage &msg,Client *client)
{}
void Server::handlePrivmsg(IrcMessage &msg,Client *client)
{}
void Server::handleKick(IrcMessage &msg,Client *client)
{}
void Server::handleInvite(IrcMessage &msg,Client *client)
{}
void Server::handleTopic(IrcMessage &msg,Client *client)
{}
void Server::handleMode(IrcMessage &msg,Client *client)
{}

Server::~Server()
{
    if(running)
        stop();
}