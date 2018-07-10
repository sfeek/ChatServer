all:
	$(CC) -Wall -Werror chat_server.c tinyexpr.c -ggdb -lpthread -lm -o chat_server

clean:
	$(RM) -rf chat_server
