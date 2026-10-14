# ft_irc

This project was created as part of the 42 curriculum by yel-moha, vturlas,
and <login_gabry>.

## Description

`ft_irc` is a lightweight IRC server written in C++98. It implements the
client-to-server part of the IRC protocol over TCP and is designed to work
with standard IRC clients such as Irssi, HexChat, or WeeChat.

The server supports multiple simultaneous clients without threads or
processes. Clients authenticate with a server password, register an IRC
identity, join channels, exchange private or channel messages, and use
channel operator features.

### Features

#### Registration and connection management

- `PASS` — authenticate with the server password.
- `NICK` — set or change a unique nickname.
- `USER` — set the username and real name and complete registration.
- `CAP` — handle capability negotiation requests from modern clients.
- `PING` / `PONG` — keep the connection alive.
- `QUIT` — disconnect cleanly and notify channel members.

#### Channels and messaging

- `JOIN` — join an existing channel or create it when it does not exist.
- `PART` — leave a channel.
- `PRIVMSG` — send a private message to a user or broadcast to a channel.
- `NOTICE` — send a notice without generating normal error replies.
- `TOPIC` — read or update a channel topic.
- `WHO` — list the users in a channel.
- `KICK` — remove a member from a channel as an operator.
- `INVITE` — invite a user to an invite-only channel.
- `MODE` — inspect or change the supported channel modes.

The supported channel modes are:

- `i` — invite-only channel;
- `t` — only channel operators may change the topic;
- `k` — channel key/password;
- `o` — grant or remove channel-operator privileges;
- `l` — maximum number of channel members.

The server sends standard IRC numeric replies and errors, allowing reference
clients to interpret registration, channel, messaging, and permission events.

The project does not currently implement optional bonus features such as DCC
file transfer or an IRC bot. It also does not implement server-to-server IRC
links or TLS.

### Project structure

| Directory/file | Purpose |
| --- | --- |
| `Server/` | Listening socket, event loop, client lifecycle, command dispatch, and server state. |
| `Client/` | Per-connection identity, registration state, input buffer, and output buffer. |
| `Channel/` | Channel membership, topics, invites, operators, and channel modes. |
| `IrcParser/` | Parsing IRC lines into commands, parameters, and trailing parameters. |
| `Commands/` | Handlers for registration, messaging, channel commands, and modes. |
| `docs/` | Subject notes, design documentation, testing guides, and evaluation checklists. |
| `main.cpp` | Validates command-line arguments, creates the server, and starts it. |
| `Makefile` | C++98 build and cleanup targets. |

## Instructions

### Compilation

The project requires a C++ compiler and standard POSIX socket headers.
Compile it from the repository root:

```bash
make
```

Additional Makefile targets:

```bash
make clean   # remove object and dependency files
make fclean  # remove object files and the ircserv executable
make re      # perform a full clean rebuild
```

### Execution

Start the server with a TCP port and a connection password:

```bash
./ircserv <port> <password>
```

For example:

```bash
./ircserv 6667 password
```

The port must be between `1` and `65535`. The password must not be empty or
contain spaces. The server binds to `0.0.0.0`, so it listens on all available
network interfaces.

### Connecting with a client

#### Using `nc`

`nc` is useful for testing the raw IRC protocol. The `-C` option makes it send
the CRLF line endings expected by IRC:

```bash
nc -C 127.0.0.1 6667
```

Then register and join a channel:

```text
PASS password
NICK alice
USER alice 0 * :Alice Example
JOIN #test
PRIVMSG #test :Hello from alice!
```

Open a second terminal and repeat the registration with another nickname to
test channel broadcasting and private messages.

#### Using a reference IRC client

With Irssi, for example:

```text
/connect 127.0.0.1 6667 password
/join #test
```

HexChat and WeeChat can be configured with the same host, port, and server
password. TLS/SSL should be disabled because this server exposes a plain TCP
IRC connection.

## Technical choices

- **Single event loop:** one `poll()` call monitors the listening socket and
  every connected client.
- **Non-blocking I/O:** the listening socket and client sockets use
  `fcntl(fd, F_SETFL, O_NONBLOCK)`.
- **TCP/IPv4 sockets:** the server binds to `INADDR_ANY` and accepts multiple
  client connections.
- **Buffered protocol handling:** input is accumulated until complete IRC
  lines are available, so fragmented TCP packets and multiple commands in one
  packet are handled correctly.
- **Poll-driven output:** replies are queued in each client's output buffer and
  sent when `poll()` reports `POLLOUT`, preventing a slow client from blocking
  the server.
- **Explicit ownership:** `Server` owns `Client` and `Channel` objects.
  `Channel` stores non-owning client pointers, while `Client` stores
  non-owning channel pointers.
- **C++98 implementation:** the code uses the required `-Wall -Wextra -Werror`
  flags and does not depend on Boost or other external libraries.
- **Clean shutdown:** `SIGINT` and `SIGQUIT` request a controlled shutdown;
  client sockets, channels, and the listening socket are then released.

## Resources

### References

- [RFC 1459 — Internet Relay Chat Protocol](https://www.rfc-editor.org/rfc/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://www.rfc-editor.org/rfc/rfc2812)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- The official 42 `ft_irc` subject and evaluation sheet, available in
  `docs/01_subject/`
- Project-specific design and testing notes in `docs/03_progettazione/` and
  `docs/04_test/`

The repository also contains a practical testing guide,
[`docs/04_test/GUIDA_SEMPLICE.md`](docs/04_test/GUIDA_SEMPLICE.md), and a
feature checklist in [`CORREZIONE_FT_IRC.md`](CORREZIONE_FT_IRC.md).

### AI usage

AI assistance was used for project analysis and documentation, including:

- reviewing the repository structure and existing implementation;
- checking that the README reflects the implemented commands and architecture;
- improving the clarity and English wording of this documentation.

AI was not used as a substitute for understanding the implementation. The
team remains responsible for every line of code and must be able to explain
the socket lifecycle, `poll()` event loop, IRC parsing, buffering, ownership,
registration flow, and channel permissions during the project evaluation.
