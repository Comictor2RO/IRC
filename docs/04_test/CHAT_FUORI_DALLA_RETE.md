# ft_irc — chattare con qualcuno fuori dalla propria rete

## 0. Perché da fuori non funziona "così com'è"

Il server fa già la sua parte: `bind()` su `INADDR_ANY` (0.0.0.0), quindi accetta connessioni da **qualsiasi** interfaccia (`Server/Server.cpp`, funzione che crea il socket).

Il problema è la rete:

```
amico (internet) ──X──> [router di casa, IP pubblico] ──> PC Kali 192.168.x.x:6667
```

Il PC ha un IP **privato**. Da internet si vede solo il router, e il router non sa a quale PC passare una connessione in entrata sulla porta 6667. Le soluzioni qui sotto risolvono proprio questo punto.

| Metodo | Difficoltà | L'amico deve installare qualcosa? | Tocca il router? | Funziona con CGNAT? |
|---|---|---|---|---|
| 1. Tailscale / ZeroTier | facile | sì | no | sì |
| 2. Tunnel TCP (ngrok, playit.gg, bore) | facile | no | no | sì |
| 3. Port forwarding | media | no | sì | **no** |
| 4. VPS | media | no | no | sì |

**Consiglio:** Tailscale per giocare tra amici, ngrok se l'amico non vuole installare nulla.

---

## 1. Avviare il server

```bash
cd ~/Desktop/Github/42_CommonCore/IRC        # root del progetto
make
./ircserv 6667 UnaPasswordRobusta
```

Lascia questo terminale aperto: lì vedi i log `[fd N] >> ...`.

---

## 2. Metodo A — Tailscale (VPN privata, consigliato)

### Sul tuo PC (Kali)
```bash
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up            # apre un link per fare il login
tailscale ip -4              # es. 100.101.102.103  ← questo è l'indirizzo da dare all'amico
```

### Far entrare l'amico
- Dalla console web <https://login.tailscale.com/admin/machines> → sulla tua macchina → **Share** → invia il link all'amico,
  **oppure** lo inviti nella tua tailnet (Settings → Users → Invite).
- L'amico installa Tailscale (Windows / macOS / Linux / Android / iOS) e fa il login.

### L'amico si collega
```bash
nc -C 100.101.102.103 6667
```
oppure con un client vero (vedi sezione 6).

**Pro:** traffico cifrato, niente porte aperte su internet, funziona anche con CGNAT.
**ZeroTier** funziona allo stesso modo: crei una rete su <https://my.zerotier.com>, entrambi fate `zerotier-cli join <network-id>` e usate l'IP assegnato.

---

## 3. Metodo B — Tunnel TCP pubblico (l'amico non installa nulla)

### ngrok
```bash
# installazione: https://ngrok.com/download  (serve un account gratuito)
ngrok config add-authtoken <IL_TUO_TOKEN>
ngrok tcp 6667
```
Output:
```
Forwarding   tcp://0.tcp.eu.ngrok.io:12345 -> localhost:6667
```
L'amico si collega a **host** `0.tcp.eu.ngrok.io` e **porta** `12345`, non alla 6667:
```bash
nc -C 0.tcp.eu.ngrok.io 12345
```

> Nota: ngrok può chiedere di verificare una carta per abilitare i tunnel TCP. In alternativa ci sono **playit.gg** (gratis, pensato per i giochi, supporta TCP) e **bore** (`bore local 6667 --to bore.pub`).

**Effetto collaterale:** sul server tutti i client compaiono con IP `127.0.0.1`, perché passano dal tunnel locale. È normale.

---

## 4. Metodo C — Port forwarding sul router

1. **IP locale del PC**
   ```bash
   ip -4 a | grep inet        # es. 192.168.1.50
   ```
   Conviene dargli un IP fisso (DHCP reservation nel router), altrimenti dopo un riavvio può cambiare.
2. **Router** (di solito <http://192.168.1.1>) → *Port forwarding / Virtual server / NAT*:
   `TCP 6667 esterna → 192.168.1.50:6667`.
3. **Firewall del PC** (solo se attivo):
   ```bash
   sudo ufw allow 6667/tcp
   ```
4. **IP pubblico** da dare all'amico:
   ```bash
   curl ifconfig.me
   ```
5. L'amico: `nc -C <IP_PUBBLICO> 6667`.

### Attenzione al CGNAT
Molti provider (soprattutto quelli su fibra FWA o 4G/5G) non ti danno un IP pubblico vero. Ce l'hai se:
- l'IP WAN mostrato nella pagina del router è **diverso** da `curl ifconfig.me`, oppure
- l'IP WAN è nel range `100.64.0.0` – `100.127.255.255`.

In quel caso il port forwarding **non può funzionare**: usa il metodo A o B.

---

## 5. Metodo D — VPS (server sempre acceso)

1. Prendi una VM con IP pubblico (Oracle Cloud free tier, Hetzner, ecc.).
2. Copia il progetto, `make`, `./ircserv 6667 password` (meglio dentro `tmux` così resta acceso).
3. Apri la porta TCP 6667 **sia** nel firewall del provider (security group) **sia** sulla VM (`ufw allow 6667/tcp`).
4. Tutti si collegano a `<IP_VPS> 6667`.

---

## 6. Come si collega l'amico

### Con `nc` (test grezzo)
```bash
nc -C <host> <porta>
PASS UnaPasswordRobusta
NICK amico
USER amico 0 * :Nome Amico
JOIN #test
PRIVMSG #test :ciao dall'esterno!
```
`-C` serve a mandare `\r\n` a fine riga, come vuole il protocollo IRC.

### Con un client vero
- **irssi** (Linux/macOS): `/connect <host> <porta> UnaPasswordRobusta amico`, poi `/join #test`
- **HexChat** (Windows/Linux): Network List → Add → server `<host>/<porta>`, password del server = `UnaPasswordRobusta`, **SSL disattivato**.

Tu, dal tuo PC, puoi collegarti in locale con `nc -C 127.0.0.1 6667`, fare `JOIN #test` e chattare con lui.

---

## 7. Come verificare che sia davvero raggiungibile da fuori

Il test va fatto **da fuori**: se ti colleghi da un altro PC della stessa Wi-Fi non dimostra niente.

- Telefono con **Wi-Fi spento** (dati mobili) + app **Termux**: `pkg install netcat-openbsd` → `nc -C <host> <porta>`.
- Oppure chiedi all'amico di provare e guarda il terminale del server: deve comparire la nuova connessione.

### Se non si collega
| Sintomo | Causa probabile |
|---|---|
| `Connection refused` | server non avviato, oppure porta sbagliata (con ngrok la porta è quella di ngrok!) |
| si blocca senza risposta (timeout) | firewall, port forwarding sbagliato, oppure CGNAT |
| si connette ma nessuna risposta ai comandi | manca `-C` in `nc` (righe senza `\r\n`), oppure manca `PASS` |
| funziona da casa ma non da fuori | ti stai collegando con l'IP privato `192.168.x.x` |

---

## 8. Sicurezza

Con i metodi B, C e D il server è **esposto a tutto internet**, e i bot scansionano di continuo le porte aperte.
- Usa una password lunga, non `pass`.
- Spegni il server (`Ctrl+C`) e il tunnel o il port forwarding quando hai finito.
- È un progetto scolastico, non un server temprato per resistere a input ostili: non lasciarlo acceso senza controllo.
- Il traffico IRC è **in chiaro** (niente TLS). Con Tailscale o ZeroTier è cifrato dalla VPN, con gli altri metodi no.
