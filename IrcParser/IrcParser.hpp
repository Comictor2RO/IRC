#ifndef IRCPARSER_HPP
#define IRCPARSER_HPP

// Prima: guard __IRC_PARSER_HPP__ (i nomi con doppio underscore sono riservati al compilatore)
// e IrcMessage senza il campo hasTrailing.

#include <string>
#include <vector>

// [:prefix] COMMAND [params...] [:trailing]
// hasTrailing distingue "TOPIC #c :" (topic vuoto = cancella) da "TOPIC #c" (lettura)
struct IrcMessage{
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
    std::string trailing;
    bool hasTrailing;

    IrcMessage() : hasTrailing(false) {}
};

class IrcParser{
    public:
        static IrcMessage parse(const std::string &raw);
};

#endif
