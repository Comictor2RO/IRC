# Correzione ft_irc — Checklist (da ng_5_ft_irc.pdf)

> Regole generali: segfault / crash / terminazione inattesa durante la difesa = **0**.
> Leak di memoria = flag **Leaks**. Clonare il repo in una cartella vuota e verificare che non ci siano alias strani.

Allegati citati nella scheda: `subject.pdf`, `bircd.tar.gz`.

---

## 0. Preparazione

```bash
git clone <repo> /tmp/eval_irc && cd /tmp/eval_irc
alias            # controlla che non ci siano alias sospetti (make, c++, nc...)
make
./ircserv 6667 password
```

Tieni aperti più terminali: server, 2-3 `nc`, client di riferimento (es. irssi / weechat / hexchat).

---

## 1. Basic checks (se uno fallisce → voto 0)

- [ ] C'è un **Makefile**, compila correttamente, è in **C++**, l'eseguibile si chiama **`ircserv`**
- [ ] C'è **un solo `poll()`** (o equivalente: select/epoll/kqueue) nel codice
- [ ] `poll()` viene chiamato **prima di ogni** `accept`, `read/recv`, `write/send`
- [ ] Dopo queste chiamate **`errno` non viene usato** per decidere azioni (es. rileggere se `errno == EAGAIN`)
- [ ] Ogni `fcntl()` è esattamente `fcntl(fd, F_SETFL, O_NONBLOCK);` — qualsiasi altro uso è vietato

Comandi utili:
```bash
grep -rn "poll(\|select(\|epoll_wait\|kevent" --include=*.cpp --include=*.hpp .
grep -rn "fcntl"  --include=*.cpp --include=*.hpp .
grep -rn "errno"  --include=*.cpp --include=*.hpp .
grep -rn "accept(\|recv(\|send(\|read(\|write(" --include=*.cpp .
```
Chiedi di mostrarti il flusso: tutti gli `accept/recv/send` devono stare dentro il ramo dopo `poll()` con `revents` corrispondente (`POLLIN` / `POLLOUT`).

---

## 2. Networking

- [ ] Il server parte e **ascolta su tutte le interfacce** sulla porta passata da riga di comando
  ```bash
  ss -ltnp | grep 6667     # deve essere 0.0.0.0:6667 (o *:6667)
  ```
- [ ] Con **`nc`** ci si connette, si mandano comandi e il server risponde
  ```bash
  nc -C 127.0.0.1 6667
  PASS password
  NICK tizio
  USER tizio 0 * :Tizio Rossi
  ```
- [ ] Chiedi al gruppo **qual è il client IRC di riferimento**
- [ ] Con quel client ci si connette al server (es. `irssi` → `/connect 127.0.0.1 6667 password`)
- [ ] **Connessioni multiple contemporanee**, il server non si blocca e risponde a tutti (test client + nc insieme)
- [ ] **JOIN** di un canale: tutti i messaggi di un client sul canale arrivano a **tutti gli altri** membri
  ```
  JOIN #test
  PRIVMSG #test :ciao a tutti
  ```

---

## 3. Networking specials

- [ ] **Comandi parziali** con nc (come nel subject): il server ricompone correttamente; nel frattempo le altre connessioni funzionano
  ```bash
  nc -C 127.0.0.1 6667
  # scrivi "com" poi Ctrl+D, "man" poi Ctrl+D, "d" poi Invio
  # es: "PRIV" ^D "MSG #test" ^D " :ciao" Invio
  ```
- [ ] **Kill improvviso di un client** (`kill -9` / chiudi terminale): il server resta operativo per gli altri e accetta nuovi client
- [ ] **Kill di un nc con mezzo comando inviato**: il server non resta in stato strano / bloccato
- [ ] **Ctrl+Z su un client** connesso a un canale, poi **flood del canale** da un altro client:
  - il server **non si blocca**
  - quando il client riprende (`fg`), tutti i comandi/messaggi accumulati vengono processati normalmente
  - controlla i **leak** durante l'operazione
  ```bash
  # flood da un altro terminale:
  for i in $(seq 1 10000); do echo "PRIVMSG #test :flood $i"; done | nc -C 127.0.0.1 6667
  # (prima fai PASS/NICK/USER/JOIN nello stesso stream)
  ```

Leak:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./ircserv 6667 password
```

---

## 4. Client Commands — **voto da 0 a 5**

- [ ] Con **nc e con il client di riferimento**: autenticazione (`PASS`), `NICK`, `USER`, `JOIN` funzionano
- [ ] **PRIVMSG** e **NOTICE** pienamente funzionanti con parametri diversi
  ```
  PRIVMSG nick :ciao           # privato
  PRIVMSG #test :ciao          # canale
  PRIVMSG nick_inesistente :x  # errore 401
  PRIVMSG                      # errore 411/461
  NOTICE nick :avviso
  ```
- [ ] Un **utente normale NON ha privilegi da operatore** (deve ricevere 482 ERR_CHANOPRIVSNEEDED)
- [ ] Poi testa **con un operatore** tutte le operazioni di canale — **togli 1 punto per ogni feature non funzionante**:

| Comando | Test |
|---|---|
| `KICK` | `KICK #test nick :motivo` |
| `INVITE` | `INVITE nick #test` |
| `TOPIC` | `TOPIC #test` (legge) / `TOPIC #test :nuovo topic` (scrive) |
| `MODE +i / -i` | canale solo su invito → JOIN senza invito deve fallire (473) |
| `MODE +t / -t` | con +t solo gli operatori cambiano il topic |
| `MODE +k / -k` | `MODE #test +k chiave` → `JOIN #test chiave` (senza → 475) |
| `MODE +o / -o` | `MODE #test +o nick` dà/toglie operatore |
| `MODE +l / -l` | `MODE #test +l 2` → il 3° utente non entra (471) |

Casi extra utili: nick già usato (433), comandi prima della registrazione (451), password errata (464), JOIN canale senza `#`/`&`.

---

## 5. Bonus

- [ ] **File transfer** funziona con il client di riferimento (es. DCC SEND in irssi/hexchat) — Yes/No
- [ ] C'è un **bot IRC** — Yes/No

> I bonus si valutano solo se la parte obbligatoria è perfetta.

---

## 6. Flag finali

`Ok` · `Outstanding project` · `Empty work` · `Invalid compilation` · `Cheat` · `Crash` · `Incomplete group` · `Leaks` · `Forbidden function`

Lascia un commento finale sulla valutazione.
