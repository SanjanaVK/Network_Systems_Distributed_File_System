
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

#define MAXBUF 10 * 4096
#define CONF_FILENAME "dfc.conf"
#define FILENAME "foo1.txt"

int sock[4];
static const struct packet_t EmptyStruct; 

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
    char error_data[100];
    char username[100];
    char password[100];
};
struct packet_t sender_packet;
struct packet_t receiver_packet[4];
struct servers
{
    int first_file;
    int second_file;
};
struct servers serverstruct[4];

/*Display menu function*/
void display_menu()
{
   printf("\n================================================Command Menu=====================================================================================\n");
   printf("\"get [file_name]\"       : The server transmits the requested file to the client \n");
   printf("\"put [file_name]\"       : The server receives transmitted file from the server and stores it locally \n");
   printf("\"delete [file_name]\"    : The server deletes the file if exists otherwise does nothing \n");
   printf("\"ls\"                    : The server searches for all files in its directory and sends the list of all the files to the client \n");
   printf("\"exit\"                  : The server exits gracefully \n");
}

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
                            char * tok = strtok(token, "\n ");
                            strcpy(configstruct.username[user_index] , tok);
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
                                   char * tok = strtok(token, "\n ");
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

int check_md5sum(char * filename)
{
    int fd, nbytes;
    int result = 0;
    char buffer[MAXBUF];
    
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
    return result;
}

void get_file_server_map(int md5_mod)
{
    //serverstruct[4] = malloc(sizeof((struct servers) * 4));
    if(md5_mod == 0)
    {
        serverstruct[0].first_file = 1;
        serverstruct[0].second_file = 2;
        serverstruct[1].first_file = 2;
        serverstruct[1].second_file = 3;
        serverstruct[2].first_file = 3;
        serverstruct[2].second_file = 4;
        serverstruct[3].first_file = 4;
        serverstruct[3].second_file = 1;
    } 
    if(md5_mod == 1)
    {
        serverstruct[0].first_file = 4;
        serverstruct[0].second_file = 1;
        serverstruct[1].first_file = 1;
        serverstruct[1].second_file = 2;
        serverstruct[2].first_file = 2;
        serverstruct[2].second_file = 3;
        serverstruct[3].first_file = 3;
        serverstruct[3].second_file = 4;
    }
    if(md5_mod == 2)
    {
        serverstruct[0].first_file = 3;
        serverstruct[0].second_file = 4;
        serverstruct[1].first_file = 4;
        serverstruct[1].second_file = 1;
        serverstruct[2].first_file = 1;
        serverstruct[2].second_file = 2;
        serverstruct[3].first_file = 2;
        serverstruct[3].second_file = 3;
    } 
    if(md5_mod == 3)
    {
        serverstruct[0].first_file = 2;
        serverstruct[0].second_file = 3;
        serverstruct[1].first_file = 3;
        serverstruct[1].second_file = 4;
        serverstruct[2].first_file = 4;
        serverstruct[2].second_file = 1;
        serverstruct[3].first_file = 1;
        serverstruct[3].second_file = 2;
    }                                  
}

