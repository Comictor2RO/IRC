#ifndef __PRIVMSG_STATEMENT__
#define __PRIVMSG_STATEMENT__

#include "Statement.hpp"

class PrivmsgStatement : Statement{
    public:
        PrivmsgStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing);

        virtual void execute(Client& client, Server& server);
    
        std::string getTarget() const;
        std::string getMessage() const;
};


#endif