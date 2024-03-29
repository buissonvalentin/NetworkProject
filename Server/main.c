#if defined (WIN32)
    #include <winsock2.h>
    typedef int socklen_t;
#elif defined (linux)
    //Socket-server.c
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/types.h>
    #include <netdb.h>
#endif

#include "sqlite3.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#define PORT 8080

#define TCP 1
#define UDP 2
#define TERMINALE 3

#define buffLength 1500
#define ERR_RCV  "Socket error receiving message\n"
#define ERR_SND  "Socket error sending message\n"

#define cmdTCreate "#Create"
#define cmdTDelete "#Delete"
#define cmdTHelp "#Help"
#define cmdTExit "#Exit"
#define cmdTShowLog "#Logs"
#define cmdTShowDatabase "#Database"

#define cmdHelp "#Help"
#define cmdExit "#Exit"
#define cmdListUsers "#ListU"
#define cmdListFiles "#ListF"
#define cmdUploadFile "#TrfU"
#define cmdDownloadFile "#TrfD"
#define cmdPrvtMsg "#Private"
#define cmdPublicMsg "#Public"
#define cmdPing "#Ring"


sqlite3 *db;
int connectIndex;
SOCKET connectedUsers[50];
char (*usernames[50])[100];

//functions
int GetTypeOfConnection();
void SetUpTCPServer();
const char* GetConnectedUsers();
int CheckCredentials(char username[], char password[]);
void ConnectUser(SOCKET sock, int *index);
void *connection_handler(void *socket_desc);
void CreateBDD();
void ShowLogs();
void ShowDatabase();
int GetUserSocketIndex(char username[]);
void SendToAll(char* msg);
const char* GetFilesList();
void Terminale();
void SetUpUDPServer();
void Log(char *type, char *content);



int main(void){
    #if defined (WIN32)
        WSADATA WSAData;
        WSAStartup(MAKEWORD(2,2), &WSAData);
    #else
        int erreur = 0;
    #endif


    int connectionType = -1;


    CreateBDD();

    // Get connection type
    connectionType = GetTypeOfConnection();



    if(connectionType == TCP){
        Log("info", "Starting TCP server");
        SetUpTCPServer();
    }
    if(connectionType == UDP){
        Log("info", "Starting UDP server");
        SetUpUDPServer();

    }
    if(connectionType == TERMINALE){
        Log("info", "Starting Terminal");
        Terminale();
    }


    return 0;
}


// Read user connection type
int GetTypeOfConnection(){

    int connectionType = -1;
    char num[1];

    while(connectionType != TCP && connectionType != UDP && connectionType != TERMINALE){
        printf("What type of connection do you want  \n1 : TCP\n2 : UDP\n3 : TERMINALE\n");
        scanf("%s", num);
        connectionType = atoi (num);
    }
    return connectionType;
}


// Initiate a TCP server
void SetUpTCPServer(){
    SOCKET listenSock;
    SOCKADDR_IN sin;
    SOCKET clientSock;
    SOCKADDR_IN csin;
    socklen_t recsize = sizeof(csin);
    int sock_err;
    int listening = 1;
    pthread_t thread_id;
    listenSock = socket(AF_INET, SOCK_STREAM, 0);

        /* Configuration */
    sin.sin_addr.s_addr    = htonl(INADDR_ANY);   /* Adresse IP automatique */
    sin.sin_family         = AF_INET;             /* Protocole familial (IP) */
    sin.sin_port           = htons(PORT);         /* Listage du port */

    sock_err = bind(listenSock, (SOCKADDR*)&sin, sizeof(sin));

    if(sock_err != SOCKET_ERROR){
        sock_err = listen(listenSock, 5);
        printf("Socket ecoute sur le port %d\n", PORT);
        Log("info", "Socket listen on port 8080");

        if(sock_err != SOCKET_ERROR){
            printf("Patientez pendant que le client se connecte sur le port %d...\n", PORT);
            while(listening > 0){
                clientSock = accept(listenSock, (SOCKADDR*)&csin, &recsize);
                printf("Connection accepted\n");
                Log("info", "Socket Connected");

                if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &clientSock) < 0)
                {
                    printf("could not create thread \n");
                    Log("error", "Could not start thread");
                }

                //Now join the thread , so that we dont terminate before the thread
                //pthread_join( thread_id , NULL);
                printf("Handler assigned\n");
                Log("info", "Thread started");
            }
        }
    }
    printf("Fermeture de la socket...\n");
    closesocket(listening);
    printf("Fermeture du serveur terminee\n");
    Log("info", "Server closed");
}


