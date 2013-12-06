// client send file
// server receive and save local
// CTRL+Z on client side == send OOB data '5' to server

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
#define ARG_ERROR_MESS		"\nCLIENT_MODE:\nclient <ip> <port> <filename or full filepath>\n\nSERVER_MODE:\nserver <ip> <port>"
#define	MAX_FILEPATH_LENGHT	64						// in bytes

int 		workSock, listenSock;						// socket descriptors
int 		ind = 0;							// indicator that any socket in open
long long	fileSize = 2;
long long 	filePointer = 0;
FILE*		file;
int 		OOB = 0;							// indicator for OOB (see signal handles)


void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)			// handler for SIGINT (Ctrl+C) signal
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
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);
    }
}

void hdl_SIGTSTP(int sig, siginfo_t *siginfo, void *context)			// handler for SIGTSTP (Ctrl+Z) signal (client)
{
  if(sig==SIGTSTP)
    OOB = 1;
}

void hdl_SIGURG(int sig, siginfo_t *siginfo, void *context)			// handler for OOB data received signal (server)
{
  if(sig==SIGURG)
    OOB = 1;
}



int startServer(char *hostName, char *port)
{

	struct 		sockaddr_in hostAddr;    				// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for incomming
	int 		readBytes;						// count of	
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	int 		clientFirstPacket;					// client first packet indicator					
	char 		c[64] = {0};						// for terminal input
	char*		filePath = (char*) malloc (MAX_FILEPATH_LENGHT*sizeof(char));
	long long 	localFileSize = 0;
	uint8_t 	bufOOBin;
    	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order		
	if(!htons(inet_aton(hostName, hostAddr.sin_addr.s_addr)))		// new func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	{
		perror("func inet_aton");
        	return -1;		
	}
    	//hostAddr.sin_addr.s_addr = inet_addr(hostName);			// old func convert IPv4 char* -> IPv4 bin (+ host byte order -> network byte order too) 
	
	if(((listenSock) = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)		
   	{
        	perror("func socket");
        	return -1;
    	}    
	ind+=2;
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
	//fd_set temp;		
	//struct timeval time_out; time_out.tv_sec = 0; time_out.tv_usec = 0;
	
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		//printf("server_accept_wait_for_client\n");
		clientFirstPacket = 1;
		//FD_ZERO (&temp);
		//FD_SET (workSock,&temp);
		filePointer = 0;
		fileSize = 2;
		readBytes = 1;
		if(((workSock) = accept((listenSock), 0, 0)) < 0)		// wait for client
		{
			perror("func accept");
            		return -1;    
        	}
        	fcntl(workSock, F_SETOWN, getpid()); 				// set pid which will be recieve SIGIO and SIGURG on descriptor' events
		ind++;		
       		while(filePointer < fileSize && readBytes)							
        	{		
			if(OOB)							// if OOB data is received
			{
			  //puts("recv OOB");
			  OOB = bufOOBin = 0;					// recv sometimes return to perror "Resource temporarily unavailable"
			  while(recv(workSock, &bufOOBin, sizeof(bufOOBin), MSG_OOB) < 0)		
			  {			    
			    perror("func recv SIGURG");
			    printf("errno %d\n", errno);			// errno == 4 means EINTR == Interrupted system call 
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)		
			      return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again			
			  }
			  printf("signal SIGURG. OOB data received: %d\n", bufOOBin);
			  printf("%lld bytes for download left\n", (fileSize - filePointer));
			}
			//printf("server_select %lld %lld\n", filePointer, fileSize);
			/*if(select(0,NULL,NULL,&temp,&time_out) < 0)
			{
				puts("server timeout 10s reached");
				break;
			}     */     
			//printf("server_recv data\n");			
			if(clientFirstPacket)					// if first packet from client
										// check if file exist (filename gets from client message) 
			{		
			  clientFirstPacket = 0;
			  while((readBytes = recv((workSock), (char*)filePath, MAX_FILEPATH_LENGHT*sizeof(char), 0)) < 0)
										// receive filePath from client
			  {
				  perror("func recv");				// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
				    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
			  //printf("server_readbytes num %d, message: %s\n", readBytes, filePath);
			  while((readBytes = recv((workSock), (char*)&fileSize, sizeof(long long), 0)) < 0)
										// receive fileSize from client
			  {
			    perror("func recv");
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			// errno == 4 means EINTR == Interrupted system call 
				    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
			  //printf("server_recv fileSize %lld\n",  fileSize);
			  //if(!readBytes)
			  //	  break;
			  //printf("server_first_realloc\n"); 
			 
			  filePath[readBytes] = '\0';
			  //printf("server_accept_filePath %s\n",  filePath);
			  if(access(filePath, F_OK ) < 0)
			  {							// file not exist			    
			    file = fopen(filePath, "wb");			// create file
			    filePointer = 0;					
			  }
			  else			  			  				
			  {							// file exist
			    file = fopen(filePath, "rb");			// open file for read
			    fseek(file, 0L, SEEK_END);						
			    localFileSize = ftell(file);			// get local file size
			    fclose(file);
			    if(localFileSize == fileSize)
			    {
			      printf("Do you want to redownload %s file? Y/N\n", filePath);
			      fgets(c, sizeof(c)*sizeof(char), stdin);		// input
			      if(c[0] == 'Y')					// press 'Y'
			      {
				file = fopen(filePath, "wb");			// create file
				filePointer = 0;					
			      }
			      else						// press 'N'
			      {	
				filePointer = fileSize;				// send filePointer that point to EOF (filePointer==fileSize)
				while(send(workSock, (char*)&filePointer, sizeof(long long), 0) < 0)										
				{
				  perror("func send");				// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
				    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
				} 						
			      }
			    }
			    else
			    {
			      printf("Resume downloading %lld bytes %s file? Y/N\nY = resume, N = redownload\n",
										(fileSize-localFileSize), filePath);
			      fgets(c, sizeof(c)*sizeof(char), stdin);		// input
			      if(c[0] == 'Y')					// press 'Y'
			      {
				file = fopen(filePath, "ab");			// open file at end
				filePointer = ftell(file);;					
			      }
			      else						// press 'N'
			      {
				file = fopen(filePath, "wb");			// create file
				filePointer = 0;
			      }
			    }	
			  }			 
			  //printf("server_recv_fileSize send filePointer %lld\n",  filePointer);
			  //if(!readBytes)
			  //	break;						// send num bytes already received (filePointer)
			  while(send(workSock, (char*)&filePointer, sizeof(long long), 0) < 0)										
			  {
			    perror("func send");				// errno == 4 means EINTR == Interrupted system call 
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  } 
			   //printf("server_send num bytes: %d, mess: %lld\n",  readBytes, filePointer);
			}			
			else
			{
			  //puts("server recv data");
			  while((readBytes = recv(workSock, (char*)&buf, BUFFER_SIZE*sizeof(char), 0)) < 0)
										// receive data from client
			  {
				  perror("func recv data");				  
				  printf("errno data %d\n", errno);		// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
			  //printf("server_readbytes %d\n", readBytes);	
			  //printf("server_fwrite filePointer %lld\n",  filePointer);
			  fwrite((char*)buf, readBytes, 1, file);		// write data to file
			  filePointer += readBytes;
			 // printf("server_fwrite2 filePointer2 %lld\n",  filePointer);
			}
		}
		//printf("server_fclose\n");
		if(ftell(file) >= 0)						// check is file open
		  fclose(file);		
		//printf("server_close_socket\n");
		if(filePointer == fileSize)
		  printf("file '%s' %lld bytes successful received\n", filePath, fileSize);
		else
		{
		  printf("some errors with file '%s' %lld bytes\n", filePath, fileSize);
		  printf("%lld bytes only received\n", filePointer);
		}
		if(close((workSock)) < 0)					// close connection
		{
			perror("func close workSock");
			return -1;
		}	
		ind--;
		//printf("-\n");	  
	}
	return 0;	
}

int startClient(char *hostName, char *port, char *filePath)
{
  
	struct 		sockaddr_in hostAddr;    				// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for outcomming
	int 		readBytes;						// count of	
	int 		sendBytes;
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	uint8_t 	bufOOBin;	
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
	tim.tv_nsec = 800000L;							// sleep time in nanosec
	uint8_t bufOOB = 5;
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		if(((listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
		{
			perror("func socket");
			return -1;
		}  
		ind+=2;
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
		while(send(listenSock, (char*)filePath, strlen(filePath)+1, 0) < 0)
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
		while(send(listenSock, (char*)&fileSize, sizeof(long long), 0) < 0)
		{								// send fileSize
		  perror("func send fileSize");
		  printf("errno: %d\n", errno);
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		} 								
		
		//puts("client_recv_filePointer");
										// recv filePointer from server
		while(recv(listenSock,(char*)&filePointer, sizeof(long long), 0) < 0)
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
		if(close(listenSock) < 0)			
		  perror("sgn close listenSock");
		ind-=2;
		//puts("client_file_close");
		if(ftell(file) >= 0)						// check is file open
		  fclose(file);		
		return 0;		
	}
}			


int main(int argc, char *argv[])
{
	if(argc < 4)
	{
		puts(ARG_ERROR_MESS);
		perror("invalid command-line arguments");
		return -1;
	}	
	struct sigaction closeTerm, sendOOB, recvOOB;
	closeTerm.sa_sigaction =&hdl_SIGINT;
	closeTerm.sa_flags = SA_SIGINFO;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)				// set handler for SIGINT signal (CTRL+C)
	{
		perror("main sigaction closeTerm");
		return -1;
	}	
	if(!strcmp(argv[1], "server"))	
	{
	  recvOOB.sa_sigaction =&hdl_SIGURG;
	  recvOOB.sa_flags = SA_SIGINFO;
	  if(sigaction(SIGURG, &recvOOB, NULL) < 0)				// set handler for SIGURG signal (server)
	  {
		  perror("main sigaction recvOOB");
		  return -1;
	  }
	  startServer(argv[2], argv[3]);	
	}
	if(!strcmp(argv[1], "client"))
	{										
	  if(access(argv[4], F_OK ) < 0)					// check file exist
	  {
	    printf("file %s does not exist\n", argv[4]);
	    perror("invalid fileName");
	    return -1;
	  }
	  sendOOB.sa_sigaction =&hdl_SIGTSTP;
	  sendOOB.sa_flags = SA_SIGINFO;
	  if(sigaction(SIGTSTP, &sendOOB, NULL) < 0)				// set handler for SIGTSTP signal (CTRL+Z) (client)
	  {
		  perror("main sigaction sendOOB");
		  return -1;
	  }
	  startClient(argv[2], argv[3], argv[4]);
	}
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
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);
    	return 0;
}
