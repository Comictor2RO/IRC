#ifndef CHANNEL_HPP                    // include guard: evita inclusioni multiple
#define CHANNEL_HPP                    // definisce la macro della guard

#include <string>                      // std::string: nome, topic, chiave
#include <vector>                      // std::vector: membri in ordine di ingresso
#include <set>                         // std::set: operatori e invitati (ricerca veloce, niente duplicati)

class Client;                          // forward declaration: usiamo solo puntatori/riferimenti a Client

/*
** =====================================================================================
**  CLASSE Channel  —  lo STATO di un canale IRC (es. "#general")
** =====================================================================================
**  Contiene SOLO dati e regole del canale, non la logica dei comandi:
**   - membri (in ordine di ingresso: serve per promuovere il "prossimo" operatore);
**   - operatori (chi può fare KICK / INVITE / MODE / TOPIC con +t);
**   - inviti pendenti (per entrare in un canale +i);
**   - i modi richiesti dal subject:
**       i = invite-only        t = topic modificabile solo dagli operatori
**       k = chiave (password)  l = limite di utenti      o = dare/togliere operatore
**  I COMANDI (JOIN, KICK, MODE...) sono negli handler del Server, che usano questi metodi.
**
**  Chi crea / distrugge i Channel: SOLO il Server (createChannel / removeChannelIfEmpty).
**  Il Channel NON possiede i Client: contiene solo puntatori a client posseduti dal Server.
** =====================================================================================
*/
class Channel
{
public:
    // ---------------------------------------------------------------- costruzione / distruzione
    explicit Channel(const std::string &name);  // explicit: niente conversioni implicite string -> Channel
    ~Channel();                                 // non libera i Client (non sono suoi)

    // ---------------------------------------------------------------- getter
    const std::string   &getName() const;       // nome del canale ("#general")
    const std::string   &getTopic() const;      // argomento ("" se non impostato)
    const std::string   &getKey() const;        // password del canale ("" se non c'è +k)
    int                 getLimit() const;       // limite utenti (0 = nessun limite)
    std::string         getModes() const;       // es. "+itk segreto" per la risposta 324
    bool                isInviteOnly() const;   // modo +i attivo?
    bool                isTopicRestricted() const; // modo +t attivo?
    bool                hasKey() const;         // modo +k attivo?
    bool                isFull() const;         // modo +l attivo e limite raggiunto?

    // ---------------------------------------------------------------- setter (modi)
    void setTopic(const std::string &topic);    // TOPIC
    void setKey(const std::string &key);        // MODE +k / -k ("" = toglie la chiave)
    void setLimit(int limit);                   // MODE +l / -l (0 = toglie il limite)
    void setInviteOnly(bool inviteOnly);        // MODE +i / -i
    void setTopicRestricted(bool restricted);   // MODE +t / -t

    // ---------------------------------------------------------------- membri
    void                    addClient(Client &client);      // ingresso (il primo diventa operatore)
    void                    removeClient(Client &client);   // uscita (PART / KICK / QUIT)
    bool                    hasClient(Client &client) const; // è membro?
    std::vector<Client *>   getClients() const;             // copia della lista dei membri
    int                     getClientCount() const;         // numero di membri
    bool                    isEmpty() const;                // nessun membro -> il Server lo cancella

    // ---------------------------------------------------------------- operatori
    void addOperator(Client &client);           // MODE +o
    void removeOperator(Client &client);        // MODE -o
    bool isOperator(Client &client) const;      // ha i privilegi?

    // ---------------------------------------------------------------- inviti
    void addInvite(Client &client);             // INVITE
    bool isInvited(Client &client) const;       // può entrare se +i?
    void removeInvite(Client &client);          // invito consumato o membro uscito

    // ---------------------------------------------------------------- invio messaggi
    void broadcast(const std::string &msg, Client *exclude = NULL); // a tutti i membri (tranne exclude)
    void sendTopic(Client &client);             // 332 RPL_TOPIC oppure 331 RPL_NOTOPIC
    void sendNames(Client &client);             // 353 RPL_NAMREPLY + 366 RPL_ENDOFNAMES

private:
    // Copia vietata: due Channel con gli stessi puntatori renderebbero incoerenti i Client.
    Channel(const Channel &other);
    Channel &operator=(const Channel &other);

    std::string             name;               // nome (con # o &)
    std::string             topic;              // argomento
    std::string             key;                // chiave (+k)
    int                     limit;              // limite utenti (+l), 0 = nessuno
    bool                    inviteOnly;         // +i
    bool                    topicRestricted;    // +t
    std::vector<Client *>   clients;            // membri in ordine di ingresso
    std::set<Client *>      operators;          // sottoinsieme dei membri con privilegi
    std::set<Client *>      invites;            // client invitati non ancora entrati
};

#endif                                          // fine include guard
