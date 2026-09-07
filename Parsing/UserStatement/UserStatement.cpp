// UserStatement.cpp
#include "UserStatement.hpp"

UserStatement::UserStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing)
{
    this->prefix = prefix;
    this->command = "USER";
    this->params = params;
    this->trailing = trailing;
}

void UserStatement::execute(Client& client, Server& server) {
    // TO DO
}

std::string UserStatement::getUsername() const {
    if (!params.empty())
        return params[0];
    return "";
}

std::string UserStatement::getHostname() const {
    if (params.size() >= 2)
        return params[1];
    return "";
}

std::string UserStatement::getServername() const {
    if (params.size() >= 3)
        return params[2];
    return "";
}

std::string UserStatement::getRealname() const {
    return trailing;
}