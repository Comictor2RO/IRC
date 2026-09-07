# ft_irc — Spiegazione dettagliata del subject

> Documento di supporto (non richiesto dalla scuola) per capire a fondo cosa chiede il subject `ft_irc.subject.pdf` (versione **10.0**), i concetti tecnici nuovi che introduce e le parti "insolite" rispetto ad altri progetti del curriculum 42.

---

## 1. Scopo del progetto

Il progetto chiede di **scrivere da zero un server IRC** (Internet Relay Chat) in C++98, senza usare librerie esterne (Boost incluso).

Punti chiave dello scopo dichiarato nel subject:

- **Non va scritto un client**: userete un client IRC esistente (es. `irssi`, `WeeChat`, `HexChat`) come "reference client" per collegarvi al vostro server e testarlo.
- **Non va implementata la comunicazione server-to-server**: nella vita reale i server IRC si collegano fra loro per formare una rete (es. Libera.Chat, che è composta da decine di server collegati). Qui il vostro server è **isolato**, parla solo con i client.
- L'obiettivo pedagogico dichiarato è capire **i protocolli di rete standard** che permettono a computer diversi di comunicare: IRC è un protocollo relativamente semplice (testuale, riga per riga) che è un ottimo caso di studio per imparare socket TCP, gestione di connessioni multiple e parsing di un protocollo testuale.

In pratica il progetto vi obbliga a lavorare su tre livelli contemporaneamente:
1. **Rete di basso livello**: socket POSIX, TCP/IP, I/O non bloccante.
2. **Un protocollo applicativo testuale** (IRC, RFC 1459 / 2812): parsing di comandi, gestione di stato (utenti, canali, permessi).
3. **Architettura software in C++98**: niente `std::thread`, niente `fork()`, tutto va gestito con un solo ciclo di eventi.

---

## 2. Cos'è IRC (concetti di dominio)

Il subject dà solo una definizione minima ("Internet Relay Chat is a text-based communication protocol"), ma per implementarlo bene serve capire questi concetti:

- **Client**: un programma che si connette al server e rappresenta un utente (con un nickname, uno username, un realname).
- **Canale (channel)**: una "stanza" identificata da un nome che inizia tipicamente con `#`. I messaggi mandati al canale vengono inoltrati (forward) a tutti i membri.
- **Nickname vs Username**: il nickname (`NICK`) è il nome visibile e mutabile usato per identificare l'utente durante la sessione; lo username (`USER`) è più simile a un "account id" fissato alla registrazione.
- **Registrazione (registration)**: un client deve completare una sequenza di comandi (`PASS`, `NICK`, `USER`) prima di essere considerato "registrato" e poter operare normalmente. Finché non è registrato, il server deve rifiutare o rimandare la maggior parte degli altri comandi.
- **Operatore di canale (channel operator)**: un utente con privilegi speciali su un canale specifico (non va confuso con un operatore IRC globale, che questo progetto non richiede). Di default il primo utente che crea un canale ne diventa operatore.
- **Modalità di canale (channel modes)**: flag che cambiano il comportamento del canale. Il subject richiede solo:
  - `i` — invite-only (si entra solo se invitati)
  - `t` — topic-restricted (solo gli operatori possono cambiare il topic)
  - `k` — key/password del canale
  - `o` — dare/togliere lo stato di operatore a un utente
  - `l` — limite massimo di utenti nel canale
- **Messaggi privati vs messaggi di canale**: `PRIVMSG` può avere come destinatario sia un nickname (messaggio privato) sia un nome di canale (broadcast a tutti i membri).
- **Codici di risposta numerici**: IRC non risponde con testo libero ma con **codici a 3 cifre** standardizzati (RFC 2812), es.:
  - `001`-`004`: messaggi di benvenuto dopo la registrazione (RPL_WELCOME, ecc.)
  - `332`/`331`: RPL_TOPIC / RPL_NOTOPIC
  - `353`/`366`: RPL_NAMREPLY / RPL_ENDOFNAMES (lista utenti di un canale)
  - `4xx`: codici di errore (es. `461 ERR_NEEDMOREPARAMS`, `433 ERR_NICKNAMEINUSE`)

  Questi codici **non sono un'invenzione vostra**: un client IRC reale (il "reference client" richiesto) li interpreta in modo specifico, quindi vanno rispettati nel formato esatto per far funzionare l'integrazione.

Il subject **non elenca** tutti questi dettagli (codici, formato esatto dei messaggi): li dovete recuperare dagli RFC (1459/2812) o dalla documentazione del client scelto come riferimento — è lavoro di ricerca implicito nel progetto.

