#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include "kbhit.c"

#define	MAX_PENDING	1						// max pending connections
#define	BUFFER_SIZE  	1024						// for incomming

int clientProcessing(int *workSock, int *listenSock, char *hostName, char *port)
{
	struct sockaddr_in hostAddr;    				// this machine 
	char buf[BUFFER_SIZE];						// buffer for incomming
	int readBytes;							// count of	
	if(((*listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
   	{
        	perror("socket");
        	return -1;
    	}    
    	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));				// convert host byte order -> network byte order
/*		
	if(!htons(inet_aton(hostName, hostAddr.sin_addr.s_addr)))	// get error: "Address already in use"
	{
		perror("inet_aton");
        	return -1;		
	}
*/
    	hostAddr.sin_addr.s_addr = inet_addr(hostName);			// convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too)
    	if(bind((*listenSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
									// socket is associated with IPv4 address
    	{
        	perror("bind");
        	return -1;
    	}
    	if(listen((*listenSock), MAX_PENDING) < 0)				// set socket to listen status
	{
		perror("listen");
        	return -1;		
	} 
	while(!kbhit())								// if any key is pressed -> exit from while loop
    	{
		printf("waiting for connection\n");
		if(((*workSock) = accept((*listenSock), 0, 0)) < 0)		// processing client
		{
			perror("accept");
            		return -2;    
        	}
		printf("new client connected\n");
       		while(!kbhit())							// if any key is pressed -> exit from while loop
        	{			
           		if((readBytes = recv((*workSock), buf, BUFFER_SIZE, 0)) < 0)
									// receive from client
			{
				perror("recv");
				return -2;
			}	
			if(!readBytes)
			{
				perror("recv connection lost");
				break;
			}					
            		if(send((*workSock), buf, readBytes, 0) < 0)	// send to client
			{
				perror("send");
				return -2;
			}			
       		}   	
        	if(close((*workSock)) < 0)						
		{
			perror("close workSock child");
			return -2;
		}	
		printf("connection closed\n");
    	}	
	if(close(*listenSock) < 0)		
	{				
		perror("close listenSock");
		return -1;
	}
	return 0;	
}


int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		perror("invalid command-line arguments");
		return -1;
	}
    	int workSock, listenSock;					// descriptors
	int childPid, status, retChild;   				// parameters for child process
	switch((childPid = fork()))
	{

		case -1: 
			{
				perror("fork");
        			return -1;				
			}	
		case 0 : 						// child process
			{			
				status = clientProcessing(&workSock, &listenSock, argv[1], argv[2]);
				return status;	
			}			
		default :						// parent process
			{
				if(wait(&status) < 0)
					perror("wait");
				if (WIFEXITED (status))
  					retChild = WEXITSTATUS (status); 
									// child return value 
				break;				
			}
	}       
	if(retChild == -2) 						// close workSock && listenSock if it did not
	{ 	  
		if(close(workSock) < 0)						
			perror("close workSock parent");
		if(close(listenSock) < 0)						
			perror("close listenSock");
	}
	if(retChild == -1)						// close only listenSock if it did not
	{
		if(close(listenSock) < 0)						
			perror("close listenSock");
	}	  
    	return 0;
}