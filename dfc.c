
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>

#define MAXBUF 4096
#define CONF_FILENAME "dfc.conf"

struct config //ws.conf parsed map
{
   char *server;
   char *server_addr;
   char *server_port;
   char *username;
   char *password;
   int number_of_users;
   int number_of_servers;
}; 
struct config get_conf_parameters(char *filename) //parse configuration file to structures
{
    struct config configstruct;
    char *token;
    configstruct.number_of_users = 0;
    configstruct.number_of_servers = 0;
    FILE *file = fopen (filename, "r");
    int user_index = 0;
    int server_index = 0;
    if(file == NULL)
    {
        perror("File not opened :");
        exit(-1);
    }

    if (file != NULL)
    {
        char line[MAXBUF];
        while(fgets(line, sizeof(line), file) != NULL)
        {
            char *cfline;
            if( strchr(line, '#') !=NULL)
            {
                continue;
            }
            else
            {
                if( strstr(line, "Server") != NULL) //get server address and port number
                {
                    token = strtok(line, " ");
                    int count = 0;
                    char temp_server[100];
                    while(token != NULL)
                    {
                        if(count == 1)
                        {
                            strcpy(configstruct.server[server_index], token);
                        }
                        else if(count == 2)
                        {
                            bzero(temp_server, sizeof(temp_server));
                            strcpy(temp_server,token);
                        }
                        token = strtok(NULL, " ");
                        count++;
                     
                    }
                    count = 0;
                    token = strtok(temp_server, ":");
                    while(token != NULL)
                    {
                        if(count = 0)
                        {
                            strcpy(configstruct.server_addr[server_index], token);
                        }
                        else if(count == 1)
                        {
                            strcpy(configstruct.server_port[server_index], token);
                        }
                        token = strtok(NULL, ":");
                        count++;
                    }
                    configstruct.number_of_servers++;
                    server_index ++;
                }
                else if( strstr(line, "Username") != NULL) //get username and password
                {
                    int count = 0;
                    token = strtok(line, ":");
                    while(token != NULL)
                    { 
                        if(count == 1)
                        {
                            strcpy(configstruct.username[user_index], token);
                        }
                        token = strtok(NULL, ":");
                        count++;                          
                    }
                    count = 0;
                    if(fgets(line, sizeof(line), file) != NULL)
                    {
                        if(strstr(line, "Password") != NULL)
                        {
                            token = strtok(line,":");
                            while(token != NULL)
                            {
                               if(count == 1)
                               {
                                   strcpy(configstruct.password[user_index], token);
                               }
                               token = strtok(NULL, ":");
                               count++; 
                            }
                        }
                        else
                        {
                            printf("%s username does not have a password, please assign a password and try again\n", configstruct.username[user_index]);
                            exit(-1);
                        }
                    }
                    user_index++;
                    configstruct.number_of_users ++;                                            
                }
            }             
           
        } // End while
        fclose(file);
        int i = 0;
        printf("==========Contents of conf file===========\n");
        for(i = 0; i< configstruct.number_of_servers; i++)
        {
            printf("ServerName: %s, ServerAddress: %s, ServerPort: %s\n", configstruct.server[i], configstruct.server_addr[i], configstruct.server_port[i]);
        } 
        for(i = 0; i< configstruct.number_of_users; i++)
        {
            printf("Username: %s, Password: %s\n", configstruct.username[i], configstruct.password[i]);
        } 
           
                           
    } // End if file
       
    return configstruct;

}

int main(int argc , char *argv[])
{
    int sock;
    struct config configstruct;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000];
    configstruct = get_conf_parameters(CONF_FILENAME);
     
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 8888 );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }
     
    puts("Connected\n");
     
    //keep communicating with server
    while(1)
    {
        printf("Enter message : ");
        scanf("%s" , message);
         
        //Send some data
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
         
        //Receive a reply from the server
        /*if( recv(sock , server_reply , 2000 , 0) < 0)
        {
            puts("recv failed");
            break;
        }
         
        puts("Server reply :");
        puts(server_reply);*/
    }
     
    close(sock);
    return 0;
}
