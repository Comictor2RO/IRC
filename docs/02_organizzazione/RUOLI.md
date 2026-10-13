# ft_irc — Divisione dei compiti

Ognuno è responsabile della propria parte e deve saperla spiegare riga per riga in valutazione.
Le parti in comune (sezione finale) le seguiamo tutti e tre.

---

## Youssef — Server, Client, Channel (classi e stato)

Le tre classi che tengono **i dati** del server e le regole su chi possiede cosa.

**File:** `Server/Server.hpp`, `Server/Server.cpp` (parte di gestione), `Client/`, `Channel/`

- **Server: gestione di client e canali**
  - `clientsByFD` (fd → Client, proprietario), `clientsByNick` (nick in minuscolo → Client, solo indice), `channels` (nome in minuscolo → Channel, proprietario)
  - `getClientByFd`, `getClientByNick`, `isNickTaken`, `updateNick`
  - `getChannel`, `createChannel`, `removeChannelIfEmpty`
  - `removeClient` (QUIT agli altri membri, uscita dai canali, delete) e `cleanupClients`
  - `initCommands` + `dispatchCommand` (tabella comando → handler, 421 / 451)
  - distruttore: nessun leak e nessun fd aperto
- **Client**
  - identità: nickname, username, realname, hostname, `getPrefix()` (`nick!user@host`)
  - registrazione: `isAuth`, `isRegistered`, `tryRegister` (001–005 + 422)
  - risposte: `send`, `sendReply`, `sendError`
  - ciclo di vita: `markForDeletion(motivo)`, `shouldDelete`, `getQuitReason`
  - lista dei canali a cui è iscritto
- **Channel**
  - membri, operatori, inviti
  - modi `i`, `t`, `k`, `l`, `o` e `getModes()` (risposta 324)
  - `broadcast`, `sendTopic` (332/331), `sendNames` (353/366)
  - passaggio automatico dell'operatore quando l'ultimo operatore esce

---

## Vlad — Parser e comandi

Tutto ciò che trasforma una riga di testo in un'azione IRC.

**File:** `IrcParser/`, `Commands/`

- **IrcParser**
  - `[:prefix] COMMAND [params...] [:trailing]` → `IrcMessage`
  - comando in maiuscolo, spazi multipli, `hasTrailing`
- **Registrazione** (`Commands/Registration.cpp`): PASS, NICK, USER, CAP, PING, PONG, QUIT
  - validazione del nick (431 / 432 / 433), password sbagliata → 464 + chiusura
- **Messaggi** (`Commands/Messaging.cpp`): PRIVMSG, NOTICE
  - più destinatari, canali `#` e `&`, errori 401 / 403 / 404 / 411 / 412 (mai per NOTICE)
- **Canali** (`Commands/ChannelCommands.cpp`): JOIN, PART, TOPIC, KICK, INVITE, MODE, WHO
  - comandi da operatore richiesti dal subject: KICK, INVITE, TOPIC (con +t), MODE `i t k o l`
  - codici di errore corretti per ogni caso (461, 403, 442, 482, 441, 443, 471, 473, 475, 476, 472)

---

## Gabry — Networking

Tutto ciò che riguarda socket, `poll()` e il passaggio dei byte tra rete e buffer.

**File:** `Server/Server.cpp` (parte di rete), `Client/Client.cpp` (buffer), `main.cpp`

- **Avvio**
  - `main`: controllo degli argomenti (porta 1–65535, password non vuota), `try/catch`
  - `setupSocket`: `socket` → `SO_REUSEADDR` → `fcntl(O_NONBLOCK)` → `bind` → `listen`
  - `setupSignals`: SIGINT / SIGQUIT → spegnimento pulito, SIGPIPE ignorato
- **Loop principale**
  - `run`: **un solo `poll()`** per iterazione per ascolto + tutti i client
  - `buildPollFds`: POLLIN per i client attivi, POLLOUT solo se c'è qualcosa in coda
  - `handlePollEvents`: POLLIN / POLLOUT / POLLHUP / POLLERR
  - `acceptNewClient`: `accept` + `O_NONBLOCK` + `new Client`
- **I/O**
  - `readFromClient`: una `recv()`, niente `errno` dopo `recv`/`send`
  - buffer di ingresso: `appendToBuffer`, `extractLine` (`\r\n` e `\n`), righe > 510 → 417, `discardLongLine`
  - buffer di uscita: `send()` solo con POLLOUT (`flushOutput`, `hasPendingOutput`, `discardOutput`)
  - comandi spezzati (`nc` + `Ctrl+D`) e più comandi nello stesso pacchetto

---

## In comune

- **Test** con il client di riferimento (irssi / HexChat) e con `nc -C 127.0.0.1 <porta>`
- **valgrind** `--leak-check=full --track-fds=yes`: 0 leak, nessun fd aperto
- **README.md** richiesto dal subject: Description, Instructions, Resources (con la parte su come è stata usata l'IA)
- **Makefile**: `all`, `clean`, `fclean`, `re`, nessun relink, `-Wall -Wextra -Werror -std=c++98`
- Prima della valutazione ognuno spiega la propria parte agli altri due
