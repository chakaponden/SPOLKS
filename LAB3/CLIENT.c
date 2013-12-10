/* many clients (limit MAX_PENDING) send files
 * server receive and save local
 * if some clients send file with the same fileName
 * then server drop last client connection with same fileName
 * and continue to download from first client with same fileName
 * CTRL+Z on client side == send OOB data '5' to server
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


#define	MAX_PENDING		1						// max pending client connections
#define	BUFFER_SIZE  		1024						// for incomming
#define ARG_ERROR_MESS		"\n[protocol] [ip] [port] [filename or full filepath]\n\n[protocol] need to be 'tcp'/'udp'\n"
#define	MAX_FILEPATH_LENGHT	64						// in bytes

int 		listenSock;							// socket descriptors
int 		ind = 0;							// indicator that any socket in open
long long	fileSize = 2;
long long 	filePointer = 0;
FILE*		file;
int 		OOB = 0;							// indicator for OOB (see signal handles)


void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)			// handler for SIGINT (Ctrl+C) signal
{
    if (sig==SIGINT)
    {	 
	if(ind)
	{
	  if(shutdown(listenSock, SHUT_RDWR) < 0)				// deny connection
	    perror("func shutdown listenSock signal");
	  if(close(listenSock) < 0)			
		  perror("sgn close listenSock signal");
	  ind--;
	}	
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);
    }
}

void hdl_SIGTSTP(int sig, siginfo_t *siginfo, void *context)			// handler for SIGTSTP (Ctrl+Z) signal (client)
{
  if(sig==SIGTSTP)
    OOB = 1;
}


int startClientTcp(char *hostName, char *port, char *argFilePath)
{
  
	struct 		sockaddr_in hostAddr;    				// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for outcomming
	int 		readBytes;						// count of	
	int 		sendBytes;
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	uint8_t 	bufOOBin;	
    	char		filePath[MAX_FILEPATH_LENGHT];
	strcpy(filePath, argFilePath);
	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order		
	hostAddr.sin_addr.s_addr = inet_addr(hostName);				// old func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	filePath[strlen(filePath)] = '\0';
	file = fopen(filePath, "rb");						// open file for read
	fseek(file, 0L, SEEK_END);						
	fileSize = ftell(file);							// get file size
	fseek(file, 0L, SEEK_SET);						
	//fd_set temp;		
	struct timeval time_out; time_out.tv_sec = 0; time_out.tv_usec = 0;
	struct timespec tim, tim2;
	tim.tv_sec = 0;
	tim.tv_nsec = 900000L;							// sleep time in nanosec
	uint8_t bufOOB = 5;
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		if(((listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
		{
			perror("func socket");
			return -1;
		}  
		ind++;
		//hostAddr.sin_addr.s_addr = inet_addr(hostName);		// old func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
		setsockopt(listenSock, SOCK_STREAM, 
							SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
										// reuse ADDR when socket in TIME_WAIT condition
		
		
						  
		//puts("client_connect");					
		if(connect(listenSock, (struct sockaddr*) &hostAddr, sizeof(hostAddr)) < 0)
		{
		  perror("func connect");
		  return -1;		
		}
		
		//printf("client_send_filePath %s\n", filePath);
		while(send(listenSock, (char*)&filePath, MAX_FILEPATH_LENGHT*sizeof(char), 0) < MAX_FILEPATH_LENGHT*sizeof(char))
		{								// send filePath
		  perror("func send filePath");
		  printf("errno: %d\n", errno);					// errno == 4 means EINTR == Interrupted system call 
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		}   								
				
		//FD_ZERO (&temp);
		//FD_SET (listenSock,&temp);
		
		
		//printf("client_filePointer: %lld\n", filePointer);						
		
		//printf("client_send_fileSize %lld\n", fileSize);
		while(send(listenSock, (char*)&fileSize, sizeof(long long), 0) < sizeof(long long))
		{								// send fileSize
		  perror("func send fileSize");
		  printf("errno: %d\n", errno);
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		} 								
		
		//puts("client_recv_filePointer");
										// recv filePointer from server
		while(recv(listenSock,(char*)&filePointer, sizeof(long long), MSG_WAITALL) < sizeof(long long))
		{									      			  
		  perror("func recv filePointer");
		  printf("errno: %d\n", errno);
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		}		
		
		//printf("client_recvFilePointer_fromServer %lld\n", filePointer);
		fseek(file, filePointer, SEEK_SET);				// go to addr in file, according filePointer
		
		do
		{
		  readBytes = fread(&buf, sizeof(char), BUFFER_SIZE, file);
		  if(OOB)							// if signal SIGTSTP set OOB to 1
		  {
		    OOB = 0;							// send OOB data
		    while(send(listenSock, &bufOOB, sizeof(bufOOB), MSG_OOB) < 0)
		    {		      
		      perror("func send SIGTSTP");
		      printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		      
		    }
		    printf("signal SIGTSTP. OOB data sended: %d\n", bufOOB);
		    if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		    {
			perror("nano sleep system call failed");
			printf("errno: %d\n", errno);
										// errno == 4 means EINTR == Interrupted system call 
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
			  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		    }
		  }		  
		  //printf("client_send_fread %d\n", readBytes);
		  /*if(select(0,NULL,&temp,NULL,&time_out))
		  {
		    	  bufOOBin = 0;
			  if(recv(workSock, &bufOOBin, sizeof(bufOOBin), MSG_OOB | MSG_WAITALL) < 0)			
			  {							// recv sometimes return to perror "Resource temporarily unavailable"					
			    printf("errno %d\n", errno);
			    perror("func recv select");			    
			  }
			  printf("OOB data received: %d\n", bufOOBin);
		  }*/ 		
		  //printf("client_select filePointer %lld\n", filePointer);	// send data to server
		  while((sendBytes = send(listenSock, (char*)&buf, readBytes, 0)) < 0)
		  {
		    perror("func send data");
		    printf("errno: %d\n", errno);
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		      return -1;		    
		  } 
		  filePointer += sendBytes;
		  if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		  {
		      perror("nano sleep system call failed");
		      printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		  }
		  //puts("client_send_fieFragment");
		}while(readBytes > 0);
		//puts("client_close_socket");
		if(shutdown(listenSock, SHUT_RDWR) < 0)				// deny connection
		{
			perror("func shutdown listenSock");
			return -1;
		}
		if(close(listenSock) < 0)
		{
		  perror("close listenSock");
		  return -1;
		}
		ind--;
		//puts("client_file_close");
		if(ftell(file) >= 0)						// check is file open
		  fclose(file);		
		return 0;		
	}
}			

