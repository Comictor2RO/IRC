# Documentazione ft_irc

Le guide sono divise in quattro cartelle, da leggere più o meno in quest'ordine.

## 01_subject — cosa chiede il progetto

| File | Contenuto |
|---|---|
| [ft_irc.subject.pdf](01_subject/ft_irc.subject.pdf) | Il subject ufficiale |
| [scheda_valutazione.pdf](01_subject/scheda_valutazione.pdf) | La scheda usata in valutazione |
| [SPIEGAZIONE_SUBJECT.md](01_subject/SPIEGAZIONE_SUBJECT.md) | Il subject spiegato punto per punto: regole, funzioni permesse, README richiesto |
| [COS_E_IRC_E_CLIENT.md](01_subject/COS_E_IRC_E_CLIENT.md) | Cos'è IRC e come si usano irssi, HexChat e `nc` |
| [beej_guida_network.pdf](01_subject/beej_guida_network.pdf) | Beej's Guide to Network Programming (socket, `poll()`) |

## 02_organizzazione — chi fa cosa

| File | Contenuto |
|---|---|
| [RUOLI.md](02_organizzazione/RUOLI.md) | Divisione dei compiti tra Youssef, Vlad e Gabry |
| [MILESTONES.md](02_organizzazione/MILESTONES.md) | Ordine delle fasi di sviluppo |

## 03_progettazione — appunti di progettazione

| File | Contenuto |
|---|---|
| [PIANO_CLASSI.md](03_progettazione/PIANO_CLASSI.md) | Progetto delle classi Server, Client e Channel |
| [NOTE_CHANNEL.md](03_progettazione/NOTE_CHANNEL.md) | Requisiti della classe Channel |
| [NOTE_SOCKET.md](03_progettazione/NOTE_SOCKET.md) | Costanti di socket, `poll()` e `fcntl()` |

## 04_test — provare il server

| File | Contenuto |
|---|---|
| [GUIDA_SEMPLICE.md](04_test/GUIDA_SEMPLICE.md) | Tutti i comandi obbligatori provati passo passo con tre utenti |
| [TEST_E_FUNZIONAMENTO.md](04_test/TEST_E_FUNZIONAMENTO.md) | Come funziona un comando dall'inizio alla fine e test dettagliati |
| [CHAT_FUORI_DALLA_RETE.md](04_test/CHAT_FUORI_DALLA_RETE.md) | Collegarsi al server da un'altra rete |

## Compilare e avviare

Dalla root del progetto:

```bash
make
./ircserv <porta> <password>
```
