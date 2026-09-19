First week

1. Client Class (priority)
    - Constructor with fd
    - Getters/Setters for nickname, user, password
    - isAuth() → return if it has PASS
    - isRegistered() → returns if it has PASS + NICK + USER
    - tryRegister() → if it has PASS + NICK + USER, sets registered = true and send a welcome message.
    - send() → send a message to the client with ::send(fd, ...)
    - sendError() → sends IRC Errors (ex: "461 * PASS:Not enough parameters...")
    - sendReply() → send reply IRC (ex: "001 Nick:Welcome...")
    - Buffer handling: appendToBuffer(), clearBuffer().
    - markForDeletion → sets a flag for a user that needs to be deleted.

2. Channel Class (priority)
    - Constructor with channel name
    - Getters/setters for topic, key, limit
    - Mode flags: inviteOnly, topicRestricted
    - addClient() / removeClient() → member management
    - addOperator() / removeOperator() → first client is becoming automaticly Operator(admin)
    - addInvite() / isInvited() → for invite-only channels
    - addBan() / isBanned() → ban list
    - broadcast() → sends a message to all members in that channel
    - sendTopic() → sends RPL_TOPIC (332) or RPL_NOTOPIC (331)
    - sendNames() → sends RPL_NAMREPLY (353) with the user list

3. Server Class (priotity)
    - Socket + Poll(ask Vlad for the example)
    - Socket creation, bind, listen
    - Poll loop with poll()
    - New connection → Client *(new connections are creating new objects of type Client *)
    - Non-blocking I/O with fcntl() + O_NONBLOCK
    - Management POLLHUP, POLLERR, EAGAIN


TIPS & TRICKS
- Some references to make it easy!

1. Data structures:
    -std::map<int, Client*> clientsByFD → for Client declared in Server(private)
    -std::map<std::string, Client*> clientsByNick → for Client declared in Server(private)
    -std::map<std::string, Channel*> channels → for Channel declared in Server(private)

2. Client management:
    -addClient() → adds a client in maps
    -removeClient() → removes from maps, close fd, delete
    -getClientByFd() → finds a client after fd
    -getClientByNick() → finds a client after nick
    -isNickTaken() → checks if the nickname is used

3. Channel management:
    -getChannel() → finds a channel after the name
    -createChannel() → creates a channel if it does not exists
    -removeChannel() → deletes the channel

