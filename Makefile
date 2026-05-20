SRC = main.c \
	  print.c

SRCDIR = src/
OBJDIR = objects/
SRCS = $(addprefix $(SRCDIR), $(SRC))
OBJS = $(patsubst $(SRCDIR)%.c, $(OBJDIR)%.o, $(SRCS))
NAME = ft_ping
CC = gcc
CFLAGS = -Wall -Wextra -Werror -I includes
RM = rm -f


ORANGE = \033[0;33m
GREEN = \033[0;32m
RED = \033[0;31m
RESET = \033[0m

all: $(OBJDIR) $(NAME)

$(OBJDIR):
	@mkdir -p $(OBJDIR)


$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) -o $(NAME) $(OBJS)
	@echo "✅ $(GREEN)./$(NAME) compiled successfully$(RESET)"

$(OBJDIR)%.o: $(SRCDIR)%.c
	@mkdir -p $(dir $@)
	@echo "🛠️ $(ORANGE) Compiling $<...$(RESET)"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "🗑️ $(RED) Cleaning object files...$(RESET)"
	@$(RM) -r $(OBJDIR)

fclean: clean
	@echo "🗑️ $(RED) Cleaning executable...$(RESET)"
	@$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re