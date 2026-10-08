# ======================================================================================
#  Makefile TEMPORANEO: compila la parte di Youssef (cartella ..) insieme al codice degli
#  altri (questa cartella) per poterla testare. Da eliminare insieme alla cartella quando
#  i file di Youssef verranno integrati nel progetto del gruppo.
#  Uso:  cd _DA_ELIMINARE_codice_altri && make && ./ircserv 6667 pass
#  NB: i commenti stanno SOPRA le righe, perché un commento a fine riga lascerebbe
#      degli spazi in coda al valore della variabile.
# ======================================================================================

# nome dell'eseguibile richiesto dal subject
NAME        = ircserv

# compilatore e flag obbligatori del subject
CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98
# -MMD -MP generano i file .d: se cambia un .hpp vengono ricompilati i .cpp che lo includono
DEPFLAGS    = -MMD -MP

# cartella con Server/, Client/, Channel/ (la parte di Youssef)
YOUSSEF     = ..

# cartelle in cui cercare gli #include "X.hpp" (per questo nei .cpp non ci sono percorsi relativi)
INCLUDES    = -I$(YOUSSEF)/Server -I$(YOUSSEF)/Client -I$(YOUSSEF)/Channel -IIrcParser

# sorgenti di Youssef
SRCS_YOUSSEF = $(YOUSSEF)/Server/Server.cpp \
               $(YOUSSEF)/Client/Client.cpp \
               $(YOUSSEF)/Channel/Channel.cpp

# sorgenti degli altri (questa cartella)
SRCS_ALTRI  = main.cpp \
              IrcParser/IrcParser.cpp \
              Commands/Registration.cpp \
              Commands/ChannelCommands.cpp \
              Commands/Messaging.cpp

# tutti i sorgenti
SRCS        = $(SRCS_YOUSSEF) $(SRCS_ALTRI)

# gli oggetti vanno tutti in obj/ (obj/Server.o, obj/main.o, ...), non sparsi nelle cartelle
OBJDIR      = obj
OBJS        = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.cpp=.o)))
DEPS        = $(OBJS:.o=.d)

# vpath: in quali cartelle make deve cercare il .cpp che corrisponde a obj/<nome>.o
vpath %.cpp $(YOUSSEF)/Server $(YOUSSEF)/Client $(YOUSSEF)/Channel . IrcParser Commands

# regola di default
all: $(NAME)

# link finale
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# compila ogni .cpp nel suo .o (| $(OBJDIR) = crea prima la cartella, senza ricompilare quando cambia)
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# crea la cartella degli oggetti
$(OBJDIR):
	mkdir -p $(OBJDIR)

# rimuove gli oggetti
clean:
	rm -rf $(OBJDIR)

# rimuove anche l'eseguibile
fclean: clean
	rm -f $(NAME)

# ricompila da zero
re: fclean all

# include i .d generati (il '-' ignora quelli che non esistono ancora)
-include $(DEPS)

# regole che non corrispondono a file
.PHONY: all clean fclean re
