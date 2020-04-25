# IoT-Based-Periodic-Temperature-Reporter

### tls.c
This is a source file that can read temperature data from BeagleBone and generates report which will be sent to a remote server via TLS connection. This file also reads input from the server and treats them as commands to control the report. To connect to the server successfully, port number, id number, host name/address should be specified before executing. The user can specify the temperature scale, log option, and time period between two reports before executing the executable.The user can also use START, STOP, SCALE=, LOG, OFF, PERIOD= to control the report during run time.

### Makefile
   This contains three targets:
   - (default) ... build the tls executables.
   - clean ... delete all files created by the Makefile.
   - dist ... build a distribution tarball.
