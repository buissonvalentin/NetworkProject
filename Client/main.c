#if defined (WIN32)
    #include <winsock2.h>
    typedef int socklen_t;
#elif defined (linux)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket(s) close(s)
    typedef int SOCKET;
    typedef struct sockaddr_in SOCKADDR_IN;
    typedef struct sockaddr SOCKADDR;
#endif

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define PORT 8080

#define cmdHelp "#Help"
#define cmdExit "#Exit"
#define cmdListUsers "#ListU"
#define cmdListFiles "#ListF"
#define cmdUploadFile "#TrfU"
#define cmdDownloadFile "#TrfD"
#define cmdPrvtMsg "#Private"
#define cmdPublicMsg "#Public"
#define cmdPing "#Ring"

#define TCP 1
#define UDP 2

#define buffLength 1500
#define ERR_RCV "Socket error receiving message\n"
#define ERR_SND "Socket error sending message\n"

//functions
void *messageListener(void *socket_desc);

int main(void)
{
     #if defined (WIN32)
        WSADATA WSAData;
        int erreur = WSAStartup(MAKEWORD(2,2), &WSAData);
    #else
        int erreur = 0;
    #endif

    SOCKET sock;
    SOCKADDR_IN sin;
    char buffer[buffLength];


    int connectionType = -1;
    pthread_t thread_id;
    // Get connection type
    connectionType = GetTypeOfConnection();

    if(connectionType == TCP){
        if(!erreur){
            sock = socket(AF_INET, SOCK_STREAM, 0);

            /* Configuration de la connexion */
            sin.sin_addr.s_addr = inet_addr("127.0.0.1");
            sin.sin_family = AF_INET;
            sin.sin_port = htons(PORT);

            if(connect(sock, (SOCKADDR*)&sin, sizeof(sin)) != SOCKET_ERROR){
                printf("Connection a %s sur le port %d\n", inet_ntoa(sin.sin_addr), htons(sin.sin_port));

                Connect(sock);
                printf("Connected");
                if( pthread_create( &thread_id , NULL ,  messageListener , (void*) &sock) < 0)
                {
                    printf("could not create thread\n");
                    return 1;
                }
                ReadUserCommand(sock);

            }
            else{
                printf("Impossible de se connecter\n");
            }
            closesocket(sock);
        }
    }


    return 0;
}


// read user connection type
int GetTypeOfConnection(){

    int connectionType = -1;
    char num[1];

    while(connectionType != TCP && connectionType != UDP){
        printf("What type of connection do you want  1 : TCP   2 : UDP \n");
        scanf("%s", num);
        connectionType = atoi (num);
    }
    return connectionType;
}

