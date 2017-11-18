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

#define MAXLINE 4096 /*max text line length*/
#define LISTENQ 8 /*maximum number of client connections*/
#define MAXBUF 4096
#define FILENAME "dfs.conf" /*Configuration file*/

struct packet_t{
    int first_chunk_number;
    unsigned int first_datasize;
    char first_data[MAXBUF];
    int second_chunk_number;
    unsigned int second_datasize;
    char second_data[MAXBUF];
    char command[50];
    char filename[50];
    int valid;
    char username[100];
    char password[100];
};
struct packet_t sender_packet, receiver_packet;

struct config //ws.conf parsed map
{

   char* username[10];
   char* password[10];
   int number_of_users;
};

struct config get_conf_parameters(char *filename) //parse configuration file to structures
{
    struct config configstruct;
    char *token;
    configstruct.number_of_users = 0;
    
    FILE *file = fopen (filename, "r");
    int user_index = 0;
   
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
                token = strtok(line, "\n");                   
                while(token != NULL)
                {
                    int count = 0;
                    char * tok;
                    tok = strtok(token, " ");
                    while(tok != NULL)
                    {
                        if(count == 0)
                        {
                            configstruct.username[configstruct.number_of_users] = malloc(50 * sizeof(char));
                            strcpy(configstruct.username[configstruct.number_of_users],tok);
                        }
                        else if(count == 1)
                        {
                            configstruct.password[configstruct.number_of_users] = malloc(50 * sizeof(char));
                            strcpy(configstruct.password[configstruct.number_of_users],tok);
                        }
                        tok = strtok(NULL, " ");
                        count++;
                    }
                    configstruct.number_of_users++;
                    token = strtok(NULL, "\n");
                }
            }             
           
        } // End while
        fclose(file);
        int i = 0;
        printf("==========Contents of conf file===========\n");
        for(i = 0; i< configstruct.number_of_users; i++)
        {
           printf("Username:%s Password: %s\n", configstruct.username[i],configstruct.password[i]);
        } 
    } // End if file
       
    return configstruct;
}

int check_cred_match(struct config configstruct, struct packet_t receiver_packet)
{
    int i = 0;
    printf("username : %s\n", receiver_packet.username);
    printf("password : %s\n", receiver_packet.password);
    for(i = 0; i< configstruct.number_of_users; i++)
    {
        if(strcmp(receiver_packet.username, configstruct.username[i]) == 0)
        {
            if(strcmp(receiver_packet.password, configstruct.password[i]) == 0)
            {
                return 1;
            }
         }
     }
     return 0;
}
int main()
{
    int listenfd, connfd, n;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in remote;     //"Internet socket address structure"
    unsigned int remote_length;  
    char buf[MAXLINE];
    struct sockaddr_in cliaddr, servaddr;
    struct config configstruct;
    configstruct = get_conf_parameters(FILENAME); // get config parameters
    
    char file_extension_type[50];
    //Create a socket for the soclet
    //If sockfd<0 there was an error in the creation of the socket
    if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) <0) 
    {
        perror("Problem in creating the socket");
        exit(2);
    }
    //preparation of the socket address
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(10001);
	
    //bind the socket
    if(bind (listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        printf("Unable to bind");
    //listen to the socket by creating a connection queue, then wait for clients
    listen (listenfd, LISTENQ);
    printf("\n%s\n","Server running...waiting for connections.");
    for(; ;)
    {
        clilen = sizeof(cliaddr);
	//accept a connection
	connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);
        remote_length = sizeof(cliaddr);
	printf("%s\n","Received request...");
	if ( (childpid = fork ()) == 0 ) 
	{
	    //if it’s 0, it’s child process
	    printf ("%s\n","Child created for dealing with client requests");
            //close listening socket
	    close (listenfd);
            int result=0;
            int fifo_index = 0;
            int other_request = 0;
            char first_chunk[10000];
            char second_chunk[10000];
            while (recvfrom(connfd, &receiver_packet, sizeof(receiver_packet),0,(struct sockaddr *)&cliaddr , &remote_length) > 0) 
            {
                if(check_cred_match(configstruct, receiver_packet) == 0)
                {
                    printf("credentials do not match\n");
                    break;
                }
                printf("Credentials match\n");    
                printf("Size of receiver_packet is %d\n", sizeof(receiver_packet));
                printf("%s","String received from client:\n"); //get request from the client
                printf("1. chunk number is %d\n",receiver_packet.first_chunk_number);
                strcpy(first_chunk, receiver_packet.first_data);
                puts(receiver_packet.first_data);
                printf("2. chunk number is %d\n",receiver_packet.second_chunk_number);
                strcpy(second_chunk, receiver_packet.second_data);
                puts(receiver_packet.second_data);
                puts(buf);
                
               /* if(receiver_packet.chunk_number == 2)
                {
                   strcpy(second_chunk, receiver_packet.data);
                   puts(receiver_packet.data); 
                }*/
            }
	        //close(connfd);
                //printf("Closing socket.........\n");
                exit(0);
             
            }  
        //close socket of the server
        close(connfd);
        printf("socket closed\n");
    }
    
}




