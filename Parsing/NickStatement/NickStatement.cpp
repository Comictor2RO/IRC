#include "NickStatement.hpp"

NickStatement::NickStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing)
{
    this->prefix = prefix;
    this->command = "NICK";
    this->params = params;
    this->trailing = trailing;
}

void NickStatement::execute(Client& client, Server& server)
{
    //TO DO
}

std::string NickStatement::getNickname() const
{
    if (!params.empty())
        return params[0];
    return "";
}