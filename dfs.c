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
#define FILENAME "ws.conf" /*Configuration file*/

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
};
struct packet_t sender_packet, receiver_packet;

struct config //ws.conf parsed map
{

   char* username[10];
   char* password[10];
};

int main()
{
    int listenfd, connfd, n;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in remote;     //"Internet socket address structure"
    unsigned int remote_length;  
    char buf[MAXLINE];
    struct sockaddr_in cliaddr, servaddr;
     
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
		    
                printf("size of receiver_packet is %d\n", sizeof(receiver_packet));
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