int put_file(char * filename, struct sockaddr_in server, struct config *configstruct)
{
    char *message[4];
    unsigned long filesize = calculate_filesize(filename);
    printf("File size %ld\n", filesize);
    int rem_size = filesize % 4;
    int size_array[4];
    int fd;
    int i;
    
    printf("rem size is %d\n", rem_size);
    unsigned long temp_size= filesize/4;
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
   // int fd_write;
    long nbytes;
    //fd_write = open( "result.png", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    i = 0;
    //keep communicating with server
    printf("Sending files : \n");
    i = 0; 
    char buf[100];
    unsigned int remote_length = sizeof(server);
   
    message[0] = calloc(size_array[3] , sizeof(char));
    message[1] = calloc(size_array[3] , sizeof(char));
    message[2] = calloc(size_array[3] , sizeof(char));
    message[3] = calloc(size_array[3] , sizeof(char));
    while(nbytes = read(fd, message[i], size_array[i])) //read MAXBUFSIZE from the file
    {
        printf("%s\n",message[i]);
        i++;
    }
    
    int fd_write1 = open("chunk1.1", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[0] , size_array[0]);
    close(fd_write1);
    fd_write1 = open("chunk2.2", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[1] , size_array[1]);
     close(fd_write1);
     fd_write1 = open("chunk3.3", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[2] , size_array[2]);
    close(fd_write1);
    fd_write1 = open("chunk4.4", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[3] , size_array[3]);
    close(fd_write1);
   int fd_write4 = open("total.png", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
   write(fd_write4, message[0] , size_array[0]);
   write(fd_write4, message[1] , size_array[1]);
   write(fd_write4, message[2] , size_array[2]);
   write(fd_write4, message[3] , size_array[3]);
   close(fd_write4);
    
    for(i = 0; i< 4; i++)
    {
    	//char * encrypted = encryptdecrypt(message[i], nbytes); //encrypt file data packet
                 
       // char * decrypted = encryptdecrypt(encrypted, nbytes); //decrypt file packet
       
       // write(fd_write, decrypted, nbytes);
         
        //Send some data
       
       	 printf("\nsending first chunk\n %s\n", message[serverstruct[i].first_file - 1]);
         
         //sender_packet = EmptyStruct;
         strcpy(sender_packet.username, configstruct->username[0]);
         strcpy(sender_packet.password, configstruct->password[0]);
   
	 sender_packet.first_chunk_number = serverstruct[i].first_file;
         sender_packet.first_datasize = size_array[serverstruct[i].first_file - 1];
         
	 bzero(sender_packet.first_data, sizeof(sender_packet.first_data));
         memcpy(sender_packet.first_data , message[serverstruct[i].first_file - 1],sender_packet.first_datasize);

         sender_packet.second_chunk_number = serverstruct[i].second_file;
         sender_packet.second_datasize = size_array[serverstruct[i].second_file - 1];
         
	 bzero(sender_packet.second_data, sizeof(sender_packet.second_data));
         memcpy(sender_packet.second_data , message[serverstruct[i].second_file - 1], sender_packet.second_datasize);
         
	 printf("\n%d sending first chunk::::: %s\t, %d::%d\n",i, sender_packet.first_data, sender_packet.first_chunk_number, serverstruct[i].first_file);
         printf("\n%d sending second chunk:::: %s\t, %d::%d\n", i,sender_packet.second_data, sender_packet.second_chunk_number, serverstruct[i].second_file);
        
         printf("size of sender_packet is %d\n", sizeof(sender_packet));
         printf("sender filename is %s\n", sender_packet.filename);
	 if( sendto(sock[i] , &sender_packet, sizeof(sender_packet),0,(struct sockaddr *)&server , remote_length) < 0)
         {
            puts("Send failed");
            return 1;
         } 
        

        //printf("\n sending second chunk\n %s\n", message[serverstruct[i].second_file - 1]);
         
         //sender_packet.chunk_number = '\0';
	 
         
       //Send some data
        /*if( send(sock[i] , &sender_packet, sizeof(sender_packet) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }*/

         
        //Receive a reply from the server
        /*if( recv(sock , server_reply , 2000 , 0) < 0)
        {
            puts("recv failed");
            break;
        }
         
        puts("Server reply :");
        puts(server_reply);*/
    }
    close(fd);

}

int get_file(char * filename, struct sockaddr_in server, struct config configstruct)
{
    unsigned long filesize = calculate_filesize(filename);
    printf("File size %ld\n", filesize);
    int rem_size = filesize % 4;
    int size_array[4];
    int fd;
    int i;
    char *message[4];
    
    printf("rem size is %d\n", rem_size);
    unsigned long temp_size= filesize/4;
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
    unsigned int remote_length = sizeof(server);
    message[0] = calloc(size_array[3] , sizeof(char));
    message[1] = calloc(size_array[3] , sizeof(char));
    message[2] = calloc(size_array[3] , sizeof(char));
    message[3] = calloc(size_array[3] , sizeof(char));

    strcpy(sender_packet.username, configstruct.username[0]);
    strcpy(sender_packet.password, configstruct.password[0]);
    
    for(i = 0; i< 4; i++)
    {
        
         printf("size of sender_packet is %d\n", sizeof(sender_packet));
         printf("sender filename is %s\n", sender_packet.filename);
	 if( sendto(sock[i] , &sender_packet, sizeof(sender_packet),0,(struct sockaddr *)&server , remote_length) < 0)
         {
            printf("Send failed to server %d", i);
            //return 1;
         } 
    }
    
    for(i = 0; i < 4; i++)
    {
        if(recvfrom(sock[i], &receiver_packet[i], sizeof(receiver_packet[i]),0,(struct sockaddr *)&server , &remote_length) <= 0) 
        {
            printf("Receive failed from server %d", i);
            //return 1;
        }
    }
    printf("file received\n");
    int check_received[4];
    int fd_write;
    bzero(check_received , sizeof(check_received));
    //get_chunks_from_packets(&message);
    for( i = 0; i<4; i++)
    {
        printf(":::%s, %d:::\n", receiver_packet[i].first_data, receiver_packet[i].first_chunk_number);
        printf(":::%s, %d:::\n", receiver_packet[i].second_data, receiver_packet[i].second_chunk_number);
        if(receiver_packet[i].first_chunk_number == 1 || receiver_packet[i].second_chunk_number == 1)
        {
            printf("chunk  number 1 found \n");
            //printf("size is %d::%d\n", receiver_packet[i].first_datasize, size_array[0]);
                if((receiver_packet[i].first_chunk_number == 1 )&& (receiver_packet[i].first_datasize == size_array[0]))
                {
                    printf("chunk  number 1 confirmed \n");
                    if(check_received[0] != 1)
                    {   
                        memcpy(message[0] , receiver_packet[i].first_data, size_array[0]);
                        //printf("data is %s\n", message[0]);
                        check_received[0] = 1;
                        //continue;
                    }
                }
                else if((receiver_packet[i].second_chunk_number == 1) && (receiver_packet[i].second_datasize== size_array[0]))
                {
                    if(check_received[0] != 1)
                    {   
                        memcpy(message[0] , receiver_packet[i].second_data, size_array[0]);
                        check_received[0] = 1;
                        //continue;
                    }
                }          
        }   
        if(receiver_packet[i].first_chunk_number == 2 || receiver_packet[i].second_chunk_number == 2)
        {
                printf("chunk  number 2 found \n");
            //printf("size is %d::%d\n", receiver_packet[i].first_datasize, size_array[0]);
                if((receiver_packet[i].first_chunk_number == 2) && (receiver_packet[i].first_datasize == size_array[1]))
                {
                    if(check_received[1] != 1)
                    {   
                        memcpy(message[1] , receiver_packet[i].first_data, size_array[1]);
                        check_received[1] = 1;
                        //continue;
                    }
                }
                if((receiver_packet[i].second_chunk_number == 2) && (receiver_packet[i].second_datasize == size_array[1]))
                {
                    if(check_received[1] != 1)
                    {   
                        memcpy(message[1] , receiver_packet[i].second_data, size_array[1]);
                        check_received[1] = 1;
                        //continue;
                    }
                }          
        }   
        if(receiver_packet[i].first_chunk_number == 3 || receiver_packet[i].second_chunk_number == 3)
        {
            printf("chunk  number 3 found \n");
                if((receiver_packet[i].first_chunk_number == 3) && (receiver_packet[i].first_datasize == size_array[2]))
                {
                    if(check_received[2] != 1)
                    {   
                        memcpy(message[2] , receiver_packet[i].first_data, size_array[2]);
                        check_received[2] = 1;
                        //continue;
                    }
                }
                if((receiver_packet[i].second_chunk_number == 3) && (receiver_packet[i].second_datasize == size_array[2]))
                {
                    if(check_received[2] != 1)
                    {   
                        memcpy(message[2] , receiver_packet[i].second_data, size_array[2]);
                        check_received[2] = 1;
                        //continue;
                    }
                }          
        }   
        else if(receiver_packet[i].first_chunk_number == 4 || receiver_packet[i].second_chunk_number == 4)
        {
                printf("chunk  number 4 found \n");
                //printf("size is %d::%d\n", receiver_packet[i].first_datasize, size_array[3]);
                if((receiver_packet[i].first_chunk_number == 4) && (receiver_packet[i].first_datasize == size_array[3]))
                {
                   // printf("size is %d::%d:%d\n", receiver_packet[i].first_datasize, size_array[3], check_received[3]);
                    if(check_received[3] != 1)
                    {   
                        memcpy(message[3] , receiver_packet[i].first_data, size_array[3]);
                        //printf("data is %s\n", message[3]);
                        check_received[3] = 1;
                        //continue;
                    }
                }
               // printf("size is %d::%d\n", receiver_packet[i].second_datasize, size_array[3]);
                if((receiver_packet[i].second_chunk_number == 4) && (receiver_packet[i].second_datasize == size_array[3]))
                {
                    if(check_received[3] != 1)
                    {   
                        memcpy(message[3] , receiver_packet[i].second_data, size_array[3]);
                        //("data is %s\n", message[3]);
                        check_received[3] = 1;
                       // continue;
                    }
                }          
        }   
                
    }
    char filename_result[100];
    strcpy(filename_result, receiver_packet[0].filename);
    strcat(filename_result,".result");
    fd_write = open( filename_result, O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    for(i = 0; i < 4; i++)
    {
        printf(":::%s:::\n", message[i]);
        printf("size of message is %ld\n", sizeof(message[i]));
        write(fd_write, message[i] , size_array[i]);
    }  
    int fd_write1 = open("chunk1", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[0] , size_array[0]);
    close(fd_write1);
    fd_write1 = open("chunk2", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[1] , size_array[1]);
     close(fd_write1);
     fd_write1 = open("chunk3", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[2] , size_array[2]);
    close(fd_write1);
    fd_write1 = open("chunk4", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0666);
    write(fd_write1, message[3] , size_array[3]);
    close(fd_write1);
    close(fd_write);    
}

int main(int argc , char *argv[])
{
   
    struct config configstruct;
    struct sockaddr_in server;
    char *message[4] , server_reply[2000];
    int md5_mod;
    int i = 0;
    int port_int[4];
    char user_input[100];
    char *token;
    
    configstruct = get_conf_parameters(CONF_FILENAME);

    

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
            //return 1;
        }
        puts("Connected\n");
    }

    while(1)
    {
        display_menu();
        sender_packet = EmptyStruct;
        printf("\n Please enter a valid command from the command menu\n");
        
        gets(user_input);// Get user input
        printf("Command entered: %s\n", user_input);
        strcpy(sender_packet.command , "\0");
        strcpy(sender_packet.filename, "\0");
        //Tokenize user input to get command and filename(if required)
        token = NULL;
        token = strtok(user_input, " [ ]");
        if (token == NULL)
        {
            sender_packet.valid =0;
        }
        else
            strcpy(sender_packet.command, token);
        while (token != NULL)
        {
            token = strtok(NULL, " [ ]");
            if(token != NULL)
            {
                strcpy(sender_packet.filename, token);
            }
        }
        
        /* Execute appropriate functions according to commands*/
        if (strcmp(sender_packet.command , "get") == 0)
        {
            if(strcmp(sender_packet.filename, "\0") == 0) //Check if filename is present, else ask user to retry
            {
                printf("Error: Please Enter a Filename: get [<filename>]");
                continue;
            }
 
            printf("Obtaining file %s from the server\n", sender_packet.filename);
            sender_packet.valid = 1;
            md5_mod = check_md5sum(sender_packet.filename);
            printf("md5 mod result is %d\n", md5_mod);
            get_file_server_map(md5_mod);
            get_file(sender_packet.filename, server, configstruct); //Get file from the server
        }
        
        else if (strcmp(sender_packet.command , "put") == 0)
        {
            if(strcmp(sender_packet.filename, "\0") == 0) //Check if filename is present, else ask user to retry
            {
                printf("Error: Please Enter a Filename: put [<filename>]");
                continue;
            }
 
            printf("Sending file %s to the server.....\n", sender_packet.filename);
            sender_packet.valid = 1;
            md5_mod = check_md5sum(sender_packet.filename);
            printf("md5 mod result is %d\n", md5_mod);
            get_file_server_map(md5_mod);
            put_file(sender_packet.filename, server, &configstruct);// Put file into server only if file exists in client side
              //  continue;
        }
       
        else if (strcmp(sender_packet.command , "ls") == 0)
        {
            if(strcmp(sender_packet.filename, "\0") != 0)
            {
                printf("Did you mean <ls>?, try again\n");
                continue;
            }
            sender_packet.valid = 1;
          //  get_list_of_files(remote, remote_length, sockfd); //List the files in the server directory
        }
       
    
     }
     
    for(i = 0 ; i< 4; i++)
        close(sock[i]);
    return 0;
}
