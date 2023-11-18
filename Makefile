all: xhttpd

xhttpd: httpserver.c
	gcc -W -Wall -lpthread -o xhttpd httpserver.c

clean:
	rm xhttpd
