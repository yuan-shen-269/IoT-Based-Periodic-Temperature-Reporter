TARBALL= tls.tar.gz
CFLAG = -Wall -Wextra -g -lmraa -lm

default:	
	gcc $(CFLAG) -lssl -lcrypto -o lab4c_tls lab4c_tls.c

clean:
	rm -f *.tar.gz lab4c_tls

dist:   
	tar -cvzf $(TARBALL) README Makefile lab4c_tls.c 
