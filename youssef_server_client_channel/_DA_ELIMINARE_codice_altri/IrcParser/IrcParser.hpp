#ifndef IRC_PARSER_HPP                 // include guard
#define IRC_PARSER_HPP                 // definisce la macro della guard

#include <string>                      // std::string
#include <vector>                      // std::vector per i parametri

/*
** =====================================================================================
**  IrcMessage — una riga IRC già divisa nei suoi pezzi
** =====================================================================================
**  Formato di una riga IRC:   [:prefix] COMMAND [param1 param2 ...] [:trailing]
**  Esempio:  ":alice PRIVMSG #chan :ciao a tutti"
**     prefix   = "alice"
**     command  = "PRIVMSG"         (sempre convertito in MAIUSCOLO: "join" == "JOIN")
**     params   = ["#chan"]
**     trailing = "ciao a tutti"    (l'ultimo parametro: dopo ':' può contenere spazi)
**     hasTrailing = true           (serve a distinguere "TOPIC #c :" (topic vuoto = cancella)
**                                   da "TOPIC #c" (chiede il topic))
** =====================================================================================
*/
struct IrcMessage
{
    std::string                 prefix;      // mittente (i client normalmente non lo mandano)
    std::string                 command;     // comando in maiuscolo
    std::vector<std::string>    params;      // parametri "normali" (senza spazi)
    std::string                 trailing;    // ultimo parametro dopo ':'
    bool                        hasTrailing; // true se nella riga c'era il ':' finale

    IrcMessage() : hasTrailing(false) {}     // costruttore: hasTrailing parte a false
};

class IrcParser
{
public:
    static IrcMessage parse(const std::string &raw); // analizza una riga (senza \r\n)
};

#endif                                       // fine include guard
