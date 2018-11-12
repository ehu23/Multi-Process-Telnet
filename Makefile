#Edward Hu

default: client server

client: client.c
	gcc -o client -Wall -Wextra -lz client.c

server: server.c
	gcc -o server -Wall -Wextra -lz server.c

FILES = client.c server.c README Makefile
dist: $(FILES)
	tar -czf dist.tar.gz $(FILES)

clean:
	rm -f client server dist.tar.gz

