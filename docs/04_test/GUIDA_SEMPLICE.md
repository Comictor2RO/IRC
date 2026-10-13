# Guida semplice: provare TUTTI i comandi obbligatori del subject

Il subject chiede questi comandi:

| Gruppo             | Comandi                                                                                |
| ------------------ | -------------------------------------------------------------------------------------- |
| Login              | `PASS`, `NICK`, `USER`                                                           |
| Canali e messaggi  | `JOIN`, `PRIVMSG` (nel canale e in privato)                                        |
| Solo per operatori | `KICK`, `INVITE`, `TOPIC`, `MODE` con i modi `i`, `t`, `k`, `o`, `l` |

La guida li prova tutti, **in ordine**, con tre utenti: `alice`, `bob` e `carl`.
Segui i passi dall'alto verso il basso, senza saltarne: ogni passo usa quello che hai fatto nei precedenti.

---

## 0. Prepara 4 terminali

In VSCode: menu **Terminale → Nuovo terminale**, ripetuto 4 volte. Puoi rinominarli (tasto destro sul nome → *Rinomina*).

| Terminale    | Chi è    | Cosa ci scrivi                                     |
| ------------ | --------- | -------------------------------------------------- |
| **T0** | il server | solo i comandi per avviarlo, poi lo guardi e basta |
| **T1** | `alice` | sarà l'**operatore** del canale (il capo)   |
| **T2** | `bob`   | utente normale                                     |
| **T3** | `carl`  | utente normale, all'inizio fuori dal canale        |

### T0 — avvia il server

```bash
cd ~/Desktop/Github/42_CommonCore/IRC
make
./ircserv 6667 ciao123
```

Deve comparire `[server] in ascolto sulla porta 6667`.
Se invece compare `Address already in use`, hai già un server acceso da un'altra parte: trova quel terminale e premi Ctrl+C, oppure usa la porta `6668` ovunque in questa guida.

In T0 vedrai passare ogni riga che i client mandano, così:

```
[fd 4] >> PASS ciao123
```

È il modo più comodo per capire cosa arriva al server.

### T1, T2, T3 — collegati

In **ognuno** dei tre terminali scrivi:

```bash
nc -C 127.0.0.1 6667
```

Lo schermo resta vuoto: è normale, sei connesso e il server aspetta i tuoi comandi.
`-C` serve a far finire ogni riga con `\r\n`, come vuole il protocollo IRC. Non scrivere mai `\r` a mano.

---

## 1. Come è fatta una riga IRC (e cosa fa il parser)

> ⚠️ **Questa sezione è solo teoria: non scrivere niente nei terminali.**
> Gli esempi usano il canale `#42`, che però **ancora non esiste**: all'avvio il server non ha nessun canale. `#42` verrà creato al **passo 3**, quando alice scrive `JOIN #42`.

Ogni comando è **una riga di testo** fatta così:

```
[:prefisso] COMANDO param1 param2 ... [:trailing]
```

| Pezzo         | Obbligatorio? | Cos'è                                                                                                                                                 |
| ------------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `:prefisso` | no            | chi manda il messaggio. Lo usa il**server** quando inoltra (es. `:alice!alice@127.0.0.1`); tu non lo scrivi mai                                |
| `COMANDO`   | sì           | la parola che dice cosa fare:`JOIN`, `PRIVMSG`, ...                                                                                                |
| `param`     | dipende       | parole separate da spazi,**senza** spazi dentro                                                                                                  |
| `:trailing` | no            | l'**ultimo** parametro: inizia con `:` e prende **tutto il resto della riga, spazi compresi**. Si usa per frasi: messaggi, topic, motivi |

### Il percorso di una riga dentro il server

