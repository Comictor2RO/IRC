#include "Channel.hpp"                 // dichiarazione della classe Channel
#include "Client.hpp"                  // serve la classe completa: chiamiamo metodi di Client
#include <sstream>                     // std::ostringstream: converte il limite (int) in testo

/* =====================================================================================
**  COSTRUTTORE
**  Un canale nasce senza topic, senza chiave, senza limite e senza modi attivi.
**  Viene creato dal Server quando il primo utente fa JOIN di un nome che non esiste.
** ===================================================================================== */
Channel::Channel(const std::string &name)
    : name(name),                      // salva il nome
      limit(0),                        // 0 = nessun limite di utenti
      inviteOnly(false),               // -i
      topicRestricted(false)           // -t
{}

Channel::~Channel()
{}                                     // nulla da liberare: i Client appartengono al Server

// ------------------------------------------------------------------------------ GETTER

const std::string &Channel::getName() const
{
    return name;                       // nome del canale
}

const std::string &Channel::getTopic() const
{
    return topic;                      // argomento
}

const std::string &Channel::getKey() const
{
    return key;                        // chiave
}

int Channel::getLimit() const
{
    return limit;                      // limite utenti
}

/* =====================================================================================
**  getModes()
**  Costruisce la stringa dei modi attivi per la risposta 324 (RPL_CHANNELMODEIS),
**  mettendo in fondo i parametri dei modi che li hanno, nello stesso ordine delle lettere.
**  Esempi: nessun modo -> "+"   |   +i +t -> "+it"   |   +t +k +l -> "+tkl segreto 10"
** ===================================================================================== */
std::string Channel::getModes() const
{
    std::string modes = "+";           // le lettere attive
    std::string params;                // i loro parametri
    if (inviteOnly)                    // +i attivo
        modes += "i";                  // aggiunge la lettera
    if (topicRestricted)               // +t attivo
        modes += "t";                  // aggiunge la lettera
    if (hasKey())                      // +k attivo
    {
        modes += "k";                  // aggiunge la lettera
        params += " " + key;           // e la chiave come parametro
    }
    if (limit > 0)                     // +l attivo
    {
        std::ostringstream oss;        // stream per convertire l'intero in stringa (niente to_string in C++98)
        oss << limit;                  // scrive il numero nello stream
        modes += "l";                  // aggiunge la lettera
        params += " " + oss.str();     // e il limite come parametro
    }
    return modes + params;             // es. "+tkl segreto 10"
}

bool Channel::isInviteOnly() const
{
    return inviteOnly;                 // +i?
}

bool Channel::isTopicRestricted() const
{
    return topicRestricted;            // +t?
}

bool Channel::hasKey() const
{
    return !key.empty();               // c'è una chiave impostata?
}

bool Channel::isFull() const
{
    return limit > 0 && static_cast<int>(clients.size()) >= limit; // limite attivo e raggiunto
}

// ------------------------------------------------------------------------------ SETTER

void Channel::setTopic(const std::string &topic)
{
    this->topic = topic;               // nuovo argomento ("" = cancellato)
}

void Channel::setKey(const std::string &key)
{
    this->key = key;                   // nuova chiave ("" = tolta)
}

void Channel::setLimit(int limit)
{
    this->limit = limit;               // nuovo limite (0 = tolto)
}

void Channel::setInviteOnly(bool inviteOnly)
{
    this->inviteOnly = inviteOnly;     // +i / -i
}

void Channel::setTopicRestricted(bool restricted)
{
    topicRestricted = restricted;      // +t / -t
}

// ------------------------------------------------------------------------------ MEMBRI

/* =====================================================================================
**  addClient()
**  Aggiunge un membro. Le verifiche (+i, +k, +l) le fa l'handler di JOIN PRIMA di chiamarla.
**   1. se è già membro non fa nulla (difesa contro i duplicati);
**   2. lo aggiunge in fondo alla lista e avvisa il Client (joinChannel) così entrambi
**      i lati sanno del legame: il Server, quando il client si disconnette, sa da quali
**      canali toglierlo;
**   3. l'eventuale invito è "consumato": se esce e il canale è +i dovrà essere reinvitato;
**   4. se è il PRIMO membro (ha appena creato il canale) diventa operatore.
** ===================================================================================== */
void Channel::addClient(Client &client)
{
    if (hasClient(client))             // già dentro?
        return;                        // niente duplicati
    clients.push_back(&client);        // aggiunge alla lista dei membri
    client.joinChannel(this);          // il client registra il canale nella sua lista
    invites.erase(&client);            // invito usato
    if (clients.size() == 1)           // primo membro = creatore del canale
        operators.insert(&client);     // diventa operatore
}

