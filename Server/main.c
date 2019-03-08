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

#define cmdTCreate "#Crt"
#define cmdTDelete "#Dlt"
#define cmdTModify "#Mdf"
#define cmdTHelp "#Help"
#define cmdTExit "#Exit"

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


//functions
int GetTypeOfConnection();
const char* GetConnectedUsers();
int CheckCredentials(char username[], char password[]);
void *connection_handler(void *socket_desc);
void CreateBDD();
void SendToAll(char* msg);
void SendToUser(char* user, char* msg);
const char* GetFilesList();
int callback(void *NotUsed, int argc, char **argv,char **azColName);



int main(void){
    #if defined (WIN32)
        WSADATA WSAData;
        int erreur = WSAStartup(MAKEWORD(2,2), &WSAData);
    #else
        int erreur = 0;
    #endif

    int temp;

    SOCKET listenSock;
    SOCKADDR_IN sin;
    SOCKET clientSock;
    SOCKADDR_IN csin;
    socklen_t recsize = sizeof(csin);
    int count = 0;
    int sock_err;
    char buffer[buffLength];
    pthread_t thread_id;

    int connectionType = -1;
    int listening = 1;


    CreateBDD();

    // Get connection type
    connectionType = GetTypeOfConnection();



    if(connectionType == TCP){
        listenSock = socket(AF_INET, SOCK_STREAM, 0);
        printf("La socket %d est ouverte en mode TCP/IP\n", listenSock);

        /* Configuration */
        sin.sin_addr.s_addr    = htonl(INADDR_ANY);   /* Adresse IP automatique */
        sin.sin_family         = AF_INET;             /* Protocole familial (IP) */
        sin.sin_port           = htons(PORT);         /* Listage du port */

        sock_err = bind(listenSock, (SOCKADDR*)&sin, sizeof(sin));

        if(sock_err != SOCKET_ERROR){
            sock_err = listen(listenSock, 5);
            printf("Socket ecoute sur le port %d\n", PORT);

            if(sock_err != SOCKET_ERROR){
                printf("Patientez pendant que le client se connecte sur le port %d...\n", PORT);
                while(listening > 0){
                    clientSock = accept(listenSock, (SOCKADDR*)&csin, &recsize);
                    printf("Connection accepted\n");

                    if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &clientSock) < 0)
                    {
                        printf("could not create thread\n");
                        return 1;
                    }

                    //Now join the thread , so that we dont terminate before the thread
                    //pthread_join( thread_id , NULL);
                    printf("Handler assigned\n");
                }
            }
        }
        printf("Fermeture de la socket...\n");
        closesocket(listening);
        printf("Fermeture du serveur terminee\n");
    }
    if(connectionType == UDP){

    }
    if(connectionType == TERMINALE){
        while(listening){
            char command[10];
            printf("\nEnter command (Use %s for help) : ", cmdTHelp);
            scanf("%s", command);

            if(strcmp(command, cmdTExit) == 0){
                listening = 0;
                printf("Closing terminale");
            }
            else if(strcmp(command, cmdTHelp) == 0){
                printf("\n%s : Close terminale\n", cmdTExit);
                printf("%s : Create a user in db\n", cmdTCreate);
                printf("%s : Delete user from db\n", cmdTDelete);
                printf("%s : modify a user in db\n", cmdTModify);
            }
            else if(strcmp(command, cmdTCreate) == 0){

                char username[50];
                char password[50];
                printf("\nCreating New User");
                printf("\nUsername : ");
                scanf("%s", username);
                printf("\Password : ");
                scanf("%s", password);

            }
            else if(strcmp(command, cmdTDelete) == 0){

            }
            else if(strcmp(command, cmdTModify) == 0){

            }
            else{
                printf("Unknown Command");
            }
        }
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


// Manage message from users
void *connection_handler(void *socket_desc){
    char buff[buffLength];
    int sock = *(int*)socket_desc;

    int index = -1;
    int finnish = -1;
    int err = -1;
    ConnectUser(sock, &index);
    //printf("%s Connected\n", bddUsernames[index]);



    while(finnish < 0){
        if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){

            if(strcmp(buff, cmdListUsers) == 0){
                printf("%s requested\n", cmdListUsers);

                char *list;
                list = GetConnectedUsers();
                char buff[buffLength]= "";
                strcat(buff, "List users : ");
                strcat(buff, list);
                printf("%s", buff);

                int err = send(sock, buff, buffLength, 0);
                if(err == SOCKET_ERROR){
                    printf(ERR_SND);
                }
            }
            else if(strcmp(buff, cmdListFiles) == 0){
                printf("%s requested", cmdListFiles);
                char *list;
                list = GetFilesList();
                char buff[buffLength]= "";
                strcat(buff, "List files : ");
                strcat(buff, list);

                int err = send(sock, buff, buffLength, 0);
                if(err == SOCKET_ERROR){
                    printf(ERR_SND);
                }
            }
            else if(strcmp(buff, cmdUploadFile) == 0){
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

                                    }
                                    else{
                                        printf("received : %s", rcv);
                                        fputc( rcv[0], fr);
                                        size -= (sizeof(rcv)/sizeof(rcv[0]));
                                        printf("remaining bytes %d\n", size);
                                    }
                                }
                            }
                        }
                        fclose(fr);
                    }
                    else{
                        printf("File %s Cannot be opened.\n", path);
                    }
                }
            }
            else if(strcmp(buff, cmdDownloadFile) == 0){
                printf("%s requested", cmdDownloadFile);
            }
            else if(strcmp(buff, cmdPrvtMsg) == 0){
                printf("%s requested\n", cmdPrvtMsg);
                char userdest[buffLength];
                char message[buffLength];
                char msg[buffLength] = "Private message from ";
                if(recv(sock, userdest, buffLength, 0) != SOCKET_ERROR){
                    if(recv(sock, message, buffLength, 0) != SOCKET_ERROR){
//                        strcat(msg, bddUsernames[index]);
                        strcat(msg, " : ");
                        strcat(msg, message);
                        printf("sending %s to %s\n", msg, userdest);
                        SendToUser(userdest, msg);
                    }
                }
            }
            else if(strcmp(buff, cmdPublicMsg) == 0){
                char buff[buffLength];
                char msg[buffLength] = "Public message from ";
                printf("%s requested", cmdPublicMsg);
                if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){
//                    strcat(msg, bddUsernames[index]);
                    strcat(msg, " : ");
                    strcat(msg, buff);
                    SendToAll(msg);
                }
            }
            else if(strcmp(buff, cmdExit) == 0){
                printf("%s requested", cmdExit);
                shutdown(sock, 2);
                finnish = 1;
            }
            else if(strcmp(buff, cmdPing) == 0){
                printf("%s requested", cmdPing);
            }
        }
    }
    connectedUsers[index] = -1;