1. **Lettura** (`Server::readFromClient`): `recv()` legge i byte e li accoda al buffer del client.
2. **Taglio in righe** (`Client::extractLine`): cerca `\n` e taglia una riga completa, togliendo `\r\n`. Se il `\n` non è ancora arrivato aspetta il prossimo `recv()`: per questo un comando spezzato in più pezzi funziona lo stesso.
3. **Parser** (`IrcParser::parse`): divide la riga in 4 campi:
   - salta gli spazi iniziali;
   - se la riga inizia con `:`, legge il **prefisso** fino al primo spazio;
   - legge il **comando** e lo converte in MAIUSCOLO (`join` diventa `JOIN`);
   - legge i **parametri** uno per uno, separati da spazi;
   - quando un parametro inizia con `:`, tutto il resto della riga diventa il **trailing** (`hasTrailing = true`) e il parsing finisce.
4. **Dispatch** (`Server::dispatchCommand`): cerca il comando nella tabella `commands`.
   - comando sconosciuto → `421`, oppure `451` se non sei ancora registrato;
   - prima del login sono permessi solo `PASS`, `NICK`, `USER`, `CAP`, `PING`, `PONG` e `QUIT`; tutto il resto → `451 You have not registered`;
   - altrimenti chiama l'handler, per esempio `handleJoin`.

### Esempio completo

```
privmsg   #42   :ciao a tutti
```

(se la scrivessi adesso riceveresti un errore, perché `#42` non esiste ancora; il parser però la divide sempre allo stesso modo) diventa:

```
command     = "PRIVMSG"          ← convertito in maiuscolo
params      = ["#42"]            ← gli spazi multipli vengono saltati
trailing    = "ciao a tutti"     ← tutto dopo ':', spazi compresi
hasTrailing = true
```

### Come leggere le risposte del server

```
:ircserv 001 alice :Welcome ...        ← risposta numerica: il numero dice cosa è successo
:alice!alice@127.0.0.1 JOIN #42        ← un evento: "alice è entrata in #42"
```

I numeri da `001` a `399` sono risposte normali, quelli da `400` a `599` sono **errori**.

---

## 2. Login: `PASS`, `NICK`, `USER`

### Struttura

```
PASS <password>
NICK <nickname>
USER <username> <mode> <unused> :<nome reale>
```

### Cosa fa il parser

| Riga                            | command  | params                  | trailing          |
| ------------------------------- | -------- | ----------------------- | ----------------- |
| `PASS ciao123`                | `PASS` | `["ciao123"]`         | —                |
| `NICK alice`                  | `NICK` | `["alice"]`           | —                |
| `USER alice 0 * :Alice Rossi` | `USER` | `["alice", "0", "*"]` | `"Alice Rossi"` |

### Cosa fa il server

- `PASS`: confronta la password con quella passata a `./ircserv`. Se è giusta segna il client come "autenticato"; se è sbagliata manda `464` e chiude la connessione.
- `NICK`: controlla che il nick sia valido (inizia con una lettera, massimo 30 caratteri) e libero.
- `USER`: salva username (`params[0]`) e nome reale (trailing). `0` e `*` sono campi storici, ignorati.
- Quando ha ricevuto **tutti e tre**, il server ti manda il benvenuto `001`–`005`. Da quel momento sei "registrato".

### Prova

La password dopo `PASS` deve essere **la stessa** che hai scritto avviando il server. Qui è `ciao123` perché in T0 hai lanciato `./ircserv 6667 ciao123`. Se l'hai avviato con un'altra password (per esempio `./ircserv 6667 pass123`), scrivi quella.

**T1:**

```
PASS ciao123
NICK alice
USER alice 0 * :Alice
```

**T2:**

```
PASS ciao123
NICK bob
USER bob 0 * :Bob
```

**T3:**

```
PASS ciao123
NICK carl
USER carl 0 * :Carl
```

✅ In ognuno compare `:ircserv 001 <nick> :Welcome to the ft_irc Network ...`

### Prove di errore (facoltative: apri un 5° terminale con `nc -C 127.0.0.1 6667`)

