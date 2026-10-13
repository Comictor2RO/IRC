name - numele get(#general)
topic - topicul get/set
key (mode k) - get/set
limit (mode l) - seteaza o limita de useri get/set
inviteOnly (mode i) - get/set pt invite only channel
topicRestricted (mode t) - get/set restrictioneaza la un anumit topic doar admins pot face ast

add/remove/has client - adauga/daca are/sterge un client din channel
add/remove/get operator - adauga/sterge/verifica un client este admin (mode o)
add/remove/get invites - adauga/sterge/verifica un client daca este invitat
add/remove/get ban - adauga/sterge/verifica daca un client are ban

broadcast - trimit mesaje la toti membrii
sendTopic - trimite topicul la toti membrii
sendNames - trimite lista da users care au acces la channel


Va trebui un vector de clienti care tine clientii din acel channel
Va trebui set uril pt Admins, Invites(Client *) si un set de string uri pt Bans

Numai admins(operatorii) pot face kick, invite, topic, mode(i. t. k. l.)



Client --------> Channel -----------> La toti clientii.
	 mesaj		  broadcast
	 
	 
	 
Va trebui implementat si un operator pt server care va fi primul care se conecteaza la server.
El poate face mode o, si el trebuie sa dea permisiunea ca cineva sa creeze un nou canal.
	 
	 

