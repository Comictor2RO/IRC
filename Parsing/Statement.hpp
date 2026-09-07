#ifndef __STATEMENT_HPP__
#define __STATEMENT_HPP__

#include <string>
#include <vector>

class Statement{
    public:
        virtual ~Statement();
        virtual void execute() = 0;

        std::string getPrefix() const { return prefix; }
        std::string getCommand() const { return command; }
        const std::vector<std::string>& getParams() const { return params; }
        std::string getTrailing() const { return trailing; } 

    protected:
        std::string prefix;
        std::string command;
        std::vector<std::string> params;
        std::string trailing;
};

#endif