int startClientUdp(char *hostName, char *port, char *filePath)
{
  return 0;
}

int main(int argc, char *argv[])
{
	if(argc < 5 || 
	  ((strcmp(argv[1], "tcp")) && strcmp(argv[1], "udp")))			// check command-line arguments
	{
		puts(ARG_ERROR_MESS);
		perror("invalid command-line arguments");
		return -1;
	}
	if(access(argv[4], F_OK ) < 0)						// check file exist
	{
	  printf("file %s does not exist\n", argv[4]);
	  perror("invalid fileName");
	  return -1;
	}
	struct sigaction closeTerm, sendOOB;
	closeTerm.sa_sigaction =&hdl_SIGINT;
	closeTerm.sa_flags = SA_SIGINFO;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)				// set handler for SIGINT signal (CTRL+C)
	{
		perror("main sigaction closeTerm");
		return -1;
	}	
	sendOOB.sa_sigaction =&hdl_SIGTSTP;
	sendOOB.sa_flags = SA_SIGINFO;
	if(sigaction(SIGTSTP, &sendOOB, NULL) < 0)				// set handler for SIGTSTP signal (CTRL+Z) (client)
	{
	  perror("main sigaction sendOOB");
	  return -1;
	}
	if(!strcmp(argv[1], "tcp"))
	{	 
	  startClientTcp(argv[2], argv[3], argv[4]);				// tcp
	}
	else
	{
	  startClientUdp(argv[2], argv[3], argv[4]);				// udp
	}
	if(ind)
	{
		if(shutdown(listenSock, SHUT_RDWR) < 0)				// deny connection
		{
			perror("func shutdown listenSock main");
			return -1;
		}
		if(close(listenSock) < 0)		
		{
			perror("main close listenSock main ");	
			return -1;
		}
		ind--;
	}
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);
    	return 0;
}
