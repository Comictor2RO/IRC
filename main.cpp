#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <exception>

// Prima: #include "Server/Server.hpp", porta convertita con atoi senza controlli
// (atoi("abc") = 0), nessun controllo sulla password e server.start() senza try/catch.
// Ora la classe Server e' quella di youssef: il metodo e' run() e gli errori di avvio
// (es. porta occupata) arrivano come eccezione.

int main(int ac, char **av)
{
    if(ac != 3)
    {
        std::cerr << "Usage: " << av[0] << " <port> <password>" << std::endl;
        return 1;
    }

    // strtol instead of atoi: atoi("abc") returns 0 without any error
    char *end = NULL;
    long port = std::strtol(av[1], &end, 10);
    if(*av[1] == '\0' || *end != '\0' || port < 1 || port > 65535)
    {
        std::cerr << "Error: invalid port (1-65535)" << std::endl;
        return 1;
    }

    std::string pass = av[2];
    if(pass.empty() || pass.find(' ') != std::string::npos)
    {
        std::cerr << "Error: invalid password (empty or contains spaces)" << std::endl;
        return 1;
    }

    try
    {
        Server server(static_cast<int>(port), pass);
        server.run();
    }
    catch(const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
