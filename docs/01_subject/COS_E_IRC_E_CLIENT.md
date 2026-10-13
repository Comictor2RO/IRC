# Cos'è IRC, cos'è ft_irc, e come si usano irssi / HexChat

## Cos'è IRC

IRC (Internet Relay Chat) è un sistema di chat nato nel 1988, molto prima di WhatsApp, Discord o Slack, che in pratica ne sono i discendenti. Funziona così:

```
   alice (client)  ──┐
   bob   (client)  ──┼──►  SERVER IRC  ◄── tiene l'elenco di utenti e canali, inoltra i messaggi
   carol (client)  ──┘
```

- Il **server** è un programma sempre acceso che aspetta connessioni su una porta (ad esempio la 6667).
- Il **client** è il programma che usa la persona: si collega al server, sceglie un nickname e scrive.
- Un **canale** è come una "stanza" o un "gruppo": `#general`, `#42firenze`. Chi è dentro riceve tutti i messaggi scritti lì.
- Ci sono anche i **messaggi privati**, da utente a utente.
- Chi crea un canale ne diventa **operatore** (l'amministratore, indicato con `@`): può espellere (KICK), invitare (INVITE), mettere una password al canale (MODE +k), ecc.

La cosa importante: IRC è un **protocollo di testo**. Client e server si mandano semplici righe di testo, ad esempio:

```
NICK alice
JOIN #general
PRIVMSG #general :ciao a tutti
```

Le regole di queste righe sono scritte in documenti ufficiali (RFC 1459 / 2812).

## Cos'è il progetto ft_irc

**Dovete scrivere voi il SERVER**, in C++98. Non il client: quello esiste già.

In concreto `./ircserv 6667 password` deve:

1. aprire la porta 6667 e accettare più persone **contemporaneamente**, con un solo processo e un solo `poll()`;
2. capire i comandi che arrivano (PASS, NICK, USER, JOIN, PRIVMSG, KICK, INVITE, TOPIC, MODE);
3. rispondere e inoltrare i messaggi alle persone giuste, nel formato che i client si aspettano.

Il progetto insegna la programmazione di rete: socket, `poll()`, I/O non bloccante, gestire tanti client senza che uno lento blocchi gli altri. È lo stesso modello usato da server veri come nginx o Redis.

Nel nostro codice la divisione è questa:

- **Server**: riceve i byte dalla rete e li consegna;
- **IrcParser**: trasforma la riga di testo in comando + parametri;
- **handler** (`Commands/`): decidono cosa fare per ogni comando.

Chi segue ogni parte è scritto in [RUOLI.md](../02_organizzazione/RUOLI.md).

## Cosa sono irssi e HexChat

Sono **/veri**, cioè programmi "tipo WhatsApp" per IRC. Il subject chiede di sceglierne uno come **client di riferimento**: in valutazione il nostro server deve funzionare con quello, come con un server IRC reale.

|                       | irssi                           | HexChat                      |
| --------------------- | ------------------------------- | ---------------------------- |
| Tipo                  | da terminale (testo)            | con finestre (grafico)       |
| Installazione su Kali | `sudo apt install irssi`      | `sudo apt install hexchat` |
| Sui Mac di 42         | spesso già presente o via brew | —                           |

Con il client vero non scrivi `PRIVMSG #test :ciao`: scrivi solo `ciao` nella finestra del canale, e il client costruisce la riga di protocollo al posto tuo.

### Provare con irssi

Terminale 1, il nostro server:

```bash
make && ./ircserv 6667 pass
```

Terminale 2, il primo utente:

```bash
irssi
/connect 127.0.0.1 6667 pass alice      ← indirizzo, porta, password, nickname
/join #test                             ← entri nel canale (sei operatore, l'hai creato tu)
ciao a tutti                            ← scritto così, senza comandi, va nel canale
```

Terminale 3, il secondo utente: `irssi`, poi `/connect 127.0.0.1 6667 pass bob` e `/join #test`. Ora alice e bob si vedono e chattano.

Altri comandi utili in irssi:

- `/msg bob ciao` → messaggio privato
- `/topic nuovo argomento`
- `/mode #test +i` → canale solo su invito
- `/invite bob`
- `/kick bob motivo`
- `/part` → esci dal canale
- `/quit` → esci da irssi
- **Alt+1, Alt+2…** → passi da una finestra all'altra

### Provare con HexChat

Apri HexChat, poi la lista delle reti, poi **Aggiungi** e chiamala "locale". Modifica la rete così:

- server `127.0.0.1/6667`;
- togli la spunta da "Use SSL";
- nel campo "Server password" metti `pass`.

Poi clicca **Connetti** e scrivi `/join #test`.

## E `nc`?

`nc` (netcat) non è un client IRC: è una "connessione nuda". Quello che scrivi arriva così com'è al server, e vedi esattamente cosa risponde. Per questo serve per i test e la valutazione lo usa, ad esempio per il comando spezzato con Ctrl+D. Con `nc` le righe del protocollo le scrivi a mano: `PASS pass`, `NICK alice`, ecc., come in [TEST_E_FUNZIONAMENTO.md](../04_test/TEST_E_FUNZIONAMENTO.md).

In sintesi:

- **irssi o HexChat** = come lo usa una persona normale;
- **nc** = come lo vede il server "dall'interno".