/* =====================================================================================
**  removeClient()
**  Toglie un membro (PART, KICK, QUIT o disconnessione):
**   1. lo cerca nella lista; se non c'è non fa nulla;
**   2. lo toglie dai membri, dagli operatori e dagli invitati, e avvisa il Client;
**   3. se era l'ULTIMO operatore e nel canale resta qualcuno, il membro più "anziano"
**      (il primo della lista) diventa operatore: altrimenti il canale resterebbe senza
**      nessuno capace di gestirlo. La promozione viene annunciata a tutti con un MODE +o,
**      così i client IRC aggiornano la "@" accanto al nome.
**  Se il canale resta vuoto è il Server a cancellarlo (removeChannelIfEmpty).
** ===================================================================================== */
void Channel::removeClient(Client &client)
{
    for (std::vector<Client *>::iterator it = clients.begin(); it != clients.end(); ++it) // cerca il membro
    {
        if (*it != &client)            // non è lui
            continue;                  // passa al prossimo
        bool wasOperator = isOperator(client);         // era operatore? (va letto prima di toglierlo)
        clients.erase(it);             // lo toglie dalla lista (it non è più valido: non lo usiamo più)
        operators.erase(&client);      // non è più operatore
        invites.erase(&client);        // né invitato
        client.leaveChannel(this);     // il client dimentica il canale
        if (wasOperator && operators.empty() && !clients.empty()) // canale rimasto senza operatori?
        {
            operators.insert(clients[0]);                           // promuove il membro più anziano
            broadcast(":" SERVER_NAME " MODE " + name + " +o " + clients[0]->getNickname()); // lo annuncia
        }
        return;                        // fatto
    }
}

bool Channel::hasClient(Client &client) const
{
    for (std::vector<Client *>::const_iterator it = clients.begin(); it != clients.end(); ++it) // scorre i membri
        if (*it == &client)            // confronta gli indirizzi: stesso oggetto Client
            return true;               // è membro
    return false;                      // non è membro
}

std::vector<Client *> Channel::getClients() const
{
    return clients;                    // copia: chi la scorre può modificare il canale senza problemi
}

int Channel::getClientCount() const
{
    return static_cast<int>(clients.size()); // numero di membri
}

bool Channel::isEmpty() const
{
    return clients.empty();            // true se non c'è nessuno
}

// ------------------------------------------------------------------------------ OPERATORI

void Channel::addOperator(Client &client)
{
    if (hasClient(client))             // solo un membro può essere operatore
        operators.insert(&client);     // il set ignora i duplicati da solo
}

void Channel::removeOperator(Client &client)
{
    operators.erase(&client);          // erase su un elemento assente non fa nulla
}

bool Channel::isOperator(Client &client) const
{
    return operators.find(&client) != operators.end(); // trovato nel set?
}

// ------------------------------------------------------------------------------ INVITI

void Channel::addInvite(Client &client)
{
    invites.insert(&client);           // ricorda l'invito
}

bool Channel::isInvited(Client &client) const
{
    return invites.find(&client) != invites.end(); // invitato?
}

void Channel::removeInvite(Client &client)
{
    invites.erase(&client);            // dimentica l'invito
}

// ------------------------------------------------------------------------------ INVIO MESSAGGI

/* =====================================================================================
**  broadcast()
**  Accoda `msg` a tutti i membri del canale, tranne `exclude` (di solito il mittente:
**  chi scrive in un canale non deve ricevere di nuovo il proprio PRIVMSG).
**  Per JOIN / PART / KICK / MODE / TOPIC si passa exclude = NULL perché anche chi ha fatto
**  il comando deve vedere la conferma.
**  Client::send() accoda soltanto: i messaggi partono al POLLOUT di ciascun client.
** ===================================================================================== */
void Channel::broadcast(const std::string &msg, Client *exclude)
{
    for (std::vector<Client *>::iterator it = clients.begin(); it != clients.end(); ++it) // ogni membro
        if (*it != exclude)            // tranne quello da escludere
            (*it)->send(msg);          // accoda il messaggio
}

/* =====================================================================================
**  sendTopic()
**  Risponde a "TOPIC #canale" (senza nuovo testo) e viene usata dopo JOIN:
**   332 RPL_TOPIC   "<canale> :<topic>"            se il topic esiste
**   331 RPL_NOTOPIC "<canale> :No topic is set"    altrimenti
** ===================================================================================== */
void Channel::sendTopic(Client &client)
{
    if (!topic.empty())                                         // c'è un topic?
        client.sendReply("332", name + " :" + topic);           // RPL_TOPIC
    else
        client.sendReply("331", name + " :No topic is set");    // RPL_NOTOPIC
}

/* =====================================================================================
**  sendNames()
**  Invia la lista dei membri, richiesta dopo ogni JOIN:
**   353 RPL_NAMREPLY   "= <canale> :@alice bob carol"   ('@' davanti agli operatori)
**   366 RPL_ENDOFNAMES "<canale> :End of /NAMES list"
**  '=' indica un canale pubblico. HexChat/irssi usano questa lista per la colonna utenti.
** ===================================================================================== */
void Channel::sendNames(Client &client)
{
    std::string names;                 // lista dei nick separati da spazio
    for (std::vector<Client *>::iterator it = clients.begin(); it != clients.end(); ++it) // ogni membro
    {
        if (!names.empty())            // non è il primo nome
            names += " ";              // separatore
        if (isOperator(**it))          // **it = il Client puntato
            names += "@";              // prefisso operatore
        names += (*it)->getNickname(); // aggiunge il nick
    }
    client.sendReply("353", "= " + name + " :" + names);           // RPL_NAMREPLY
    client.sendReply("366", name + " :End of /NAMES list");        // RPL_ENDOFNAMES
}
