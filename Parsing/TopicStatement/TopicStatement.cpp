#include "TopicStatement.hpp"

TopicStatement::TopicStatement(const std::string& prefix, const std::vector<std::string>& params, const std::string& trailing)
{
    this->prefix = prefix;
    this->command = "TOPIC";
    this->params = params;
    this->trailing = trailing;
}

void TopicStatement::execute(Client& client, Server& server)
{
    // TO DO
}

std::string TopicStatement::getChannel() const
{
    if (!params.empty())
        return params[0];
    return "";
}

std::string TopicStatement::getTopic() const
{
    return trailing;
}

bool TopicStatement::hasTopic() const
{
    return !trailing.empty();
}