//    printf("%s Disconnected", bddUsernames[index]);

}


// Create and populate database
void CreateBDD(){
    char * name_database = "database.db";
    char *err_msg = 0;
    char *sql;

    connectIndex = 0;
    int rc = sqlite3_open(name_database, &db);
    printf("\nRC open Database = %d\n",rc);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    sql = "DROP TABLE IF EXISTS user;"
                "CREATE TABLE user(id INT, username TEXT, password TEXT, socketIndex INT);"
                "INSERT INTO user VALUES(1, 'v', '1', -1);"
                "INSERT INTO user VALUES(2, 'a', '1', -1);"
                "INSERT INTO user VALUES(3, 'b', '1', -1);"
                "INSERT INTO user VALUES(4, 'c', '1', -1);";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    printf("\nRC sqltite_exec = %d -> %s \n\n",rc,sql);

    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    } else {
        fprintf(stdout, "Table users created successfully\n");
    }
}


// Connect user into database and set socket at index
void ConnectUser(SOCKET sock){
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
                    connectIndex++;
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
    char *err_msg = 0;
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
        }

        while ((stepU = sqlite3_step(resUpdate) == SQLITE_ROW)) {
        }
        result = 1;



        break;
    }

    return result;
}

const char* GetConnectedUsers(){
    printf("Getting list");
    char *listU;
    char *sql;
    int step;
    sqlite3_stmt *res;

    sql = "SELECT username FROM user WHERE socketIndex > ?";
    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    sqlite3_bind_int(res, 1, 0);

    while ((step = sqlite3_step(res) == SQLITE_ROW)){
        printf("%s",sqlite3_column_text(res, 0));
        strcat(listU, sqlite3_column_text(res, 0));
        strcat(listU, "\n");
    }

    return listU;
}

void SendToAll(char* msg){
    for(int i = 0; i < 50; i++){
        if(connectedUsers[i] >= 0){
            send(connectedUsers[i], msg, buffLength, 0);
        }
    }
}

void SendToUser(char* user, char* msg){
    for(int i = 0; i < 50; i++){
       // if(strcmp(user, *bddUsernames[i]) == 0)
       {
            if(send(connectedUsers[i],msg, buffLength, 0) == SOCKET_ERROR){
                printf(ERR_SND);
            }
        }
    }
}

const char* GetFilesList(){
    DIR *d;
    char *listFiles;
    struct dirent *dir;
    d = opendir(".");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            //printf("%s\n", dir->d_name);
            strcat(listFiles, dir->d_name);
            strcat(listFiles, "\n");

        }
        closedir(d);
    }

    return listFiles;
}



int callback(void *NotUsed, int argc, char **argv,char **azColName)
{
    printf("call back 1");
    NotUsed = 0;
    for (int i = 0; i < argc; i++) {
        printf(" %s = %s ", azColName[i], argv[i] ? argv[i] : "NULL");
     }
    printf("\n");
    return 0;
}


