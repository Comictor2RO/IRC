#include "IrcParser.hpp"
#include <cctype>

IrcMessage IrcParser::parse (const std::string &raw)
{
    IrcMessage msg;
    size_t pos = 0;

    // Extract the prefix
    if(!raw.empty() && raw[0] == ':')
    {
        size_t spacePos = raw.find(' ', 1);
        if(spacePos != std::string::npos)
        {
            msg.prefix = raw.substr(1, spacePos - 1);
            pos = spacePos + 1;
        }
    }

    // Extracts command
    size_t spacePos = raw.find(' ', pos);
    if(spacePos != std::string::npos)
    {
        msg.command = raw.substr(pos, spacePos - pos);
        pos = spacePos + 1;
    }
    else
    {
        msg.command = raw.substr(pos);
        for (size_t i = 0; i < msg.command.size(); ++i)
            msg.command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(msg.command[i])));
        return msg;
    }

    for (size_t i = 0; i < msg.command.size(); ++i)
        msg.command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(msg.command[i])));

    // Extract params & trailing
    while(pos < raw.length())
    {
        // Jumps over spaces
        while(pos < raw.length() && raw[pos] == ' ')
            pos++;

        if(pos >= raw.length())
            break;

        // Check if there is trailing (starts with ':')
        if(raw[pos] == ':')
        {
            msg.trailing = raw.substr(pos + 1);
            break;
        }

        spacePos = raw.find(' ', pos);
        if(spacePos != std::string::npos)
        {
            msg.params.push_back(raw.substr(pos, spacePos - pos));
            pos = spacePos + 1;
        }
        else
        {
            msg.params.push_back(raw.substr(pos));
            break;
        }
    }

    return msg;
}