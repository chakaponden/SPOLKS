#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <signal.h> 

#define	MAX_PENDING	1								// max pending client connections
#define	BUFFER_SIZE  	1024							// for incomming

int workSock, listenSock;									// socket descriptors
int ind = 0;											// indicator that any socket in open


void hdl(int sig, siginfo_t *siginfo, void *context)				// handler for SIGINT (Ctrl+C) signal
{
    if (sig==SIGINT)
    {	 
	if(close(listenSock) < 0)			
		perror("sgn close listenSock");
	else
		ind-=2;
	if(ind)   
	{
		if(close(workSock) < 0)		
			perror("sgn close workSock");	
		else
			ind--;
	}
    }
}

int clientProcessing(char *hostName, char *port)
{

	struct sockaddr_in hostAddr;    						// this machine 
	char buf[BUFFER_SIZE];								// buffer for incomming
	int readBytes;										// count of	
	int so_reuseaddr = 1;								// for setsockop SO_REUSEADDR set enable
	if(((listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
   	{
        	perror("func socket");
        	return -1;
    	}    
	ind+=2;
    	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order		
	if(!htons(inet_aton(hostName, hostAddr.sin_addr.s_addr)))	// new func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	{
		perror("func inet_aton");
        	return -1;		
	}
    	//hostAddr.sin_addr.s_addr = inet_addr(hostName);			// old func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	setsockopt(listenSock, SOCK_STREAM, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
													// reuse ADDR when socket in TIME_WAIT condition
    	if(bind((listenSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
													// socket is associated with IPv4 address
    	{
        	perror("func bind");
        	return -1;
    	}
    	if(listen((listenSock), MAX_PENDING) < 0)				// set socket to listen status
	{
		perror("func listen");
        	return -1;		
	} 
	while(1)											// if any key is pressed -> exit from while loop
    	{
		if(((workSock) = accept((listenSock), 0, 0)) < 0)		// wait for client
		{
			perror("func accept");
            		return -2;    
        	}
		ind++;
       		while(1)										// if any key is pressed -> exit from while loop
        	{			
			printf("+\n");
           		if((readBytes = recv((workSock), buf, BUFFER_SIZE, 0)) < 0)
													// receive from client
			{
				perror("func recv");
				return -2;
			}	
			if(!readBytes)
				break;
            		if(send((workSock), buf, readBytes, 0) < 0)		// send to client
			{
				perror("func send");
				return -2;
			}				
       		}   			
        	if(close((workSock)) < 0)							// close connection
		{
			perror("func close workSock");
			return -2;
		}	
		ind--;
		printf("-\n");
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
	struct sigaction act;
	act.sa_sigaction =&hdl;
	act.sa_flags = SA_SIGINFO;
	if(sigaction(SIGINT, &act, NULL) < 0)					// set handler for SIGINT signal
	{
		perror("main sigaction");
		return -1;
	}	
	switch (clientProcessing(argv[1], argv[2]))
	{
		case -1:
		{			 
			if(ind > 1)
			{
				if(close(listenSock) < 0)		
				{
					perror("main close listenSock");	
					return -1;
				}
			}
			break;
		}
		case -2:
		{	
			if(ind > 1)
			{
				if(close(listenSock) < 0)		
				{
					perror("main close listenSock");	
					return -1;
				}
				ind-=2;
			}
			if(ind)
			{
				if(close(workSock) < 0)		
				{
					perror("main close workSock");	
					return -1;					
				}
			}
			break;
		}
		default:
			break;
	}			
    	return 0;
}