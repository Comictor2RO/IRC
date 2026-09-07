#include "KickStatement.hpp"

KickStatement::KickStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing)
{
    this->prefix = prefix;
    this->command = "KICK";
    this->params = params;
    this->trailing = trailing;
}

void KickStatement::execute(Client& client, Server& server)
{
    // TO DO    
}

std::string KickStatement::getChannel() const
{
    if (!params.empty())
        return params[0];
    return "";
}

std::string KickStatement::getUser() const
{
    if (params.size() >= 2)
        return params[1];
    return "";
}

std::string KickStatement::getReason() const
{
    return trailing;
}