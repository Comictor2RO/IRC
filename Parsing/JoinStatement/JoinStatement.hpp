#ifndef __JOIN_STATEMENT_HPP__
#define __JOIN_STATEMENT_HPP__

#include "../Statement.hpp"

class JoinStatement : public Statement{
    public:
        JoinStatement(const std::string prefix, std::vector<std::string> params, std::string trailing);

        virtual void execute(Client& client, Server& server);

        std::string getKey() const;
        std::string getChannel() const;
};

#endif