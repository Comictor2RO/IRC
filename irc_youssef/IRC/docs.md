1. AF_INET -> ,,Address Family: Internet". It is used for the structure sockaddr_in to specify that it will use IPv4 addresses.

2. SOCK_STREAM -> Socket type: ordered connection focused (TCP)

3. SOL_SOCKET -> general settings for a socket used as the first argument for setsockopt/getsockopt.

4. SO_REUSEADDR -> a socket setting that allows you to reuse the addresses or ports after the previuos socket was closed.(avoids TIME_WAIT which restricts the server at restart)

5. INADDR_ANY -> Special address for IPv4 0.0.0.0; Shows the fact that the server listens all the local networks.

6. POLLIN -> Events flag for poll: Shows the fact that in the descriptor exists data for reading(or closed connection depending on the context)

7. F_GETFL -> fcntl command which returns the current status of descriptors.

8. F_SETFL -> fcntl command which sets the status flags of a descriptor(usually you take the value with F_GETFL, you modify the bytes, and then you save them with F_SETFL)

9. O_NONBLOCK -> it is a flag that activates the non-blocking mode for descriptors: used for read()/accept()/accept()/connect() which will not block instead they will return immediatly;