| Scrivi                              | Risposta                                  | Significato                             |
| ----------------------------------- | ----------------------------------------- | --------------------------------------- |
| `JOIN #prova` (prima del login)   | `451 :You have not registered`          | prima devi fare PASS, NICK e USER       |
| `PASS sbagliata`                  | `464 :Password incorrect` + chiusura    | password errata                         |
| `PASS ciao123` poi `NICK alice` | `433 alice :Nickname is already in use` | nick già preso                         |
| `NICK 1abc`                       | `432 1abc :Erroneous nickname`          | un nick non può iniziare con un numero |

---

## 3. `JOIN` — entrare in un canale

### Struttura

```
JOIN <#canale>[,<#canale2>...] [<chiave>[,<chiave2>...]]
```

### Cosa fa il parser

`JOIN #42` → command=`JOIN`, params=`["#42"]`
`JOIN #a,#b chiave1` → params=`["#a,#b", "chiave1"]`. Le virgole le divide l'handler, non il parser.

### Cosa fa il server

- controlla il nome: deve iniziare con `#` o `&`, senza spazi né virgole;
- se il canale **non esiste** lo crea, e chi entra per primo diventa **operatore** (`@`);
- se esiste, controlla i modi: `+i` (serve un invito), `+k` (serve la chiave), `+l` (canale pieno);
- annuncia l'ingresso a tutti e ti manda la lista dei membri (`353`).

### Prova

Finora nel server **non esiste nessun canale**. Il JOIN di alice è quello che **crea** `#42`.

**T1 (alice):**

```
JOIN #42
```

✅ `:ircserv 353 alice = #42 :@alice`. alice è l'unica nel canale, e la `@` vuol dire che è operatore perché il canale l'ha creato lei. Da qui in poi `#42` esiste.

**T2 (bob):**

```
JOIN #42
```

✅ bob vede `:ircserv 353 bob = #42 :@alice bob`; alice vede `:bob!bob@127.0.0.1 JOIN #42`.

carl **per ora resta fuori**: ci servirà per provare `INVITE`, `+k` e `+l`.

---

## 4. `PRIVMSG` — mandare messaggi

### Struttura

```
PRIVMSG <#canale> :<testo>      ← a tutti i membri del canale
PRIVMSG <nick> :<testo>         ← solo a quella persona
```

### Cosa fa il parser

`PRIVMSG #42 :ciao a tutti` → params=`["#42"]`, trailing=`"ciao a tutti"`
Senza `:` il messaggio si fermerebbe al primo spazio: `PRIVMSG #42 ciao a tutti` manda solo `ciao`.

### Cosa fa il server

- canale: deve esistere (`403`) e tu devi esserne membro (`404`); il messaggio va a tutti **tranne te**;
- nick: deve esistere (`401`); il messaggio arriva solo a lui;
- testo vuoto → `412`, destinatario mancante → `411`.

### Prova

**T1 (alice):**

```
PRIVMSG #42 :ciao a tutti
PRIVMSG bob :ciao bob, questo lo vedi solo tu
```

✅ In **T2** compaiono entrambi; in **T3** niente (carl non è nel canale e il secondo era privato).

**T3 (carl):**

```
PRIVMSG #42 :posso scrivere?
```

❌ `404 carl #42 :Cannot send to channel`, perché carl non è nel canale.

---

## 5. `TOPIC` — l'argomento del canale

### Struttura

```
TOPIC <#canale>               ← legge il topic
TOPIC <#canale> :<testo>      ← lo cambia
TOPIC <#canale> :             ← lo cancella (trailing vuoto)
```

### Cosa fa il parser

Qui conta `hasTrailing`, perché le due righe hanno significati diversi:

- `TOPIC #42` → params=`["#42"]`, hasTrailing=`false` → **leggi**
- `TOPIC #42 :` → params=`["#42"]`, trailing=`""`, hasTrailing=`true` → **cancella**

### Cosa fa il server

- leggere: risponde `332` (il topic) oppure `331` (nessun topic);
- cambiare: se il canale ha `+t` serve essere operatori (`482`); il cambio viene annunciato a tutti.

### Prova

**T1 (alice):**

```
TOPIC #42 :Progetto ft_irc
TOPIC #42
```