// Manage message from users
void *connection_handler(void *socket_desc){
    char buff[buffLength];
    int sock = *(int*)socket_desc;

    int index = -1;
    int finnish = -1;
    ConnectUser(sock, &index);
    printf("%s Connected\n", usernames[index]);
    char log[100] = "";
    strcpy(log, "User connected : ");
    strcat(log, usernames[index]);
    Log("info", log);



    while(finnish < 0){
        if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){

            if(strcmp(buff, cmdListUsers) == 0){ // List Users
                //list users
                printf("%s requested\n", cmdListUsers);
                char *list;
                list = GetConnectedUsers();
                char buff[buffLength]= "";
                strcat(buff, "List users : \n");
                strcat(buff, list);
                printf("List : %s", buff);

                int err = send(sock, buff, buffLength, 0);
                if(err == SOCKET_ERROR){
                    printf(ERR_SND);
                }
            }
            else if(strcmp(buff, cmdListFiles) == 0){  // List File
                // list files
                printf("%s requested", cmdListFiles);
                char *list;
                list = GetFilesList();
                char buff[buffLength]= "";
                strcat(buff, "List files : ");
                strcat(buff, list);

                int err = send(sock, buff, buffLength, 0);
                if(err == SOCKET_ERROR){
                    printf(ERR_SND);
                    Log("error", ERR_SND);
                }
            }
            else if(strcmp(buff, cmdUploadFile) == 0){ // Upload
                // upload file
                printf("%s requested", cmdUploadFile);
                char path[buffLength];
                char rcv[buffLength];

                // receive file name
                if(recv(sock, path, buffLength, 0) != SOCKET_ERROR){
                    printf("ready to receive file %s\n", path);
                    FILE *fr = fopen(path, "wb");

                    if(fr != NULL){
                        printf("File %s opened.\n", path);
                        // receive file size
                        if(recv(sock, rcv, buffLength, 0) != SOCKET_ERROR){
                            int size = atoi(rcv);
                            printf("size of file %d\n", size);

                            //receive file
                            int done = -1;
                            while(done < 0){
                                if(recv(sock, rcv, buffLength, 0) != SOCKET_ERROR){
                                    if(strcmp(rcv, "done") == 0){
                                        done = 1;
                                        printf("\nReceived done\n");
                                        Log("info", "file received");

                                    }
                                    else{
                                        fputc( rcv[0], fr);
                                        size--;
                                    }
                                }
                            }
                        }
                        fclose(fr);
                    }
                    else{
                        printf("File %s Cannot be opened.\n", path);
                        Log("error", "could not open file");
                    }
                }
            }
            else if(strcmp(buff, cmdDownloadFile) == 0){ // Download
                printf("%s requested", cmdDownloadFile);
                char file[buffLength];
                char startMsg[buffLength] = "#file";
                if(recv(sock, file, buffLength, 0) != SOCKET_ERROR){
                    char msg[buffLength];
                    char endMsg[buffLength] = "done";

                    FILE *fs = fopen(file, "rb");

                    if(fs != NULL){

                        printf("file found\n");
                        // send file name
                        send(sock, startMsg, buffLength, 0);
                        send(sock, file, buffLength, 0);

                        char get_c;
                        while ((get_c = fgetc(fs)) != EOF)
                        {
                            //size = fread(msg, buffLength, 1, fs);

                            printf("sending data : %c\n", get_c);
                            msg[0] = get_c;
                            msg[1] = '\0';
                            send(sock, msg, buffLength, 0);
                        }

                        if(send(sock, endMsg, buffLength, 0) != SOCKET_ERROR){
                            printf("sending done");
                            Log("info", "File send");
                        }


                        fclose(fs);
                    }
                    else{
                        printf("ERROR: File %s not found.\n", file);
                        Log("error", "Could Not open file");
                    }

                }
            }
            else if(strcmp(buff, cmdPrvtMsg) == 0){// private message
                printf("%s requested\n", cmdPrvtMsg);
                char userdest[buffLength];
                char message[buffLength];
                char msg[buffLength] = "Private message from ";

                if(recv(sock, userdest, buffLength, 0) != SOCKET_ERROR){
                    if(recv(sock, message, buffLength, 0) != SOCKET_ERROR){
                        strcat(msg, usernames[index]);
                        strcat(msg, " : ");
                        strcat(msg, message);
                        printf("sending %s to %s\n", msg, userdest);
                        int destIndex = GetUserSocketIndex(userdest);
                        printf("index : %d", destIndex);
                        if(destIndex > -1){
                            send(connectedUsers[destIndex], msg, buffLength, 0);
                        }

                    }
                }
            }
            else if(strcmp(buff, cmdPublicMsg) == 0){ // public message
                char buff[buffLength];
                char msg[buffLength] = "Public message from ";
                printf("%s requested", cmdPublicMsg);
                if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){
                    strcat(msg, usernames[index]);
                    strcat(msg, " : ");
                    strcat(msg, buff);
                    SendToAll(msg);
                }
            }
            else if(strcmp(buff, cmdExit) == 0){ // exit
                printf("%s requested", cmdExit);
                shutdown(sock, 2);
                finnish = 1;
            }
            else if(strcmp(buff, cmdPing) == 0){
                printf("%s requested", cmdPing);
            }
        }
    }
    printf("User disconnected : %s\n", usernames[index]);
    strcpy(log, "User disconnected : ");
    strcat(log, usernames[index]);
    Log("info", log);
    connectedUsers[index] = -1;
    printf("Thread finished\n");
}


