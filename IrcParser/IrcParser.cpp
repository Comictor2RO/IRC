#include "IrcParser.hpp"
#include <cctype>

// Prima:
//  - non saltava gli spazi a inizio riga;
//  - una riga con solo il prefisso (":nick") diventava un comando ":nick";
//  - il comando veniva messo in maiuscolo in due punti diversi;
//  - non c'era hasTrailing, quindi "TOPIC #c :" (cancella il topic) e "TOPIC #c" (lo legge)
//    non si potevano distinguere.

static void skipSpaces(const std::string &raw, size_t &pos)
{
    while(pos < raw.length() && raw[pos] == ' ')
        pos++;
}

IrcMessage IrcParser::parse(const std::string &raw)
{
    IrcMessage msg;
    size_t pos = 0;

    skipSpaces(raw, pos);

    // Extract the prefix
    if(pos < raw.length() && raw[pos] == ':')
    {
        size_t spacePos = raw.find(' ', pos);
        if(spacePos == std::string::npos)
            return msg; // only a prefix, no command: empty message
        msg.prefix = raw.substr(pos + 1, spacePos - pos - 1);
        pos = spacePos;
        skipSpaces(raw, pos);
    }

    // Extract the command (uppercase: "join" == "JOIN")
    size_t spacePos = raw.find(' ', pos);
    if(spacePos == std::string::npos)
        spacePos = raw.length();
    msg.command = raw.substr(pos, spacePos - pos);
    for (size_t i = 0; i < msg.command.size(); ++i)
        msg.command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(msg.command[i])));
    pos = spacePos;

    // Extract params & trailing
    while(pos < raw.length())
    {
        skipSpaces(raw, pos);
        if(pos >= raw.length())
            break;

        // Trailing: everything after ':' (spaces included)
        if(raw[pos] == ':')
        {
            msg.trailing = raw.substr(pos + 1);
            msg.hasTrailing = true;
            break;
        }

        spacePos = raw.find(' ', pos);
        if(spacePos == std::string::npos)
            spacePos = raw.length();
        msg.params.push_back(raw.substr(pos, spacePos - pos));
        pos = spacePos;
    }

    return msg;
}
