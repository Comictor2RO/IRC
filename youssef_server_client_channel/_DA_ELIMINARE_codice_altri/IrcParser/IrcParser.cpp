#include "IrcParser.hpp"               // dichiarazioni di IrcMessage e IrcParser
#include <cctype>                      // std::toupper

/* =====================================================================================
**  IrcParser::parse()
**  Trasforma una riga di testo (già senza "\r\n") in un IrcMessage. Passi:
**   1. salta gli spazi iniziali;
**   2. se la riga inizia con ':' legge il prefisso fino al primo spazio;
**   3. legge il comando fino al prossimo spazio e lo converte in MAIUSCOLO;
**   4. legge i parametri uno alla volta, separati da uno o più spazi;
**      il primo parametro che inizia con ':' è il "trailing": prende tutto il resto
**      della riga, spazi compresi, e il parsing finisce.
**  Esempio: "privmsg  #a   :ciao  mondo" -> command="PRIVMSG", params=["#a"],
**           trailing="ciao  mondo", hasTrailing=true
** ===================================================================================== */
IrcMessage IrcParser::parse(const std::string &raw)
{
    IrcMessage msg;                                        // risultato (campi vuoti)
    std::string::size_type pos = 0;                        // posizione di lettura
    std::string::size_type len = raw.size();               // lunghezza della riga

    while (pos < len && raw[pos] == ' ')                   // 1. salta spazi iniziali
        ++pos;
    if (pos < len && raw[pos] == ':')                      // 2. c'è un prefisso?
    {
        std::string::size_type end = raw.find(' ', pos);   // fine del prefisso
        if (end == std::string::npos)                      // solo prefisso, niente comando
            return msg;                                    // messaggio vuoto: verrà ignorato
        msg.prefix = raw.substr(pos + 1, end - pos - 1);   // prefisso senza ':'
        pos = end;                                         // continua dopo il prefisso
        while (pos < len && raw[pos] == ' ')               // salta gli spazi
            ++pos;
    }

    std::string::size_type end = raw.find(' ', pos);       // 3. fine del comando
    if (end == std::string::npos)                          // comando senza parametri
        end = len;                                         // il comando arriva a fine riga
    msg.command = raw.substr(pos, end - pos);              // estrae il comando
    for (std::string::size_type i = 0; i < msg.command.size(); ++i) // ogni carattere
        msg.command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(msg.command[i]))); // maiuscolo
    pos = end;                                             // continua dopo il comando

    while (pos < len)                                      // 4. parametri
    {
        while (pos < len && raw[pos] == ' ')               // salta gli spazi tra i parametri
            ++pos;
        if (pos >= len)                                    // spazi finali: fine
            break;
        if (raw[pos] == ':')                               // inizio del trailing
        {
            msg.trailing = raw.substr(pos + 1);            // tutto il resto (spazi compresi)
            msg.hasTrailing = true;                        // ricorda che c'era
            break;                                         // il trailing è sempre l'ultimo
        }
        end = raw.find(' ', pos);                          // fine del parametro
        if (end == std::string::npos)                      // ultimo parametro
            end = len;                                     // arriva a fine riga
        msg.params.push_back(raw.substr(pos, end - pos));  // aggiunge il parametro
        pos = end;                                         // continua dopo il parametro
    }
    return msg;                                            // messaggio completo
}
