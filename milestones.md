🟢 Faza 1: Infrastructura de bază
1. Socket + Poll + Accept

Setup socket TCP (IPv4/IPv6)

bind(), listen()

Loop principal cu poll()

accept() pentru noi conexiuni

Non-blocking I/O pe toate socket-urile

De ce prima: Fără asta nu poți testa nimic. Trebuie să poți accepta conexiuni înainte de a parsea comenzi.

🟢 Faza 2: Înregistrarea clientului
2. PASS

Simplu: salvezi parola într-un field la Client

Verifici contra celei din config la finalul înregistrării

3. NICK

Salvezi nickname-ul

Verifici unicitate

Validezi caractere (fără spații, doar alfanumerice + -_[]\{})

4. USER

Salvezi username și realname

Completezi structura Client

5. Finalizare înregistrare

După ce ai PASS + NICK + USER → marchezi clientul ca "registered"

Trimite mesajele de welcome (001-004)

De ce împreună: Aceste 3 comenzi fac parte din același flow de înregistrare. Le poți testa imediat cu un client IRC (ex: irssi, weechat).

🟡 Faza 3: Canale de bază
6. JOIN

Creezi canal dacă nu există

Adaugi user în canal

Trimite lista de useri (RPL_NAMREPLY)

Forward mesajul de join către ceilalți

7. PRIVMSG

Trimite mesaj către canal → forward la toți membrii

Trimite mesaj către user → găsești userul și îi trimiți

De ce acum: Cu JOIN + PRIVMSG poți deja testa comunicarea între 2+ clienți. E momentul în care serverul devine "funcțional".

🟡 Faza 4: Management canale
8. TOPIC

Get topic → RPL_TOPIC

Set topic → actualizezi și notifici canalul

9. MODE (parțial)

Începe cu modurile simple: +i, +t, +k, +l

Implementezi storage pentru channel modes

Verifici permisiuni la schimbare

De ce acum: Sunt comenzi de "calitate de viață" care fac serverul să se simtă complet.

🔴 Faza 5: Comenzi de operator
10. KICK

Verifici că cineva dă kick e operator

Scoți user din canal

Notifici canalul

11. INVITE

Verifici permisiuni

Trimite invite userului țintă

Permite userului invitat să intre în canal invite-only

12. MODE (complet)

Adaugi +o (operator) și +v (voiced)

Implementezi +b (ban) dacă vrei extra

De ce la final: Acestea necesită ca totul de mai sus să funcționeze deja (canale, useri, permisiuni).

