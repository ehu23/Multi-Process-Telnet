Source Code for Remote File Access Protocol

Brief Description:
A multi-process Telnet-like server and client. Parses + polls I/O over a TCP socket connection and (de)compresses the data packets using zlib. Server forks a new shell process for every new connection.

How to use:
---Recommended running in terminal--- 

1. make distribution with "make dist"

Files in the tarball (by running "make dist"):
	Makefile => "make" builds both server and client source codes, can also build separately with "make client" and "make server". Also has "make clean" and "make dist".
	README (this file)
	lab1b-server.c => server source code
	lab1b-client.c => client source code

2. Run server's protocol on the remote machine, and client's protocol on the local.
3. Command line parameters are as follows: 
	--port=InputPortHere
	--log=filename
	--compress
4. Turn compress on with the compress option. Log data transfers with the log option. And make sure both client and server use the same port (mandatory option).
5. Access file system as any other terminal.

Note: Currently configured to localhost, change if connection is external.


External sources of information:
http://www.unixguide.net/unix/programming/2.1.2.shtml
https://www.zlib.net/zlib_how.html
http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html

