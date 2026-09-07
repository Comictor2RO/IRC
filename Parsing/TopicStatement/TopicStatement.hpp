#ifndef __TOPIC_STATEMENT_HPP__
#define __TOPIC_STATEMENT_HPP__

#include "Statement.hpp"

class TopicStatement : public Statement {
public:
    TopicStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing);
    
    virtual void execute(Client& client, Server& server);
    
    std::string getChannel() const;
    std::string getTopic() const;
    bool hasTopic() const;
};

#endif
