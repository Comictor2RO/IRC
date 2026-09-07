#ifndef __NICK_STATEMENT_HPP__
#define __NICK_STATEMENT_HPP__

#include "../Statement.hpp"

class NickStatement : public Statement{
    public:
        NickStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing);

        virtual void execute(Client& client, Server& server);

        std::string getNickname() const;
    
};

#endif