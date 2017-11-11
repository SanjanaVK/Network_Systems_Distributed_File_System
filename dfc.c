
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
#include <openssl/md5.h>

#define MAXBUF 4096
#define CONF_FILENAME "dfc.conf"
#define FILENAME "foo3"

struct config //ws.conf parsed map
{
   char* server[100];
   char* server_addr[100];
   char* server_port[100];
   char* username[100];
   char* password[100];
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
    printf("Reading conf file.......\n");

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
                            configstruct.server[server_index] = malloc(50 * sizeof(char));
                            strcpy(configstruct.server[server_index],token);
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
                        if(count == 0)
                        {
                            configstruct.server_addr[server_index] = malloc(50 * sizeof(char));
                            strcpy(configstruct.server_addr[server_index], token);
                            
                        }
                        else if(count == 1)
                        {
                            configstruct.server_port[server_index] = malloc(50 * sizeof(char));
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
                            configstruct.username[user_index] = malloc(50 * sizeof(char));
                            strcpy(configstruct.username[user_index] , token);
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
                                   configstruct.password[user_index] = malloc(50 * sizeof(char));
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
            printf("Username:%s Password: %s\n", configstruct.username[i],configstruct.password[i]);
        } 
           
                           
    } // End if file
       
    return configstruct;

}

/*This function calculates the filesize and calculates number of packets*/
unsigned long calculate_filesize(FILE *temp_fp)
{
    fseek(temp_fp, 0L, SEEK_END);
    unsigned long temp_filesize = ftell(temp_fp); //get file size
    printf("File size of the file to be transmitted is %ld\n", temp_filesize);
    fseek(temp_fp, 0L, SEEK_SET);
    int temp_bufsize = MAXBUF;
    unsigned int number_of_packets = (temp_filesize/(temp_bufsize));//get number of packets
    if ((temp_filesize % MAXBUF) != 0);
        number_of_packets ++;  
    return temp_filesize; //return number of packets
    
}

int check_md5sum(char * filename)
{
    int fd, nbytes;
    int result = 0;
    char buffer[MAXBUF];
    FILE *temp_fp;
    unsigned long size;
    temp_fp = fopen(filename, "r");
    if (temp_fp == NULL)
    {
        perror("File not opened : ");
        return -1;
    }
    size = calculate_filesize(temp_fp) ;
    fclose(temp_fp);
    printf("File size %ld\n", size);
    int temp_size, i;
    int size_array[4];
    int rem_size = size % 4;
    printf("rem size is %d\n", rem_size);
    temp_size= size/4;
    for(i= 0 ; i< 4; i++)
    {
        size_array[i] = temp_size;
    }
    size_array[3] = temp_size + rem_size;
    printf("new array size %d\n", size_array[3]);
    for(i=0; i<4; i++)
    {
        printf("filesize %d is %d\n", i, size_array[i]);
    } 
    
    fd = open(filename , O_RDONLY); //file open with read only option
    if(fd == -1)
    {
        perror("File not opened: ");
        return -1;
    }
    
    int n;
    MD5_CTX c;
    char buf[512];
    ssize_t bytes;
    unsigned char out[MD5_DIGEST_LENGTH];

    MD5_Init(&c);
    bytes=read(fd, buf, 512);
    while(bytes > 0)
    {
        MD5_Update(&c, buf, bytes);
        bytes=read(fd, buf, 512);
    }

    MD5_Final(out, &c);

    for(n=0; n<MD5_DIGEST_LENGTH; n++)
        printf("%02x", out[n]);
    printf("\n");
    result = (out[MD5_DIGEST_LENGTH - 1] % 4);
}

int main(int argc , char *argv[])
{
    int sock[4];
    struct config configstruct;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000];
    int md5_mod;
    int i = 0;
    int port_int[4];
    configstruct = get_conf_parameters(CONF_FILENAME);
    md5_mod = check_md5sum(FILENAME);
    printf("md5 mod result is %d\n", md5_mod);
    //Create socket
    for ( i = 0 ; i<4; i++)
    {
    sock[i] = socket(AF_INET , SOCK_STREAM , 0);
    if (sock[i] == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    port_int[i] = atoi(configstruct.server_port[i]);
    server.sin_port = htons( port_int[i] );
 
    //Connect to remote server
    if (connect(sock[i] , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }
     
    puts("Connected\n");
    }
     
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
