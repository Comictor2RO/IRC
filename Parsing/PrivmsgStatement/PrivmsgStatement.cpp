#include "PrivmsgStatement.hpp"

PrivmsgStatement::PrivmsgStatement(const std::string& prefix,
                                   const std::vector<std::string>& params,
                                   const std::string& trailing)
{
    this->prefix = prefix;
    this->command = "PRIVMSG";
    this->params = params;
    this->trailing = trailing;
}

void PrivmsgStatement::execute(Client& client, Server& server)
{
    // TO DO
}

std::string PrivmsgStatement::getTarget() const {
    if (!params.empty())
        return params[0];
    return "";
}

std::string PrivmsgStatement::getMessage() const {
    return trailing;
}