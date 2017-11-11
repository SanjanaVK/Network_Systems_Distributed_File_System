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
#define MAXBUF 1024
#define FILENAME "ws.conf" /*Configuration file*/

int main()
{
    int listenfd, connfd, n;
    pid_t childpid;
    socklen_t clilen;
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
    servaddr.sin_port = htons(10003);
	
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
            while ( (n = recv(connfd, buf, MAXLINE,0)) > 0)  
	    {
                printf("%s","String received from client:\n"); //get request from the client
                puts(buf);
                        
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

