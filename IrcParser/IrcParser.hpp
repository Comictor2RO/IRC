#ifndef __IRC_PARSER_HPP__
#define __IRC_PARSER_HPP__

#include <string>
#include <vector>

struct IrcMessage{
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
    std::string trailing;
};

class IrcParser{
    public:
        static IrcMessage parse(const std::string &raw);
};

#endif