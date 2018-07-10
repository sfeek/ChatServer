all:
	$(CC) -Wall -Werror chat_server.c tinyexpr.c -O2 -lpthread -lm -o chat_server

clean:
	$(RM) -rf chat_server
