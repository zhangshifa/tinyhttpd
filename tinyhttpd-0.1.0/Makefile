all: httpd test

httpd: httpd.c
	gcc -W -Wall -lpthread -o httpd httpd.c -g
test:  simpleclient.c 
	gcc simpleclient.c -o simpleclient -g
clean:
	rm httpd simpleclient