✅ Tutti vedono `:alice!... TOPIC #42 :Progetto ft_irc`; poi alice riceve `332 alice #42 :Progetto ft_irc`.

Il seguito, con `+t`, si prova nella sezione 8.

---

## 6. `MODE` — come funziona

### Struttura

```
MODE <#canale>                         ← mostra i modi attivi (324)
MODE <#canale> <+/-lettere> [parametri]
```

| Modo  | Con`+`                               | Con`-`                | Parametro                     |
| ----- | -------------------------------------- | ----------------------- | ----------------------------- |
| `i` | solo su invito                         | tutti possono entrare   | —                            |
| `t` | solo gli operatori cambiano il topic   | tutti possono cambiarlo | —                            |
| `k` | serve una chiave (password del canale) | toglie la chiave        | `+k <chiave>`               |
| `o` | dà i privilegi di operatore           | li toglie               | `<nick>` in entrambi i casi |
| `l` | limite massimo di utenti               | toglie il limite        | `+l <numero>`               |

### Cosa fa il parser

`MODE #42 +kl segreto 5` → params=`["#42", "+kl", "segreto", "5"]`
L'handler legge `+kl` lettera per lettera e prende i parametri **in ordine**: `k` prende `segreto`, `l` prende `5`.

### Cosa fa il server

- solo un operatore può cambiare i modi (`482`); per leggerli invece basta `MODE #42`;
- annuncia a tutti **solo** i modi cambiati davvero: se `+t` è già attivo, un altro `+t` non produce nulla;
- una lettera sconosciuta → `472`; un parametro mancante → `461`.

**T1 (alice):**

```
MODE #42
```

✅ `324 alice #42 +`: nessun modo attivo.

---

## 7. `MODE +i` / `-i` e `INVITE`

### Struttura di INVITE

```
INVITE <nick> <#canale>
```

Parser: `INVITE carl #42` → params=`["carl", "#42"]`. Attenzione all'ordine: prima il nick, poi il canale.

### Cosa fa il server

Registra l'invito nel canale: carl potrà entrare anche se c'è `+i`. alice riceve `341`, carl riceve `:alice!... INVITE carl :#42`.
Errori: `482` se non sei operatore, `443` se l'invitato è già nel canale, `401` se il nick non esiste.

### Prova

**T1 (alice):**

```
MODE #42 +i
```

**T2 (bob):**

```
MODE #42 -i
```

❌ `482 bob #42 :You're not channel operator`: bob non è operatore.

**T3 (carl):**

```
JOIN #42
```

❌ `473 carl #42 :Cannot join channel (+i)`

**T1 (alice):**

```
INVITE carl #42
```

**T3 (carl):**

```
JOIN #42
```

✅ Ora entra: vede `332` con il topic e `353 ... :@alice bob carl`.

Fai uscire carl e togli `+i`:

**T3 (carl):**

```
PART #42
```

**T1 (alice):**

```
MODE #42 -i
```

---

## 8. `MODE +t` / `-t` — topic riservato

**T1 (alice):**

```
MODE #42 +t
```

**T2 (bob):**

```
TOPIC #42 :lo cambio io
```

❌ `482 bob #42 :You're not channel operator`

**T1 (alice):**

```
MODE #42 -t
```

**T2 (bob):**

```
TOPIC #42 :lo cambio io
```

✅ Ora funziona: tutti vedono `:bob!... TOPIC #42 :lo cambio io`.

---

## 9. `MODE +k` / `-k` — chiave del canale

**T1 (alice):**

```
MODE #42 +k segreto
```

**T3 (carl):**

```
JOIN #42
```

❌ `475 carl #42 :Cannot join channel (+k)`

```
JOIN #42 segreto
```

✅ Entra. La chiave è il **secondo parametro** di JOIN.

```
PART #42
```

**T1 (alice):**

```
MODE #42 -k
```

---

## 10. `MODE +l` / `-l` — limite di utenti

Nel canale ora ci sono 2 persone (alice e bob).

**T1 (alice):**

```
MODE #42 +l 2
```

