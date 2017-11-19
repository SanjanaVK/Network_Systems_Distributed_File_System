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
#include <memory.h>
#include <dirent.h>

#define MAXLINE 10 * 4096 /*max text line length*/
#define LISTENQ 8 /*maximum number of client connections*/
#define MAXBUF 10 * 4096
#define FILENAME "dfs.conf" /*Configuration file*/

static const struct packet_t EmptyStruct; 

struct packet_t{
    int first_chunk_number;
    unsigned int first_datasize;
    char first_data[MAXBUF];
    int second_chunk_number;
    unsigned int second_datasize;
    char second_data[MAXBUF];
    char command[50];
    char filename[50];
    char subfolder[50];
    int valid;
    char error_data[100];
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

/*This function provides data encryption and decryption*/
char * encryptdecrypt(char * message, int length)
{
    int i;
    char key = 'S';
    for(i=0; i < length; i++)
    {
        message[i] ^= key; //Bitwise XOR with character 'S'
    }
    return message;
}
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

void check_and_create_directory(char * directory, struct packet_t receiver_packet, char * fullpath)
{

    strcpy(fullpath, ".");
    strcat(fullpath, directory);
    strcat(fullpath,"/");
    strcat(fullpath, receiver_packet.username);
    strcat(fullpath,"/");
    struct stat st = {0};

    printf("full path is %s\n", fullpath);
    if (stat(fullpath, &st) == -1) 
    {
        mkdir(fullpath, 0700);
        printf("full path is %s\n", fullpath);
    }
    if(strcmp(receiver_packet.command,"MKDIR") == 0 )
    {
        strcat(fullpath, receiver_packet.subfolder);
        if (stat(fullpath, &st) == -1) 
        {
            mkdir(fullpath, 0700);
            printf("full path in MKDIR in %s\n", fullpath);
        }
    }
    if((strcmp(receiver_packet.command, "MKDIR") != 0 ) && strlen(receiver_packet.subfolder) != 0)
    {
        strcat(fullpath, receiver_packet.subfolder);
        if (stat(fullpath, &st) == -1)
        {
            printf("subfolder %s not present\n", receiver_packet.subfolder);
            return;
        } 
        
    }
    return;
    
}

/*This function calculates the filesize and calculates number of packets*/
unsigned long calculate_filesize(char * filename)
{
    FILE *temp_fp = fopen(filename, "r");
    if (temp_fp == NULL)
    {
        perror("File not opened : ");
        return -1;
    }
    fseek(temp_fp, 0L, SEEK_END);
    unsigned long temp_filesize = ftell(temp_fp); //get file size
    printf("File size of the file to be transmitted is %ld\n", temp_filesize);
    fseek(temp_fp, 0L, SEEK_SET);  
    fclose(temp_fp);
    return temp_filesize; //return number of packets
   
}

/*This function gets list of all files from server*/ 
void get_list_of_files(char * directory, int connfd, struct sockaddr_in cliaddr)
{
    DIR *d;
    struct dirent *dir;
    int file_count  = 0;
    unsigned int remote_length = sizeof(cliaddr);
    char fullpath[100];
    bzero(fullpath, sizeof(fullpath));
    check_and_create_directory(directory, receiver_packet, fullpath);
    if ((d = opendir(fullpath)) == NULL)
    {
        perror("Error: Could not open directory :");
    }
    bzero(sender_packet.first_data, sizeof(sender_packet));
    if(d)
    {
        if( (dir = readdir(d)) != NULL) //read current directory
        {
            strcpy(sender_packet.first_data,dir->d_name);
            file_count++;
            while( (dir =readdir(d)) != NULL)
            {
                printf("%s\n", dir->d_name);
                //get a string of all filenames separated by "#"
                strcat(sender_packet.first_data, "#");
                strcat(sender_packet.first_data,dir->d_name);
                file_count++;
            }  
            sender_packet.first_datasize = file_count;
            closedir(d);
            printf("Number files in server directory is %d\n", file_count);
            if(sendto(connfd, &sender_packet, sizeof(sender_packet), 0, (struct sockaddr *)&cliaddr, remote_length) == -1) //send list of files to client
                perror("sendto:");
        } 
        else
            perror("Error: Reading directory: ");
    }
         
}
void put_file(struct config configstruct, char * directory)
{
     char fullpath[100];
     char first_chunk[MAXBUF];
     char second_chunk[MAXBUF];
     printf("filename is %s\n", receiver_packet.filename);
     bzero(fullpath, sizeof(fullpath));
     check_and_create_directory(directory, receiver_packet, fullpath);
     printf("full path is %s\n", fullpath);
                
     int fd_write1, fd_write2;
     char filename1[100];
     char filename2[100];
     strcpy(filename1, fullpath);
     strcat(filename1, ".");
     strcat(filename1, receiver_packet.filename);
     strcat(filename1, ".");
     char chunk_string[10];
     bzero(chunk_string, sizeof(chunk_string));
     sprintf( chunk_string, "%d", receiver_packet.first_chunk_number);
     strcat(filename1, chunk_string);               
     printf("filename is %s\n", filename1);

     strcpy(filename2, fullpath);
      strcat(filename2, ".");
     strcat(filename2, receiver_packet.filename);
     strcat(filename2, ".");
     bzero(chunk_string, sizeof(chunk_string));
     sprintf( chunk_string, "%d", receiver_packet.second_chunk_number);
     strcat(filename2, chunk_string);
                
     printf("filename is %s\n", filename2);

     fd_write1 = open( filename1, O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
     fd_write2 = open( filename2, O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);

        
     printf("Size of receiver_packet is %d\n", sizeof(receiver_packet));
     printf("%s","String received from client:\n"); //get request from the client
     printf("1. chunk number is %d\n",receiver_packet.first_chunk_number);
     char * decrypted = encryptdecrypt(receiver_packet.first_data, receiver_packet.first_datasize);
     
     strcpy(receiver_packet.first_data, decrypted);
     puts(receiver_packet.first_data);
     write(fd_write1, receiver_packet.first_data , receiver_packet.first_datasize);

     printf("2. chunk number is %d\n",receiver_packet.second_chunk_number);
     decrypted = encryptdecrypt(receiver_packet.second_data, receiver_packet.second_datasize);
    
     strcpy(receiver_packet.second_data, decrypted);
     puts(receiver_packet.second_data);
     write(fd_write2, receiver_packet.second_data , receiver_packet.second_datasize);  
     close(fd_write1);
     close(fd_write2);  
}

int get_file(char * directory, int connfd, struct sockaddr_in cliaddr)
{
     char fullpath[100]; 
     int i;

     printf("filename is %s\n", receiver_packet.filename);
     bzero(fullpath, sizeof(fullpath));
     check_and_create_directory(directory, receiver_packet, fullpath);
     printf("full path is %s\n", fullpath);
     
     int fd_read1, fd_read2;
     char filename1[100];
     char filename2[100];
     int first_file = 0; int second_file = 0;
  
     
    

     for( i = 1; i <= 4; i++)
     {

        char chunk_string[10];
        bzero(chunk_string, sizeof(chunk_string));
        sprintf( chunk_string, "%d", i);
        strcpy(filename1, fullpath);
        strcat(filename1, ".");
        strcat(filename1, receiver_packet.filename);
        strcat(filename1, ".");
 
        strcat(filename1, chunk_string);               
        printf("filename is %s\n", filename1);
        
        fd_read1 = open(filename1 , O_RDONLY); //file open with read only option
        if(fd_read1 != -1)
        {
            printf("Found file %s\n",filename1);
            first_file = i;
            break;
        }
        //return -1;
     }

     for( i = first_file+1 ; i <= 4; i++)
     {

        char chunk_string[10];
        bzero(chunk_string, sizeof(chunk_string));
        sprintf( chunk_string, "%d", i);
        strcpy(filename2, fullpath);
        strcat(filename2, ".");
        strcat(filename2, receiver_packet.filename);
        strcat(filename2, ".");
        strcat(filename2, chunk_string);               
        printf("filename is %s\n", filename2);
        fd_read2 = open(filename2, O_RDONLY); //file open with read only option
        if(fd_read2 != -1)
        {
            printf("Found file %s\n",filename2);
            second_file = i;
            break;
        }
        //return -1;
     }

   
    i = 0;
    unsigned long size1, size2;
    size1 = calculate_filesize(filename1);
    size2 = calculate_filesize(filename2);
    //keep communicating with server
    printf("Sending files : \n");
    i = 0; 
    char buf[100];
    unsigned long nbytes;
    char *message[2];
    message[0] = calloc(size1 , sizeof(char));
    message[1] = calloc(size2 , sizeof(char));
    
    while(nbytes = read(fd_read1, message[0], size1)) //read MAXBUFSIZE from the file
    {
        printf("%s\n",message[0]);
    }
    while(nbytes = read(fd_read2, message[1], size2)) //read MAXBUFSIZE from the file
    {
        printf("%s\n",message[1]);
    }
    
    sender_packet = EmptyStruct;
       
    strcpy(sender_packet.filename , receiver_packet.filename);  
    sender_packet.first_chunk_number = first_file;
    sender_packet.first_datasize = size1;
   
    bzero(sender_packet.first_data, sizeof(sender_packet.first_data));
    char * encrypted = encryptdecrypt(message[0], size1);
    memcpy(sender_packet.first_data , encrypted, size1);

    sender_packet.second_chunk_number = second_file;
    sender_packet.second_datasize = size2;
       
    bzero(sender_packet.second_data, sizeof(sender_packet.second_data));
    encrypted = encryptdecrypt(message[1], size2);
    memcpy(sender_packet.second_data , encrypted, size2);
         
   // printf("\n sending first chunk::::: %s\t, %d\n",sender_packet.first_data, sender_packet.first_chunk_number);
    //printf("\n sending second chunk:::: %s\t, %d\n",sender_packet.second_data, sender_packet.second_chunk_number);
        
    printf("size of sender_packet is %d\n", sizeof(sender_packet));
    printf("sender filename is %s\n", sender_packet.filename);
    unsigned int remote_length = sizeof(cliaddr);
    if( sendto(connfd , &sender_packet, sizeof(sender_packet),0,(struct sockaddr *)&cliaddr , remote_length) < 0)
    {
         puts("Send failed");
         return 1;
    } 
    for( i = 0; i< 2; i++)
    {
        free(message[i]);
    } 
    close(fd_read1);
    close(fd_read2);
    return 0;
            
}

int main(int argc , char *argv[])
{
    int listenfd, connfd, n;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in remote;     //"Internet socket address structure"
    unsigned int remote_length;  
    char buf[MAXLINE];
    char directory[MAXLINE];
    struct sockaddr_in cliaddr, servaddr;
    struct config configstruct;

    //Check for arguments. Should provide port number          
    if (argc != 3)
    {
        printf ("USAGE:  <dfs> <port>\n");
	exit(1);
    }
    strcpy(directory, argv[1]);
    printf("Server is %s\n", directory);
    
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
    servaddr.sin_port = htons(atoi(argv[2]));
	
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
            receiver_packet = EmptyStruct;
            while (recvfrom(connfd, &receiver_packet, sizeof(receiver_packet),0,(struct sockaddr *)&cliaddr , &remote_length) > 0) 
            {
                printf("Received command: %s\n",receiver_packet.command);
                if(check_cred_match(configstruct, receiver_packet) == 0)
                {
                    printf("credentials do not match\n");
                    sender_packet.valid = -1;
                    strcpy(sender_packet.error_data, "Invalid Username or Password");
                    if( sendto(connfd , &sender_packet, sizeof(sender_packet),0,(struct sockaddr *)&cliaddr , remote_length) < 0)
                    {
                        puts("Send failed");
                        return 1;
                    } 
                    
                }
                else
                {
                    sender_packet.valid = 0;
                    strcpy(sender_packet.error_data, "Success");
                    if( sendto(connfd , &sender_packet, sizeof(sender_packet),0,(struct sockaddr *)&cliaddr , remote_length) < 0)
                    {
                        puts("Send failed");
                        return 1;
                    } 
                    
                    printf("Credentials match\n"); 
        
                    if(strcmp(receiver_packet.command, "put") == 0)
                    {
                        put_file(configstruct, directory); //If command is put, then client puts file into server
                    }
           
                    else if(strcmp(receiver_packet.command, "get") == 0)
                    {
                        get_file(directory, connfd, cliaddr); //If command is get, then client gets a file from server
                     }
               
                    else if(strcmp(receiver_packet.command, "LIST") == 0)
                    {
                        get_list_of_files(directory,connfd,cliaddr); //If command is ls then get list of all files in server directory
                    }
                    else if(strcmp(receiver_packet.command, "MKDIR") == 0)
                    {
                        char fullpath[100];
                        check_and_create_directory(directory, receiver_packet, fullpath);
                     }
                 }
                receiver_packet = EmptyStruct;
            }
	        //close(connfd);
                //printf("Closing socket.........\n");
                exit(0);
             
        }  
        //close socket of the server
        close(connfd);
        printf("socket closed\n");
    }
    free(configstruct.username);
    free(configstruct.password);
    
}




