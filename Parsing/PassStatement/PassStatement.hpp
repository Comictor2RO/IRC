#ifndef __PASS_STATEMENT_HPP__
#define __PASS_STATEMENT_HPP__

#include "../Statement.hpp"

class PassStatement : public Statement{
    public:
        PassStatement(const std::string& prefix, const std::vector<std::string> params, const std::string trailing);

        virtual void execute(Client &client, Server &server); //I need Client & Server Classes to complete the parsing

        std::string getPassword() const;
};

#endif