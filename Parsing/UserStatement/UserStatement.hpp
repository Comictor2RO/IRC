#ifndef __USER_STATEMENT_HPP__
#define __USER_STATEMENT_HPP__

#include "Statement.hpp"

class UserStatement : public Statement {
public:
    UserStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing);
    
    virtual void execute(Client& client, Server& server);
    
    std::string getUsername() const;
    std::string getHostname() const;
    std::string getServername() const;
    std::string getRealname() const;
};

#endif