// Create and populate database
void CreateBDD(){
    char * name_database = "database.db";
    char *err_msg = 0;
    char *sql;

    connectIndex = 0;
    int rc = sqlite3_open(name_database, &db);
    //printf("\nRC open Database = %d\n",rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
    }

    sql = "CREATE TABLE IF NOT EXISTS user(id INT, username TEXT, password TEXT, socketIndex INT);"
          "UPDATE user SET socketIndex = -1;"
          "CREATE TABLE IF NOT EXISTS log(type TEXT, content TEXT);";
                //"INSERT INTO user VALUES(1, 'v', '1', -1);"
                //"INSERT INTO user VALUES(2, 'a', '1', -1);"
                //"INSERT INTO user VALUES(3, 'b', '1', -1);"
                //"INSERT INTO user VALUES(4, 'c', '1', -1);";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    //printf("\nRC sqltite_exec = %d -> %s \n\n",rc,sql);

    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    } else {
        fprintf(stdout, "Table users created successfully\n");
    }
    printf("Database created\n");
    Log("info", "Database created");
}


// Add a log into database
void Log(char *type, char *content){
    char *sql;
    sqlite3_stmt *res;
    int step;

    sql = "INSERT INTO log (type, content) VALUES (? , ?)";
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(res, 1, type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(res, 2, content, -1, SQLITE_TRANSIENT);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    while ((step = sqlite3_step(res) == SQLITE_ROW)) {
    }
}


//Show all logs
void ShowLogs(){
    char *sql;
    sqlite3_stmt *res;
    int step;

    sql = "SELECT type, content FROM log";
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    while ((step = sqlite3_step(res) == SQLITE_ROW)) {
        printf("%s : %s\n",sqlite3_column_text(res, 0), sqlite3_column_text(res, 1));
    }
}


// Display content of database
void ShowDatabase(){
    char *sql;
    sqlite3_stmt *res;
    int step;

    sql = "SELECT id, username, password, socketIndex FROM user";
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    while ((step = sqlite3_step(res) == SQLITE_ROW)) {
        printf("id : %d; username : %s; password : %s; socketindex : %d\n",sqlite3_column_int(res, 0), sqlite3_column_text(res, 1), sqlite3_column_text(res, 2), sqlite3_column_int(res, 3));
    }
}


// Connect user into database and set socket at index
void ConnectUser(SOCKET sock, int *index){
    int connected = -1;
    while(connected < 0){
        char username[buffLength];
        char password[buffLength];
        char buff[buffLength];
        int err;

        printf("Waiting for username\n");
        err = recv(sock, username, buffLength, 0);
        if(err != SOCKET_ERROR){
            printf("Username received : %s\n", username);
            printf("Waiting for password\n");

            err = recv(sock, password, buffLength, 0);
            printf("Password received : %s\n", password);
            if(err != SOCKET_ERROR){
                printf("Tryning to connect with (%s,%s)\n", username, password);
                connected = CheckCredentials(username, password);

                printf("Connect status : %d\n", connected);
                itoa(connected, buff, 10);
                if(send(sock, buff, buffLength, 0) != SOCKET_ERROR){
                    connectedUsers[connectIndex] = sock;
                    *index = connectIndex;
                    usernames[connectIndex] = username;
                    connectIndex += 1;
                }
                else{
                    printf(ERR_SND);
                }
            }
            else{
                printf(ERR_RCV);
            }
        }
    }
}


// Check credential in db and set index in db
int CheckCredentials(char username[], char password[]){
    char *sqlSelect;
    char *sqlUpdate;
    sqlite3_stmt *resSelect;
    sqlite3_stmt *resUpdate;
    int stepS;
    int stepU;
    int result = -1;

    sqlSelect = "SELECT id FROM user WHERE username = ? AND password = ?";
    int rc = sqlite3_prepare_v2(db, sqlSelect, -1, &resSelect, 0);

    if (rc == SQLITE_OK) {
        sqlite3_bind_text(resSelect, 1, username, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(resSelect, 2, password, -1, SQLITE_TRANSIENT);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return result;
    }


    while ((stepS = sqlite3_step(resSelect) == SQLITE_ROW)) {
        printf("Match index : %d\n", sqlite3_column_int(resSelect, 0));
        printf("Index in array will  be : %d", connectIndex);
        sqlUpdate = "UPDATE user SET socketIndex = ? WHERE id = ?";
        rc = sqlite3_prepare_v2(db, sqlUpdate, -1, &resUpdate, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(resUpdate, 1, connectIndex);
            sqlite3_bind_int(resUpdate, 2, sqlite3_column_int(resSelect, 0));
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
            return result;
            Log("error", "error database");
        }

        while ((stepU = sqlite3_step(resUpdate) == SQLITE_ROW)) {
        }
        result = 1;



        break;
    }

    return result;
}


// Return a list of all connected users
const char* GetConnectedUsers(){
    char list[buffLength] = "";
    char* l = "";
    char *sql;
    int step;
    sqlite3_stmt *res;

    sql = "SELECT username FROM user WHERE socketIndex > ?";
    sqlite3_prepare_v2(db, sql, -1, &res, 0);
    sqlite3_bind_int(res, 1, -1);

    while ((step = sqlite3_step(res) == SQLITE_ROW)){
        strcat(list, sqlite3_column_text(res, 0));
        strcat(list, "\n");
    }

    l = list;
    printf("list : %s", l);
    return l;
}


// Send message to all sockets in the socket array
void SendToAll(char* msg){
    for(int i = 0; i < 50; i++){
        if(connectedUsers[i] >= 0){
            send(connectedUsers[i], msg, buffLength, 0);
        }
    }
}


// Return index of socket array of user name
int GetUserSocketIndex(char username[]){
    int index = -1;
    char *sql;
    int step;
    sqlite3_stmt *res;

    sql = "SELECT socketIndex FROM user WHERE username = ?";
    sqlite3_prepare_v2(db, sql, -1, &res, 0);
    sqlite3_bind_text(res, 1, username, -1, SQLITE_TRANSIENT);

    while ((step = sqlite3_step(res) == SQLITE_ROW)){
        index = sqlite3_column_int(res, 0);
    }

    return index;
}


// Return a list of all files in current directory
const char* GetFilesList(){
    DIR *d;
    char *listFiles = "";
    char listTemp[buffLength] = "";
    struct dirent *dir;
    d = opendir(".");
    if (d)
    {
        printf("dir opened");
        while ((dir = readdir(d)) != NULL)
        {

            strcat(listTemp, dir->d_name);
            strcat(listTemp, "\n");

        }
        closedir(d);
    }

    listFiles = listTemp;
    return listFiles;
}


// Start a terminal to update and show the database
void Terminale(){
    int listening = 1;
    while(listening){
        char command[10];


        printf("\nEnter command (Use %s for help) : ", cmdTHelp);
        scanf("%s", command);

        if(strcmp(command, cmdTExit) == 0){
            listening = 0;
            printf("Closing terminal");
            Log("info", "Terminal closed");
        }
        else if(strcmp(command, cmdTHelp) == 0){
            printf("\n%s : Close terminale\n", cmdTExit);
            printf("%s : Create a user in db\n", cmdTCreate);
            printf("%s : Delete user from db\n", cmdTDelete);
            printf("%s : Show database\n", cmdTShowDatabase);
            printf("%s : Show logs\n", cmdTShowLog);
        }
        else if(strcmp(command, cmdTCreate) == 0){

            char *sql;
            sqlite3_stmt *res;
            int step;
            char username[50];
            char password[50];

            printf("\nCreating New User\n");
            printf("Username : ");
            scanf("%s", username);
            printf("Password : ");
            scanf("%s", password);
            sql = "INSERT INTO user (username, password) VALUES (? , ?)";
            int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(res, 1, username, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(res, 2, password, -1, SQLITE_TRANSIENT);
            } else {
                fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
            }

            while ((step = sqlite3_step(res) == SQLITE_ROW)) {
            }
            Log("info", "User created");

        }
        else if(strcmp(command, cmdTDelete) == 0){
            char *sql;
            sqlite3_stmt *res;
            int step;
            char username[50];

            printf("\nDeleting User\n");
            printf("Username : ");
            scanf("%s", username);
            sql = "DELETE FROM user WHERE username = ?";
            int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(res, 1, username, -1, SQLITE_TRANSIENT);
            } else {
                fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
            }

            while ((step = sqlite3_step(res) == SQLITE_ROW)) {
            }
            Log("info", "User deleted");
        }
        else if(strcmp(command, cmdTShowLog) == 0){
            ShowLogs();
        }
        else if(strcmp(command, cmdTShowDatabase) == 0){
            ShowDatabase();
        }
        else{
            printf("Unknown Command");
        }
    }

}


// Start UDP Connection
void SetUpUDPServer(){
    SOCKET s;
	struct sockaddr_in server, si_other;
	int slen , recv_len;
	char command[buffLength];
	WSADATA wsa;
	int finnish = -1;

	slen = sizeof(si_other) ;

	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
		printf("Failed. Error Code : %d",WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//Create a socket
	if((s = socket(AF_INET , SOCK_DGRAM , 0 )) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d" , WSAGetLastError());
	}
	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( PORT );

	//Bind
	if( bind(s ,(struct sockaddr *)&server , sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d" , WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	while(finnish == -1)
	{
		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(s, command, buffLength, 0, (struct sockaddr *) &si_other, &slen)) != SOCKET_ERROR){


            if(strcmp(command, cmdListFiles) == 0){  // List File
                // list files
                printf("%s requested", cmdListFiles);
                char *list;
                list = GetFilesList();
                char buff[buffLength]= "";
                strcat(buff, "List files : ");
                strcat(buff, list);

                if (sendto(s, buff, buffLength, 0, (struct sockaddr*) &si_other, &slen) == SOCKET_ERROR)
                {
                    printf(ERR_SND);
                    Log("error", ERR_SND);
                }
            }
            else if(strcmp(command, cmdUploadFile) == 0){ // Upload
                // upload file
                printf("%s requested", cmdUploadFile);
                char path[buffLength];
                char rcv[buffLength];

                // receive file name
                if ((recv_len = recvfrom(s, path, buffLength, 0, (struct sockaddr *) &si_other, &slen)) != SOCKET_ERROR)
                {
                    printf("ready to receive file %s\n", path);
                    FILE *fr = fopen(path, "wb");

                    if(fr != NULL){
                        printf("File %s opened.\n", path);

                        //receive file
                        int done = -1;
                        while(done < 0){
                            if ((recv_len = recvfrom(s, rcv, buffLength, 0, (struct sockaddr *) &si_other, &slen)) != SOCKET_ERROR){
                                if(strcmp(rcv, "done") == 0){
                                    done = 1;
                                    printf("\nReceived done\n");
                                    Log("info", "file received");
                                }
                                else{
                                    fputc( rcv[0], fr);
                                }
                            }
                        }

                        fclose(fr);
                    }
                    else{
                        printf("File %s Cannot be opened.\n", path);
                        Log("error", "could not open file");
                    }
                }
            }
            else if(strcmp(command, cmdDownloadFile) == 0){ // Download
                printf("%s requested", cmdDownloadFile);
                char file[buffLength];

                if ((recv_len = recvfrom(s, file, buffLength, 0, (struct sockaddr *) &si_other, &slen)) != SOCKET_ERROR){
                    char msg[buffLength];
                    char endMsg[buffLength] = "done";

                    FILE *fs = fopen(file, "rb");

                    if(fs != NULL){
                        printf("file found\n");
                        // send file name

                        char get_c;
                        while ((get_c = fgetc(fs)) != EOF)
                        {
                            //size = fread(msg, buffLength, 1, fs);
                            //printf("sending data : %c\n", get_c);
                            msg[0] = get_c;
                            msg[1] = '\0';
                            sendto(s, msg, buffLength, 0, (struct sockaddr*)&si_other, &slen);
                        }

                        if (sendto(s, endMsg, buffLength, 0, (struct sockaddr*)&si_other, &slen) == SOCKET_ERROR){
                            printf("sending done");
                            Log("info", "File send");
                        }


                        fclose(fs);
                    }
                    else{
                        printf("ERROR: File %s not found.\n", file);
                        Log("error", "Could Not open file");
                    }

                }
            }
            else if(strcmp(command, cmdExit) == 0){ // exit
                printf("%s requested", cmdExit);
                finnish = 1;
            }

		}
	}

	closesocket(s);
	WSACleanup();

	return;
}
