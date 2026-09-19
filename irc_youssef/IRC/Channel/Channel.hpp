#ifndef __CHANNEL_HPP__
#define __CHANNEL_HPP__

#include <string>
#include <vector>
#include <set>

class Client;

class Channel {
    public:
        // Constructor/Destructor
        Channel(const std::string& name);
        ~Channel();
        
        // Getters
        std::string getName() const;
        std::string getTopic() const;
        std::string getKey() const;
        int getLimit() const;
        std::string getModes() const;  // Returnează "itkl" dacă sunt active
        bool isInviteOnly() const;
        bool isTopicRestricted() const;
        bool hasKey() const;
        bool isFull() const;
        
        // Setters
        void setTopic(const std::string& topic);
        void setKey(const std::string& key);
        void setLimit(int limit);
        void setInviteOnly(bool inviteOnly);
        void setTopicRestricted(bool restricted);
        
        // Clients
        void addClient(Client& client);
        void removeClient(Client& client);
        bool hasClient(Client& client) const;
        std::vector<Client*> getClients() const;
        int getClientCount() const;
        
        // Operators
        void addOperator(Client& client);
        void removeOperator(Client& client);
        bool isOperator(Client& client) const;
        
        // Invites
        void addInvite(Client& client);
        bool isInvited(Client& client) const;
        void removeInvite(Client& client);
        
        // Bans
        void addBan(const std::string& mask);
        void removeBan(const std::string& mask);
        bool isBanned(Client& client) const;
        
        // Broadcast
        void broadcast(const std::string& msg, Client* exclude = NULL);
        void sendTopic(Client& client);
        void sendNames(Client& client);

    private:
        std::string name;
        std::string topic;
        std::string key;
        int limit;
        bool inviteOnly;
        bool topicRestricted;
        std::vector<Client*> clients;
        std::set<Client*> operators;
        std::set<Client*> invites;
        std::set<std::string> bans;
};

#endif