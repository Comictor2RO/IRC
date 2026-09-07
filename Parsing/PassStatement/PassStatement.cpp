#include "PassStatement.hpp"

PassStatement::PassStatement(const std::string& prefix, const std::vector<std::string> params, const std::string trailing)
{
    this->prefix = prefix;
    this->command = "PASS";
    this->params = params;
    this->trailing = trailing;
}

void PassStatement::execute(Client &client, Server &server)
{
    //TO DO
}

std::string PassStatement::getPassword() const
{
    if(!params.empty())
        return params[0];
    return "";
}