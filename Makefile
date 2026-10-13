# Prima: compilava solo main, Server, Channel, IrcParser e Client, senza -I e senza file .d.
# Ora ci sono anche i file di Commands/ (gli handler tolti da Server.cpp).
NAME = ircserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
# -MMD -MP: generate .d files, so a .cpp is recompiled when a .hpp it includes changes
DEPFLAGS = -MMD -MP
# the sources use #include "X.hpp" without a path
INCLUDES = -IServer -IClient -IChannel -IIrcParser

SRCS = main.cpp \
	   Server/Server.cpp \
	   Client/Client.cpp \
	   Channel/Channel.cpp \
	   IrcParser/IrcParser.cpp \
	   Commands/Registration.cpp \
	   Commands/ChannelCommands.cpp \
	   Commands/Messaging.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
