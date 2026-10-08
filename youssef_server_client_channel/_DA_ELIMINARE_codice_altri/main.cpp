#include "Server.hpp"                  // classe Server
#include <iostream>                    // std::cout / std::cerr
#include <cstdlib>                     // std::strtol
#include <exception>                   // std::exception

/* =====================================================================================
**  main  —  ./ircserv <port> <password>
**   1. controlla gli argomenti: numero, porta numerica tra 1 e 65535, password non vuota
**      e senza spazi (std::atoi("abc") restituirebbe 0 senza avvisare: per questo strtol);
**   2. crea il Server e chiama run(), che termina solo con Ctrl+C;
**   3. un errore di avvio (es. porta già in uso) arriva come eccezione: lo si stampa e si
**      esce con codice 1. Il distruttore del Server pulisce in ogni caso.
** ===================================================================================== */
int main(int argc, char **argv)
{
    if (argc != 3)                                                     // servono esattamente 2 argomenti
    {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }
    char *end = NULL;                                                  // dove strtol si ferma
    long port = std::strtol(argv[1], &end, 10);                        // converte la porta
    if (*argv[1] == '\0' || *end != '\0' || port < 1 || port > 65535)  // vuota, non numerica o fuori range
    {
        std::cerr << "Error: invalid port (1-65535)" << std::endl;
        return 1;
    }
    std::string password = argv[2];                                    // password del server
    if (password.empty() || password.find(' ') != std::string::npos)   // vuota o con spazi (PASS non la accetterebbe)
    {
        std::cerr << "Error: invalid password (empty or contains spaces)" << std::endl;
        return 1;
    }
    try
    {
        Server server(static_cast<int>(port), password);               // crea il server
        server.run();                                                  // loop fino a Ctrl+C
    }
    catch (const std::exception &e)                                    // errore fatale (socket/bind/listen)
    {
        std::cerr << "Error: " << e.what() << std::endl;               // stampa il motivo
        return 1;
    }
    return 0;                                                          // uscita pulita
}
