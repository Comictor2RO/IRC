# ft_irc — parte di Youssef: funzionamento e test

## 0. Struttura della cartella

```
youssef_server_client_channel/
├── Server/   Server.hpp  Server.cpp     ← socket, poll(), accept, recv/send, dispatch, pulizia
├── Client/   Client.hpp  Client.cpp     ← stato di UNA connessione, buffer in/out, registrazione
├── Channel/  Channel.hpp Channel.cpp    ← stato di UN canale: membri, operatori, inviti, modi i/t/k/l
├── TEST_E_FUNZIONAMENTO.md              ← questo file
└── _DA_ELIMINARE_codice_altri/          ← codice degli altri due, SOLO per compilare e testare
    ├── Makefile                         ← compila tutto in ./ircserv
    ├── main.cpp                         ← controllo argomenti + Server::run()
    ├── IrcParser/                       ← riga di testo → IrcMessage
    └── Commands/                        ← gli handler: Registration / ChannelCommands / Messaging
```

**Quando i file verranno integrati nel progetto del gruppo:** si copiano `Server/`, `Client/` e `Channel/`, si cancella `_DA_ELIMINARE_codice_altri/` e nel Makefile del gruppo si aggiungono `-I` per le cartelle (i `.cpp` usano `#include "Client.hpp"` senza percorsi relativi). Gli handler del gruppo devono:
- cambiare nick con `updateNick()`, mai con `client->setNickname()`;
- chiamare `removeChannelIfEmpty()` dopo PART e KICK;
- non chiamare mai `::send()` o `close()` direttamente.

## 1. Compilare e avviare

```bash
cd youssef_server_client_channel/_DA_ELIMINARE_codice_altri
make                       # -Wall -Wextra -Werror -std=c++98, nessun warning
./ircserv 6667 pass        # porta 1-65535, password non vuota e senza spazi
```

| Comando | Output atteso |
|---|---|
| `./ircserv` | `Usage: ./ircserv <port> <password>` (exit 1) |
| `./ircserv abc pass` / `./ircserv 70000 pass` | `Error: invalid port (1-65535)` |
| `./ircserv 6667 ""` | `Error: invalid password (empty or contains spaces)` |
| `./ircserv 6667 pass` lanciato due volte | il secondo stampa `Error: bind: Address already in use` |
| `./ircserv 80 pass` (senza root) | `Error: bind: Permission denied` sulla maggior parte dei sistemi (sui Mac di 42, per esempio). Su Kali parte lo stesso, perché `net.ipv4.ip_unprivileged_port_start = 0` |

Sul terminale del server ogni riga ricevuta compare come `[fd 4] >> NICK alice`, insieme ai log di connessione e disconnessione.

---

## 2. Come funziona l'intero processo di un comando

Esempio: alice, già registrata e dentro `#test`, scrive `PRIVMSG #test :ciao`.

```
 client alice                         SERVER (un solo processo, un solo thread)
 ────────────                         ─────────────────────────────────────────
 "PRIVMSG #test :ciao\r\n" ──TCP──►  ① run(): buildPollFds() prepara [server_fd, fd_alice, fd_bob...]
                                      ② poll() si sveglia: fd_alice ha POLLIN
                                      ③ handlePollEvents() → readFromClient(alice)
                                           recv() → 22 byte → alice->appendToBuffer()
                                      ④ alice->extractLine() → "PRIVMSG #test :ciao"
                                           (senza \r\n; se la riga è incompleta aspetta il prossimo recv)
                                      ⑤ IrcParser::parse() → {command:"PRIVMSG", params:["#test"],
                                                               trailing:"ciao", hasTrailing:true}
                                      ⑥ dispatchCommand(): comando conosciuto? registrata? → handlePrivmsg
                                      ⑦ handlePrivmsg → deliverMessage → channel->broadcast(msg, alice)
                                           → bob->send(":alice!al@127.0.0.1 PRIVMSG #test :ciao")
                                             (ACCODA nell'outBuffer di bob, NON spedisce)
                                      ⑧ cleanupClients(): nessuno da rimuovere
                                      ⑨ giro successivo: buildPollFds() vede la coda di bob → POLLOUT
                                      ⑩ poll(): fd_bob scrivibile → writeToClient(bob)
 client bob  ◄──TCP── ":alice!al@...PRIVMSG #test :ciao\r\n"   ← bob->flushOutput() fa UNA send()
```

