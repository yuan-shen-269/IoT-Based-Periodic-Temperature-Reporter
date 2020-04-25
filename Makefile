TARBALL= tls.tar.gz
CFLAG = -Wall -Wextra -g -lmraa -lm

default:	
	gcc $(CFLAG) -lssl -lcrypto -o tls tls.c

clean:
	rm -f *.tar.gz tls

dist:   
	tar -cvzf $(TARBALL) README Makefile tls.c 
