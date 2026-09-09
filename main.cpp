#include "Server/Server.hpp"
#include <cstdlib>

int main(int ac, char **av)
{
    if(ac != 3)
    {
        std::cout << "Usage: " << av[0] << "[port] [password].\n";
        return 1;
    }

    int port = std::atoi(av[1]);
    std::string pass = av[2];

    Server server(port, pass);

    server.start();
    
    return 0;
}