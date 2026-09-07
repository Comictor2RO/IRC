#include "InviteStatement.hpp"

InviteStatement::InviteStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing)
{
    this->prefix = prefix;
    this->command = "INVITE";
    this->params = params;
    this->trailing = trailing;
}

void InviteStatement::execute(Client& client, Server& server)
{
    // TO DO
}

std::string InviteStatement::getNickname() const {
    if (!params.empty())
        return params[0];
    return "";
}

std::string InviteStatement::getChannel() const {
    if (params.size() >= 2)
        return params[1];
    return "";
}