**T3 (carl):**

```
JOIN #42
```

❌ `471 carl #42 :Cannot join channel (+l)`: il canale è pieno.

**T1 (alice):**

```
MODE #42 -l
```

**T3 (carl):**

```
JOIN #42
```

✅ Ora entra.

---

## 11. `MODE +o` / `-o` — dare e togliere l'operatore

**T1 (alice):**

```
MODE #42 +o bob
```

✅ Tutti vedono `:alice!... MODE #42 +o bob`. Ora bob può usare i comandi da operatore:

**T2 (bob):**

```
MODE #42 +t
```

✅ Funziona: niente più `482`.

**T1 (alice):**

```
MODE #42 -o bob
MODE #42 -t
```

✅ bob torna utente normale.

---

## 12. `KICK` — espellere qualcuno

### Struttura

```
KICK <#canale> <nick> [:<motivo>]
```

Parser: `KICK #42 carl :fuori di qui` → params=`["#42", "carl"]`, trailing=`"fuori di qui"`

### Cosa fa il server

- servono: canale esistente (`403`), tu nel canale (`442`), tu operatore (`482`), bersaglio esistente (`401`) e dentro il canale (`441`);
- annuncia `:alice!... KICK #42 carl :fuori di qui` a **tutti**, espulso compreso, poi lo toglie dal canale;
- senza motivo usa come motivo il nick di chi espelle.

### Prova

**T2 (bob):**

```
KICK #42 carl
```

❌ `482`: bob non è operatore.

**T1 (alice):**

```
KICK #42 carl :fuori di qui
```

✅ Tutti, carl compreso, vedono il KICK.

**T3 (carl):**

```
PRIVMSG #42 :ci sono ancora?
```

❌ `404`: carl non è più nel canale.

---

## 13. Il test del subject: comando spezzato (Ctrl+D)

Il subject chiede di verificare che un comando mandato **a pezzi** funzioni.
In **T3 (carl)** scrivi `PRIV`, poi premi **Ctrl+D** (non Invio). Scrivi `MSG ali` e premi di nuovo **Ctrl+D**. Infine scrivi `ce :pezzi` e premi **Invio**.

✅ In T1 alice riceve `:carl!... PRIVMSG alice :pezzi`.
Ogni Ctrl+D fa spedire a `nc` i caratteri scritti fino a quel momento. Il server li accumula nel buffer e li esegue solo quando arriva il `\n` (sezione 1, punto 2).

---

## 14. Uscire

| Dove                 | Come                                                                              |
| -------------------- | --------------------------------------------------------------------------------- |
| un client (T1/T2/T3) | scrivi`QUIT :ciao` (gli altri vedono `QUIT :Quit: ciao`), oppure premi Ctrl+C |
| il server (T0)       | **Ctrl+C**                                                                  |

Se esce l'ultimo operatore di un canale, il server promuove automaticamente un altro membro: vedrai `:ircserv MODE #42 +o bob`.

---

## Riassunto degli errori che incontri

| Codice                      | Significato                                                                 |
| --------------------------- | --------------------------------------------------------------------------- |
| `401`                     | nick inesistente                                                            |
| `403`                     | canale inesistente                                                          |
| `404`                     | non puoi scrivere nel canale (non ci sei dentro)                            |
| `421`                     | comando sconosciuto                                                         |
| `432` / `433`           | nick non valido / nick già usato                                           |
| `441`                     | quella persona non è nel canale                                            |
| `442`                     | tu non sei nel canale                                                       |
| `443`                     | quella persona è già nel canale                                           |
| `451`                     | non hai ancora fatto il login                                               |
| `461`                     | mancano parametri                                                           |
| `464`                     | password sbagliata                                                          |
| `471` / `473` / `475` | canale pieno (`+l`) / solo su invito (`+i`) / chiave sbagliata (`+k`) |
| `472`                     | lettera di MODE sconosciuta                                                 |
| `476`                     | nome del canale non valido                                                  |
| `482`                     | non sei operatore                                                           |
