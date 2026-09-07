#include "JoinStatement.hpp"

JoinStatement::JoinStatement(const std::string prefix, std::vector<std::string> params, std::string trailing)
{
    this->prefix = prefix;
    this->command = "JOIN";
    this->params = params;
    this->trailing = trailing;
}

void JoinStatement::execute(Client& client, Server& server)
{
    // TO DO
}

std::string JoinStatement::getKey() const
{
    if(!params.empty())
        return params[0];
    return "";
}

std::string JoinStatement::getChannel() const
{
    if(params.size() >= 2)
        return params[1];
    return "";
}