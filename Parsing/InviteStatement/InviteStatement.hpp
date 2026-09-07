#ifndef __INVITE_STATEMENT_HPP__
#define __INVITE_STATEMENT_HPP__

#include "Statement.hpp"

class InviteStatement : public Statement {
public:
    InviteStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing);
    
    virtual void execute(Client& client, Server& server);
    
    std::string getNickname() const;
    std::string getChannel() const;
};

#endif