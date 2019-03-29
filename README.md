# NetworkProject

Server and Client in C

##Server
The server can be start in TCP or UDP and can accept up to 50 clients simultaneously

Fonctionnalities:

1) Identification when client connect (credentials saved in Sqlite3 database)
2) Get list of all users connected on the server
3) Send a message to all connected users
4) Send a message to a specific user
5) Get list of files on the server
6) Upload a file to the server
7) Download a file from the server

###Client

The client can connect in UDP or TCP to the server
Use thread to be able to receive a message at any time