---

## 3. Regole generali (Capitolo II) — cosa cambia rispetto ad altri progetti 42

- **Il programma non deve MAI crashare**, nemmeno in caso di out-of-memory. Se crasha durante la valutazione, il voto è **0**, punto. Questo è più severo della norma "no leak" vista in altri progetti: qui si parla di **robustezza totale a runtime**, quindi ogni `new`, ogni accesso a mappa, ogni parsing di input utente deve essere difensivo (l'input di un client IRC è dati non fidati che arrivano dalla rete).
- **Makefile** con le regole standard `$(NAME)`, `all`, `clean`, `fclean`, `re`, senza relink inutili.
- **Flag di compilazione**: `-Wall -Wextra -Werror`, standard **C++98** (deve compilare anche forzando `-std=c++98`). Questo esclude `auto`, lambda, `nullptr`, `std::thread`, smart pointer moderni, range-based for, ecc. — tutte cose disponibili solo da C++11 in poi.
- **Preferire le versioni C++ delle funzioni C** quando possibile (es. `<cstring>` invece di `<string.h>`).
- **Vietate librerie esterne e Boost.**

---

## 4. La vera novità: Capitolo III "AI Instructions"

Questa è la parte **più insolita** rispetto ai subject "classici" di 42: un intero capitolo dedicato a come è lecito usare l'IA (come me) durante lo sviluppo. Non era presente nelle versioni storiche del curriculum: è un'aggiunta recente legata al fatto che ora l'uso di IA è esplicitamente regolamentato invece che ignorato o vietato implicitamente.

Punti sostanziali:

- **L'uso di IA è permesso e incoraggiato**, ma con uno scopo preciso: ridurre compiti ripetitivi/noiosi, allenare le proprie capacità di prompting, capire come funzionano i sistemi di IA (bias, rischi, limiti etici).
- **Non è permesso usare IA come sostituto della comprensione**: il subject è esplicito che dovrete **giustificare ogni riga di codice durante la difesa/peer-evaluation**. Se non sapete spiegare cosa fa una funzione o perché è scritta così, **fallite il progetto**, anche se il codice compila e funziona.
- **Esempi di buona pratica** dati dal subject:
  - Chiedere all'IA idee generali (es. "come testo una funzione di sorting?"), provarle, e discuterne con un peer.
  - Usare l'IA per progettare un parser, poi rivederne la logica con un compagno, trovare bug insieme e riscriverla capendola a fondo.
- **Esempi di cattiva pratica**:
  - Far generare una funzione intera all'IA e fare copia-incolla senza capirla: durante la peer-evaluation non riuscite a spiegarla → perdete credibilità e fallite.
  - Lasciare che uno strumento tipo Copilot generi una parte chiave del progetto senza saperne spiegare il funzionamento interno.
- **Il peer-review è centrale**: il subject insiste che i compagni di corso, che condividono il vostro contesto, sono una risorsa di validazione migliore dell'IA da sola, perché l'IA tende a dare risposte generiche "più probabili" mentre un peer può cogliere sfumature specifiche del vostro progetto.

**Implicazione pratica per voi**: qualunque codice che io vi aiuti a scrivere in questo repository, dovete essere in grado di spiegarlo a un valutatore riga per riga — architettura dei socket, gestione del buffer, motivo di ogni scelta — altrimenti in sede di difesa il progetto viene bocciato indipendentemente dal fatto che funzioni.

Un'altra novità collegata: il **README.md richiesto** (Capitolo V) deve avere una sezione **"Resources"** che descrive esplicitamente **come è stata usata l'IA**, per quali task e in quali parti del progetto — è un requisito di trasparenza che prima non esisteva nei subject 42.

---

## 5. Parte obbligatoria (Capitolo IV) — architettura tecnica

### 5.1 Interfaccia del programma

```
Program Name:     ircserv
Files:            Makefile, *.h/*.hpp, *.cpp, *.tpp, *.ipp, (opzionale) file di config
Esecuzione:       ./ircserv <port> <password>
```

- `port`: la porta TCP su cui il server ascolta le connessioni in ingresso.
- `password`: la password di connessione che ogni client dovrà fornire (comando `PASS`) per registrarsi.

### 5.2 Funzioni esterne autorizzate — spiegazione

Il subject elenca una lista chiusa di funzioni di sistema utilizzabili. Ecco cosa fa ciascuna e perché serve (molte già annotate in `docs.md`, qui ampliate):

| Funzione | A cosa serve nel contesto di ft_irc |
|---|---|
| `socket()` | Crea l'endpoint di comunicazione (il "telefono"): specifica famiglia (IPv4/IPv6), tipo (`SOCK_STREAM` = TCP) e protocollo. |
| `bind()` | Associa il socket del server a un indirizzo IP + porta locali, così il SO sa instradare lì i pacchetti in arrivo su quella porta. |
| `listen()` | Mette il socket in modalità "passiva": pronto ad accettare connessioni in ingresso, con una coda di connessioni pendenti. |
| `accept()` | Estrae una connessione dalla coda di `listen()` e crea **un nuovo socket dedicato** a quel client (il socket originale resta in ascolto per altri client). |
| `connect()` | Usata lato client per iniziare una connessione (nel vostro progetto non scrivete il client, ma la funzione resta autorizzata, es. per eventuali test). |
| `send()` / `recv()` | Scrivere/leggere byte su un socket già connesso. |
| `close()` | Chiude un file descriptor (socket) e libera le risorse associate. |
| `setsockopt()` / `getsockopt()` | Impostano/leggono opzioni del socket. La più usata qui è `SO_REUSEADDR` (via `SOL_SOCKET`): permette di riavviare subito il server sulla stessa porta senza aspettare lo stato `TIME_WAIT` del sistema operativo. |
| `getsockname()` | Recupera l'indirizzo locale (IP/porta) a cui un socket è collegato. |
| `getprotobyname()` | Traduce un nome di protocollo (es. `"tcp"`) nel suo identificativo numerico. |
| `gethostbyname()` / `getaddrinfo()` / `freeaddrinfo()` | Risolvono nomi host in indirizzi IP (DNS lookup) in modo "moderno" (`getaddrinfo`, che supporta anche IPv6) o "legacy" (`gethostbyname`). |
| `htons()`/`htonl()`/`ntohs()`/`ntohl()` | Convertono numeri tra **byte order della macchina (host)** e **byte order di rete (network, big-endian)**. Necessario perché macchine diverse possono avere endianness diversa, ma il protocollo di rete richiede un ordine fisso. |
| `inet_addr()` / `inet_ntoa()` / `inet_ntop()` | Convertono indirizzi IP fra rappresentazione testuale (`"127.0.0.1"`) e binaria, e viceversa. |
| `signal()` / `sigaction()` / `sigemptyset()` / `sigfillset()` / `sigaddset()` / `sigdelset()` / `sigismember()` | Gestione dei segnali Unix — tipicamente usate per intercettare `SIGINT` (Ctrl+C) e spegnere il server in modo pulito (chiudendo tutti i socket). |
| `lseek()` / `fstat()` | Operazioni sui file — utili soprattutto se implementate il bonus "file transfer". |
| `fcntl()` | Vedi sezione dedicata sotto: qui è **la funzione chiave** per l'I/O non bloccante. |
| `poll()` (o equivalente) | Vedi sezione dedicata sotto: è **il cuore dell'architettura** del server. |

### 5.3 Il vincolo più importante: un solo `poll()`, I/O non bloccante, niente fork

Questo è il concetto tecnico centrale e più nuovo del progetto (probabilmente il primo posto nel curriculum dove lo incontrate in questa forma):

- **Niente `fork()`**: non potete creare un processo per client. Un server IRC reale deve gestire migliaia di connessioni; con un processo/thread per client lo scaling e la sincronizzazione diventano complessi. Il progetto vi forza verso il modello **event-driven single-thread**, lo stesso usato da server reali ad alte prestazioni (nginx, Redis, node.js).
- **Un solo `poll()` (o `select()`/`epoll()`/`kqueue()`) per gestire TUTTI i descrittori**: sia il socket di ascolto (nuove connessioni) sia tutti i socket dei client già connessi (lettura/scrittura), tutto nello stesso ciclo di attesa. Non potete chiamare `poll()` ripetutamente per ogni singolo client dentro cicli separati: dev'essere **una singola chiamata per iterazione del loop principale** che monitora l'insieme di tutti i fd.
- **Perché è centrale**: `poll()` (o equivalenti) permette al processo di **dormire finché non c'è davvero qualcosa da fare** su uno qualsiasi dei descrittori monitorati (dato pronto in lettura, spazio libero in scrittura, nuova connessione in arrivo), invece di consumare CPU controllando ciclicamente ("busy polling" / polling attivo) o bloccarsi su una singola operazione impedendo di servire gli altri client.
- **I/O non bloccante obbligatorio** (`fcntl(fd, F_SETFL, O_NONBLOCK)`): senza questo, una singola `read()`/`accept()`/`connect()` lenta o "appesa" bloccherebbe l'intero server, impedendo di servire chiunque altro. Con `O_NONBLOCK`, se non c'è nulla da leggere/scrivere subito, la chiamata ritorna immediatamente (tipicamente con errore `EAGAIN`/`EWOULDBLOCK`) invece di attendere.
- **Trappola esplicitamente segnalata (voto 0 se ci si cade)**: è tecnicamente possibile scrivere codice che fa `read()`/`recv()`/`write()`/`send()` su un fd non bloccante **senza** passare da `poll()` prima. Funzionerebbe "per caso" (non si blocca), ma sprecherebbe risorse di sistema (continua tentativi a vuoto) e viola lo spirito del vincolo "un solo poll() per gestire tutto". Il subject dice esplicitamente: se lo fate, **voto 0**.

In sintesi il modello architetturale forzato è:

```
loop infinito:
    poll(tutti_i_fd)                    // si blocca qui finché non succede qualcosa
    per ogni fd con evento:
        se è il socket di ascolto → accept() nuova connessione, aggiungila al set monitorato
        se è un client pronto in lettura → recv(), accumula nel buffer, prova a parsare comandi completi
        se è un client pronto in scrittura (se avete dati in coda) → send()
        gestisci disconnessioni (POLLHUP/POLLERR) → rimuovi client, chiudi fd, libera memoria
```

Questo spiega perché nel vostro `milestones.md` la primissima fase pianificata è "Socket + Poll + Accept": letteralmente nulla si può testare finché questo loop non esiste.

### 5.4 Perché serve "riassemblare" i pacchetti (buffering)

Il subject dà un esempio di test con `nc`:

```
$> nc -C 127.0.0.1 6667
com^Dman^Dd
$>
```

Qui il comando `command\n` viene inviato **frammentato in tre invii separati** (`com`, poi `man`, poi `d\n`), premendo Ctrl+D tra un pezzo e l'altro.

Il concetto nuovo da capire: **TCP è uno stream di byte, non un protocollo a messaggi**. Non c'è garanzia che una `recv()` restituisca esattamente un comando IRC completo, né che un comando non venga spezzato su più `recv()` (o che più comandi arrivino in un'unica `recv()`). Per questo:

- Ogni client deve avere un **buffer di ricezione persistente** (accumulo tra una `recv()` e l'altra — è esattamente il campo/metodo `appendToBuffer()` già previsto nella vostra `Client` in `Yousef.md`).
- Solo quando nel buffer compare un **terminatore di riga completo** (IRC usa `\r\n`, ma per tolleranza spesso si accetta anche solo `\n`) si estrae e processa un comando, lasciando il resto nel buffer per il giro successivo.
- Bisogna gestire input "sporco": righe vuote, comandi senza `\r`, più comandi accodati nello stesso pacchetto, ecc. Il subject dice esplicitamente: *"Verify every possible error and issue, such as receiving partial data, low bandwidth, etc."*

### 5.5 Caso speciale MacOS

Su MacOS, `write()` sui socket non si comporta in modo identico agli altri Unix. Il subject permette, **solo su MacOS**, un uso estremamente ristretto di `fcntl()`:

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
```

Qualunque altro flag o uso di `fcntl()` oltre a questo è vietato su MacOS. Su Linux questo vincolo aggiuntivo non si applica (ma l'uso di `fcntl()` per `O_NONBLOCK` resta comunque necessario ovunque, essendo il meccanismo stesso dell'I/O non bloccante).

### 5.6 Funzionalità funzionali richieste (cosa deve fare il server)

Riassunto dei comandi obbligatori, con collegamento ai concetti IRC visti al punto 2:

| Comando | Funzione |
|---|---|
| `PASS` | Autentica la connessione con la password passata come argomento al programma. |
| `NICK` | Imposta/cambia il nickname del client (deve essere univoco sul server). |
| `USER` | Imposta username e realname, completa la registrazione insieme a `PASS`+`NICK`. |
| `JOIN` | Entra in un canale (crea il canale se non esiste). |
| `PRIVMSG` | Invia un messaggio privato a un utente o pubblico a un canale (inoltrato a tutti i membri). |
| `TOPIC` | Legge o imposta l'argomento del canale (soggetto a restrizione se il modo `t` è attivo). |
| `MODE` | Cambia le impostazioni del canale (`i`, `t`, `k`, `o`, `l` — vedi sopra). |
| `KICK` | Un operatore espelle un membro dal canale. |
| `INVITE` | Un operatore invita un utente in un canale (necessario per entrare se il canale è invite-only). |

Vincoli trasversali:
- Devono esistere **utenti regolari e operatori**, con permessi diversi verificati **lato server** (non fidatevi del client: un utente non-operatore che manda `KICK` deve ricevere un errore, non essere semplicemente ignorato "per caso").
- Ogni messaggio inviato a un canale deve arrivare **a tutti gli altri membri** (non a se stessi, tipicamente, a seconda delle convenzioni scelte — da chiarire col client di riferimento).

---

## 6. README.md richiesto (Capitolo V)

Requisito formale e abbastanza rigido, diverso dal solito "scrivi un README":

- **Prima riga obbligatoria, in corsivo**, con formula fissa:
  `*This project has been created as part of the 42 curriculum by <login1>[, <login2>[, <login3>[...]]].*`
- Sezioni minime obbligatorie:
  - **Description**: scopo e overview del progetto.
  - **Instructions**: come compilare, installare, eseguire.
  - **Resources**: riferimenti classici sull'argomento (RFC, articoli, tutorial) **più** una descrizione di come è stata usata l'IA (per quali task, in quali parti — coerente col Capitolo III).
- Possono essere richieste sezioni aggiuntive a seconda del progetto (esempi d'uso, elenco funzionalità, scelte tecniche).
- **Il README dev'essere in inglese.**

---

## 7. Bonus part (Capitolo VI)

Due bonus possibili, **valutati solo se la parte obbligatoria è "perfetta"** (tutti i requisiti mandatory soddisfatti e nessun malfunzionamento — non "quasi tutto", proprio tutto):

1. **File transfer**: gestire il trasferimento di file tra client tramite il server (funzionalità presente in client IRC reali via DCC, ad esempio).
2. **Un bot**: un client automatico che risponde a comandi/eventi.

Nota implicita: se anche solo un requisito mandatory manca o è difettoso, i bonus **non vengono nemmeno guardati** — è un "tutto o niente" più severo della media.

---

## 8. Submission e peer-evaluation (Capitolo VII) — un'altra novità da notare

- Viene valutato **solo ciò che è nel repository Git**.
- I test che scrivete **non vengono consegnati né valutati**, ma sono incoraggiati (utili sia per voi sia per valutare il progetto di un peer in futuro).
- **Il client di riferimento scelto sarà usato durante la valutazione**: se scegliete `irssi` come riferimento, il valutatore userà `irssi` per collegarsi — quindi va testato a fondo proprio con quel client, non solo con `nc`.
- **Novità procedurale rilevante**: durante la valutazione può esservi chiesta **una piccola modifica al progetto sul momento** (poche righe, una feature facile da aggiungere, un cambio di comportamento minore), da fare in pochi minuti, nel vostro ambiente di sviluppo abituale. Serve a verificare che **abbiate davvero capito** il codice e non l'abbiate solo fatto funzionare (coerente col discorso sull'uso dell'IA del Capitolo III: se non capite il vostro stesso codice, questa prova vi smaschera). I dettagli esatti (cosa modificare, tempo a disposizione) sono decisi caso per caso nelle linee guida di valutazione.

---

## 9. Collegamento con lo stato attuale del progetto

Guardando i file già presenti nel repository:

- **`docs.md`** copre già bene i concetti base di socket/fcntl visti nella sezione 5.2-5.3 qui sopra.
- **`Yousef.md`** ha già pianificato correttamente l'architettura a tre classi (`Client`, `Channel`, `Server`) coerente con quanto richiesto dal subject: buffer per client (sezione 5.4), gestione operatori/inviti/ban per canale (sezione 5.6), socket+poll+non-blocking per il server (sezione 5.3).
- **`milestones.md`** segue esattamente l'ordine logico imposto dalle dipendenze del subject: prima l'infrastruttura di rete (senza la quale non si testa nulla), poi la registrazione (`PASS`/`NICK`/`USER`), poi le funzionalità di canale base (`JOIN`/`PRIVMSG`), poi gestione avanzata (`TOPIC`/`MODE` parziale), infine i comandi da operatore (`KICK`/`INVITE`/`MODE` completo) — che richiedono che tutto il resto sia già solido.

Questo significa che il piano di lavoro già delineato nel repo è coerente con i requisiti reali del subject; il valore aggiunto di questo documento è avere in un unico posto **il "perché"** dietro ogni vincolo tecnico, utile soprattutto per la fase di difesa/peer-evaluation dove dovrete giustificare le scelte.