Regole del subject che questo schema rispetta:

| Regola | Dove |
|---|---|
| Un solo `poll()` per tutti i fd | `Server::run()` |
| Tutti i fd non bloccanti con `fcntl(fd, F_SETFL, O_NONBLOCK)` | `setupSocket()`, `acceptNewClient()` |
| Mai `recv`/`send` senza che `poll()` abbia segnalato il fd | `readFromClient()` solo su POLLIN, `flushOutput()` solo su POLLOUT |
| Niente `errno` dopo `recv`/`send` | si guarda solo il valore di ritorno |
| Comandi spezzati e incollati | `appendToBuffer()` + `extractLine()` |
| Nessun `fork` | — |

**Ciclo di vita di un client:**

```
accept() → new Client (clientsByFD)
  → PASS ok → auth
  → NICK → updateNick (clientsByNick)
  → USER → tryRegister → 001..005, 422
  → … comandi …
  → QUIT / password errata / recv()==0 / POLLHUP / errore send
      → markForDeletion(motivo)            (NON si cancella subito)
      → (se ha messaggi in coda, es. "ERROR", aspetta il POLLOUT che li spedisce)
      → cleanupClients() a fine giro → removeClient():
           QUIT agli altri (una volta per persona) → esce dai canali → canali vuoti distrutti
           → tolto da clientsByNick e clientsByFD → delete (il distruttore fa close(fd))
```

**Spegnimento:** Ctrl+C manda SIGINT → `signalHandler` imposta `stopSignal = 1` → `poll()` ritorna -1 con EINTR → il loop termina → `~Server()` cancella client e canali e chiude `server_fd`. SIGPIPE è ignorato: un client che sparisce mentre gli scriviamo non fa crashare il server.

---

## 3. Test con `nc`

Usare **`nc -C 127.0.0.1 6667`**: `-C` manda `\r\n` come terminatore, come un vero client IRC. Senza `-C` arriva solo `\n`, che il server accetta lo stesso. Le righe che iniziano con `>` sono quelle da scrivere, le altre sono la risposta attesa. Gli output qui sotto sono stati **catturati da un'esecuzione reale**.

### 3.1 Registrazione corretta
```
> PASS pass
> NICK alice
> USER al 0 * :Alice Rossi
:ircserv 001 alice :Welcome to the ft_irc Network alice!al@127.0.0.1
:ircserv 002 alice :Your host is ircserv, running version 1.0
:ircserv 003 alice :This server was created for the 42 ft_irc project
:ircserv 004 alice ircserv 1.0 o itkol
:ircserv 005 alice CHANTYPES=#& PREFIX=(o)@ CHANMODES=,k,l,it NICKLEN=30 :are supported by this server
:ircserv 422 alice :MOTD File is missing
```
Se prima si manda `CAP LS 302` (come fanno irssi e HexChat), arriva in più `:ircserv CAP * LS :`. I comandi sono accettati anche in minuscolo (`nick alice`).

### 3.2 Password sbagliata → disconnessione
```
> PASS sbagliata
:ircserv 464 * :Password incorrect
ERROR :Closing Link: 127.0.0.1 (Password incorrect)
```
`nc` termina: il server chiude la connessione dopo aver spedito l'ERROR.

### 3.3 NICK + USER senza PASS
```
> NICK nopass
> USER u 0 * :u
:ircserv 464 nopass :Password required
ERROR :Closing Link: 127.0.0.1 (Password required)
```

### 3.4 Comandi prima della registrazione
```
> JOIN #a
:ircserv 451 * :You have not registered
> PRIVMSG a :ciao
:ircserv 451 * :You have not registered
> FOO
:ircserv 451 * :You have not registered
```

### 3.5 Errori di NICK (alice è già connessa)
```
> PASS pass
> NICK
:ircserv 431 * :No nickname given
> NICK 1abc
:ircserv 432 * 1abc :Erroneous nickname
> NICK ALICE
:ircserv 433 * ALICE :Nickname is already in use      ← maiuscole e minuscole non contano
> NICK bob
> USER b 0 * :Bob
:ircserv 001 bob :Welcome ...   (e il resto del benvenuto)
```
Dopo la registrazione: `PASS x` o `USER ...` danno `462 :You may not reregister`.

