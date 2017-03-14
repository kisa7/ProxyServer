# ProxyServer was created for System Programming lab of Seoul National University
C

Proxy:
Implemention of a sequential logging proxy. Proxy opens a socket and listed for a connection request. 
When it receives a connection request, it accepts the connection, read the echo request and parse it 
to determine the hostname and port of the echo server. It should then open a connection to the end echo 
server, send it the request, receive the reply, and forward the reply to the echo client if the request 
is not blocked.

Logging:
Proxy keeps track of all the requests in a log file named proxy.log. Each log file entry is of the form:

date: clientIP clientPort size echostring
   
Where clientIP is the IP address of the echo client, clientPort is the port number of the echo client, size 
is the size in bytes of the echo string that was returned from the echo server, and echostring is the echo 
string returned from the server.


Dealing with Multiple Requests Concurrently:
With this approach, it is not possible for multiple peer threads to access the log file concurrently. 
Thus, was used a semaphore to synchronize access to the file such that only one peer thread can modify it at a time. 
If do not synchronize the threads, the log file might be corrupted.