// Loop to read user util exit
void ReadUserCommand(SOCKET sock){
    int end = -1;
    int err = -1;

    while(end < 0){

        char input[100];

        printf("\nEntrez votre commande(Use %s for help) : ", cmdHelp);
        scanf("%s", input);

        if(strcmp(input, cmdExit) == 0){
            if(send(sock, cmdExit, buffLength, 0) == SOCKET_ERROR){
                printf(ERR_SND);
            }
            end = 0;
            printf("Closing connection\n");
            closesocket(sock);
        }
        else if(strcmp(input, cmdHelp) == 0){
            printf("%s : Close Connection\n", cmdExit);
            printf("%s : List all users on server\n", cmdListUsers);
            printf("%s : List all files on server\n", cmdListFiles);
            printf("%s : Upload a file to server\n", cmdUploadFile);
            printf("%s : Download a file from server\n", cmdDownloadFile);
            printf("%s : Send a message to a user\n", cmdPrvtMsg);
            printf("%s : Send a message to all users\n", cmdPublicMsg);
            printf("%s : Notification if user is connected\n", cmdPing);
        }
        else if(strcmp(input, cmdListUsers) == 0){
            // List users
            if(send(sock, cmdListUsers, buffLength, 0) != SOCKET_ERROR){
                if(err != SOCKET_ERROR){
                }
                else{
                    printf(ERR_RCV);
                }
            }
            else{
                printf(ERR_SND);
            }
        }
        else if(strcmp(input, cmdListFiles) == 0){
            // List files
            char buff[buffLength];
            if(send(sock, cmdListFiles, buffLength, 0) != SOCKET_ERROR){
            }
            else{
                printf(ERR_SND);
            }
        }
        else if(strcmp(input, cmdUploadFile) == 0){
            // UploadFile
            if(send(sock, cmdUploadFile, buffLength, 0) != SOCKET_ERROR){
                char path[buffLength];
                char msg[buffLength];
                char endMsg[buffLength] = "done";
                int size;
                int remain_data;
                printf("File path : ");
                scanf("%s", path);
                FILE *fs = fopen(path, "rb");

                if(fs != NULL)
                {
                    printf("file found\n");
                    // send file name
                    if(send(sock, path, buffLength, 0) != SOCKET_ERROR){

                        fseek(fs, 0L, SEEK_END);
                        size = ftell(fs);
                        fseek(fs, 0L, SEEK_SET);

                        printf("File Size: %d bytes\n", size);

                        itoa(size, msg, 10);
                        // send  size
                        if(send(sock, msg, buffLength, 0) != SOCKET_ERROR){

                            remain_data = size;
                            char get_c;
                            // send file
                            while ((get_c = fgetc(fs)) != EOF)
                            {
                                //size = fread(msg, buffLength, 1, fs);

                                printf("sending data : %c\n", get_c);
                                msg[0] = get_c;
                                msg[1] = '\0';
                                if(send(sock, msg, buffLength, 0) != SOCKET_ERROR){
                                    //printf("sending %s", msg);
                                }
                                remain_data--;
                                printf("byte remaining to send : %d\n", remain_data);
                            }

                            if(send(sock, endMsg, buffLength, 0) != SOCKET_ERROR){
                                printf("sending done");
                            }
                        }
                    }
                    fclose(fs);
                }
                else{
                    printf("ERROR: File %s not found.\n", path);
                }
            }
        }
        else if(strcmp(input, cmdDownloadFile) == 0){
            // Download File
            send(sock, cmdDownloadFile, buffLength, 0);
            char file[buffLength];
            printf("File to download ! ");
            scanf("%s", file);
            send(sock, file, buffLength, 0);
        }
        else if(strcmp(input, cmdPublicMsg) == 0){

            // Public Message
            char message[buffLength];
            printf("Message : ");
            scanf("%s", message);

            if(send(sock, cmdPublicMsg, buffLength, 0) != SOCKET_ERROR){
                if(send(sock, message, buffLength, 0) == SOCKET_ERROR){
                    printf(ERR_SND);
                }
            }
            else{
                printf(ERR_SND);
            }
        }
        else if(strcmp(input, cmdPrvtMsg) == 0){
            //Private message
            char userDest[buffLength];
            char message[buffLength];
            printf("To : ");
            scanf("%s", userDest);

            printf("Message : ");
            scanf("%s", message);

            if(send(sock, cmdPrvtMsg, buffLength, 0) != SOCKET_ERROR){
                if(send(sock, userDest, buffLength, 0) != SOCKET_ERROR){
                    if(send(sock, message, buffLength, 0) == SOCKET_ERROR){
                        printf(ERR_SND);
                    }
                }
                else{

                    printf(ERR_SND);
                }
            }
            else{
                printf(ERR_SND);
            }
        }
        else if(strcmp(input, cmdPing) == 0){

        }
        else{
            printf("Unknown command. Use #Help to see list of command\n");
        }
    }
}


// Connect user to access server
void Connect(SOCKET sock){
    int connected = -1;
    int err;
    char username[buffLength];
    char password[buffLength];
    char buff[buffLength];
    char sep = 'H';

    while(connected < 0){
        //read username
        printf("Enter Username :");
        scanf("%s", username);

        if(send(sock, username, buffLength, 0) != SOCKET_ERROR){

            printf("Enter Password :");
            scanf("%s", password);
            if(send(sock, password, buffLength, 0) != SOCKET_ERROR){
                err = recv(sock, buff, buffLength, 0);
                if(err != SOCKET_ERROR){
                    printf("connection status: %s", buff);
                    //parse response to int
                    connected = atoi (buff);
                    if(connected < 0) printf("Invalid Credentials\n");
                }
                else{
                    printf(ERR_RCV);
                }
            }
            else{
                printf(ERR_SND);
            }
        }
        else{
            printf(ERR_SND);
        }
    }
}

// Thread that listen and display every message received
void *messageListener(void *socket_desc){
    printf("Thread Started\n");
    char buff[buffLength];
    int sock = *(int*)socket_desc;

    while(1){
        if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){
            if(strcmp(buff, "#file") == 0){
                //receive file name
                if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){
                    printf("ready to receive file %s\n", buff);
                    FILE *fr = fopen(buff, "wb");

                    if(fr != NULL){
                        printf("File %s opened.\n", buff);

                        //receive file
                        int done = -1;
                        while(done < 0){
                            if(recv(sock, buff, buffLength, 0) != SOCKET_ERROR){
                                if(strcmp(buff, "done") == 0){
                                    done = 1;
                                    printf("\nReceived done\n");
                                }
                                else{
                                    fputc( buff[0], fr);
                                }
                            }
                        }
                        fclose(fr);
                    }
                    else{
                        printf("File %s Cannot be opened.\n", buff);
                    }
                }

            }
            else{
                printf("\n%s", buff);
            }
        }
    }
}