### 3.6 Il test del subject: comando spezzato (Ctrl+D)
`nc` spedisce quello che hai scritto quando premi **Ctrl+D**, anche senza a capo:
```
> PA        [Ctrl+D]
> SS pa     [Ctrl+D]
> ss        [Invio]
```
Il server non risponde finché la riga non è completa, poi la tratta come `PASS pass`. Lo stesso vale per `NICK`/`USER` spezzati.
In un test automatico, la sequenza `"PA"` / `"SS pa"` / `"ss\r\nNICK fr"` / `"ag\nUSER f 0 * :F\r\nPING :tok\r\n"`, spedita in 4 `send()` separate, ha prodotto il benvenuto completo per `frag` e poi `:ircserv PONG ircserv :tok`.

### 3.7 Più comandi in un solo pacchetto
```bash
printf 'ping :uno\r\nPING :due\nmode #test\r\n'      # un solo send() da parte del client
```
```
:ircserv PONG ircserv :uno
:ircserv PONG ircserv :due
:ircserv 324 alice #test +
```

Per i test dal 3.8 in poi servono **più terminali**, ognuno con il suo `nc -C 127.0.0.1 6667` già registrato (alice, bob, frag, carol, dave).

### 3.8 JOIN e lista utenti
```
alice> JOIN #test
alice: :alice!al@127.0.0.1 JOIN #test
       :ircserv 353 alice = #test :@alice                ← il creatore è operatore (@)
       :ircserv 366 alice #test :End of /NAMES list
bob>   join #test
bob:   :bob!b@127.0.0.1 JOIN #test
       :ircserv 353 bob = #test :@alice bob
       :ircserv 366 bob #test :End of /NAMES list
alice: :bob!b@127.0.0.1 JOIN #test                       ← gli altri vedono l'ingresso
```
`JOIN test` (senza #) → `476 test :Bad Channel Mask`. `JOIN #a,#b` entra in entrambi.

### 3.9 PRIVMSG
```
alice> PRIVMSG #test :ciao a tutti
bob:   :alice!al@127.0.0.1 PRIVMSG #test :ciao a tutti   (alice NON lo riceve indietro)
bob>   PRIVMSG alice :ciao privato
alice: :bob!b@127.0.0.1 PRIVMSG alice :ciao privato
bob>   PRIVMSG nessuno :x      → :ircserv 401 bob nessuno :No such nick/channel
bob>   PRIVMSG #nonc :x        → :ircserv 403 bob #nonc :No such channel
bob>   PRIVMSG alice           → :ircserv 412 bob :No text to send
bob>   PRIVMSG                 → :ircserv 411 bob :No recipient given (PRIVMSG)
frag>  PRIVMSG #test :x        → :ircserv 404 frag #test :Cannot send to channel   (frag non è nel canale)
```
NOTICE funziona allo stesso modo ma non risponde mai con errori.

### 3.10 Un utente normale non può usare i comandi da operatore
```
bob> KICK #test alice :via   → :ircserv 482 bob #test :You're not channel operator
bob> MODE #test +i           → :ircserv 482 bob #test :You're not channel operator
bob> INVITE frag #test       → :ircserv 482 bob #test :You're not channel operator
```

### 3.11 TOPIC e modo +t
```
bob>   TOPIC #test :topic di bob          (senza +t chiunque nel canale può cambiarlo)
tutti: :bob!b@127.0.0.1 TOPIC #test :topic di bob
alice> MODE #test +t
tutti: :alice!al@127.0.0.1 MODE #test +t
bob>   TOPIC #test :di nuovo  → :ircserv 482 bob #test :You're not channel operator
bob>   TOPIC #test            → :ircserv 332 bob #test :topic di bob          (leggerlo è sempre permesso)
alice> TOPIC #test :          → topic cancellato; poi "TOPIC #test" → 331 #test :No topic is set
```

### 3.12 MODE +i e INVITE
```
alice> MODE #test +i              → tutti: :alice!al@127.0.0.1 MODE #test +i
frag>  JOIN #test                 → :ircserv 473 frag #test :Cannot join channel (+i)
alice> INVITE frag #test          → alice: :ircserv 341 alice frag #test
                                    frag:  :alice!al@127.0.0.1 INVITE frag :#test
frag>  JOIN #test                 → :frag!f@127.0.0.1 JOIN #test
                                    :ircserv 332 frag #test :topic di bob
                                    :ircserv 353 frag = #test :@alice bob frag
                                    :ircserv 366 frag #test :End of /NAMES list
```
`INVITE bob #test` (bob è già dentro) → `443 bob #test :is already on channel`.

### 3.13 MODE +k (chiave)
```
alice> MODE #test -i+k segreto    → tutti: :alice!al@127.0.0.1 MODE #test -i+k segreto
carol> JOIN #test                 → :ircserv 475 carol #test :Cannot join channel (+k)
carol> JOIN #test sbagliata       → :ircserv 475 carol #test :Cannot join channel (+k)
carol> JOIN #test segreto         → :carol!carol@127.0.0.1 JOIN #test  (+ 332, 353, 366)
alice> MODE #test +k              → :ircserv 461 alice MODE :Not enough parameters
```

### 3.14 MODE +l (limite)
```
alice> MODE #test -k+l 4          → tutti: :alice!al@127.0.0.1 MODE #test -k+l 4
dave>  JOIN #test                 → :ircserv 471 dave #test :Cannot join channel (+l)   (già 4 membri)
alice> MODE #test                 → :ircserv 324 alice #test +tl 4
```
`MODE #test +l abc` o `+l 0` vengono ignorati; `MODE #test -l` toglie il limite.

### 3.15 MODE +o e KICK
```
alice> MODE #test +o bob          → tutti: :alice!al@127.0.0.1 MODE #test +o bob
bob>   KICK #test frag :spam      → tutti (frag compreso): :bob!b@127.0.0.1 KICK #test frag :spam
alice> MODE #test -o bob          → tutti: :alice!al@127.0.0.1 MODE #test -o bob
alice> MODE #test +o nessuno      → :ircserv 401 alice nessuno :No such nick/channel
alice> MODE #test +x              → :ircserv 472 alice x :is unknown mode char to me
alice> KICK #test zed             → 401 (non esiste) | 441 se esiste ma non è nel canale
```
Se l'ultimo operatore esce, il membro più vecchio diventa operatore e tutti ricevono `:ircserv MODE #test +o <nick>`.

### 3.16 PART, cambio nick, QUIT
```
carol> PART #test :ciao           → tutti (carol compresa): :carol!carol@127.0.0.1 PART #test :ciao
bob>   NICK roberto               → bob e chi condivide un canale: :bob!b@127.0.0.1 NICK :roberto
bob>   QUIT :a dopo               → bob:   ERROR :Closing Link: 127.0.0.1 (Quit: a dopo)   e la connessione si chiude
                                    alice: :roberto!b@127.0.0.1 QUIT :Quit: a dopo
```

### 3.17 Disconnessione brutale (Ctrl+C su nc)
```
alice: :carol!carol@127.0.0.1 QUIT :Connection closed
server: [server] disconnesso fd=7 (Connection closed)
```
Il server continua a funzionare per tutti gli altri.

### 3.18 Canale distrutto quando si svuota
Se tutti fanno PART, il canale viene cancellato. Il JOIN successivo lo ricrea da zero (chi entra è operatore, niente topic e niente modi):
```
alice> JOIN #test   → :ircserv 353 alice = #test :@alice
```

### 3.19 WHO e PING
```
alice> WHO #test
:ircserv 352 alice #test al 127.0.0.1 ircserv alice H@ :0 Alice Rossi
:ircserv 352 alice #test dave 127.0.0.1 ircserv dave H :0 Real dave
:ircserv 315 alice #test :End of WHO list
alice> PING 123
:ircserv PONG ircserv :123
```

### 3.20 Riga più lunga di 510 caratteri
```bash
python3 -c 'print("PRIVMSG #test :" + "x"*600)'   # incollala in nc
```
```
:ircserv 417 alice :Input line was too long
```
Viene scartata tutta la riga, compresa la parte arrivata con il `recv()` successivo, e il comando dopo funziona normalmente.

### 3.21 Comando sconosciuto
```
alice> BOH   → :ircserv 421 alice BOH :Unknown command
```

---

## 4. Test con il client di riferimento (irssi o HexChat)

```bash
irssi
/connect 127.0.0.1 6667 pass alice     # in irssi la password è il 3° argomento
/join #test
/msg #test ciao
/topic #test nuovo topic
/mode #test +i          /mode #test +k segreto     /mode #test +l 5     /mode #test +o bob
/invite bob #test
/kick #test bob motivo
/part #test
/nick nuovo
/quit
```
Cosa aspettarsi:
- nessun errore dopo la connessione: CAP, PING, `MODE alice +i`, `MODE #test` e `MODE #test b` hanno tutti una risposta;
- la lista utenti mostra `@` accanto agli operatori;
- i messaggi arrivano in tutte le finestre.

In HexChat: aggiungi una rete `127.0.0.1/6667`, imposta la password del server e disattiva SSL.

---

## 5. Robustezza, memoria, file descriptor

| Test | Come | Atteso |
|---|---|---|
| Leak e fd | `valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./ircserv 6667 pass`, poi qualche client, poi **Ctrl+C con i client ancora connessi** | `All heap blocks were freed -- no leaks are possible` e `FILE DESCRIPTORS: 3 open (3 inherited)` (solo stdin/out/err). **Verificato.** |
| fd durante l'esecuzione | `ls /proc/$(pidof ircserv)/fd \| wc -l` prima, durante e dopo la connessione di N client | il numero torna a quello iniziale dopo le disconnessioni |
| CPU a riposo | `top -p $(pidof ircserv)` | 0%: `poll()` dorme con timeout -1 |
| Tanti client | `for i in $(seq 100); do (printf 'PASS pass\r\nNICK u%s\r\nUSER u 0 * :u\r\nJOIN #a\r\n' $i; sleep 5) \| nc 127.0.0.1 6667 >/dev/null & done` | tutti registrati; il server resta reattivo |
| Client lento | un client entra in un canale e **non legge mai**; un altro manda 3000 messaggi da 400 byte | il server non si blocca e un terzo client si registra subito. Il client lento, quando inizia a leggere, riceve tutti i circa 1,3 MB. **Verificato.** |
| Client sospeso | `nc` in un canale, poi **Ctrl+Z** (processo sospeso); gli altri continuano a scrivere; poi `fg` | il server non si blocca; dopo `fg` arrivano i messaggi accumulati |
| Comando spezzato | 3.6 | risposta solo a riga completa |
| Morte improvvisa | `kill -9` su un `nc` | QUIT `Connection closed` agli altri, nessun crash (SIGPIPE ignorato) |
| Riavvio immediato | Ctrl+C e rilancio subito sulla stessa porta | parte senza "Address already in use" (SO_REUSEADDR) |

---

## 6. Domande probabili alla valutazione (risposte brevi)

- **Perché `pollFds` viene ricostruito a ogni giro?** Una sola fonte di verità (`clientsByFD`). Non si fa mai `erase` sul vettore mentre lo si scorre, quindi non ci sono indici sfasati.
- **Perché i client si segnano e non si cancellano subito?** Durante `handlePollEvents()` si sta scorrendo `pollFds` e un altro client potrebbe avere ancora un puntatore a quello eliminato. Si cancella a fine giro, in `cleanupClients()`.
- **Perché una coda di uscita?** Il subject vieta `send()` senza `poll()`. Inoltre su un socket non bloccante una `send()` può spedire solo una parte dei byte: il resto resta in coda e parte al POLLOUT successivo.
- **Chi chiude i fd?** Il fd di un client lo chiude solo `~Client()`, `server_fd` solo `~Server()`. Così non ci sono mai doppie chiusure.
- **Perché `volatile sig_atomic_t`?** È l'unico tipo che si può scrivere in modo sicuro da un signal handler.
- **Perché `clientsByNick` usa chiavi in minuscolo?** In IRC i nick non distinguono maiuscole e minuscole: `Alice` e `alice` sono la stessa persona.
