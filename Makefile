NAME		= ircserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -MD -MP -Isrc

SRCDIR		= src
OBJDIR		= obj

SRC			= \
	main.cpp \
	core/CommandDispatcher.cpp \
	messages/Message.cpp \
	networking/Channel.cpp \
	networking/Client.cpp \
	networking/NetworkHost.cpp \
	networking/g_host.cpp \
	util/LogType.cpp \
	util/Logger.cpp

OBJ			= $(addprefix $(OBJDIR)/,$(SRC:.cpp=.o))
DEP			= $(OBJ:.o=.d)

.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEP)
