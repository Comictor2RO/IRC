#ifndef __KICK_STATEMENT_HPP__
#define __KICK_STATEMENT_HPP__

#include "Statement.hpp"

class KickStatement : public Statement {
public:
    KickStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing);
    
    virtual void execute(Client& client, Server& server);
    
    std::string getChannel() const;
    std::string getUser() const;
    std::string getReason() const;
};

#endif
