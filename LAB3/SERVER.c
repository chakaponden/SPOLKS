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
#include <poll.h>


#define	MAX_PENDING		16				// max pending client connections
#define	BUFFER_SIZE  		1024				// for incomming
#define ARG_ERROR_MESS		"\n[protocol] [ip] [port]\n\n[protocol] need to be 'tcp'/'udp'\n"
#define	MAX_FILEPATH_LENGHT	64				// in bytes

int 		workSock, listenSock;				// socket descriptors
int 		ind = 0;					// indicator that any socket in open
int 		nClients = 0;					// count of connected clients
FILE*		file;
int 		OOB = 0;					// indicator for OOB (see signal handles)



struct clientInf
{
  struct in_addr addr;						// client addr
  char filePath[MAX_FILEPATH_LENGHT];				// fileName that client send
  pid_t pid;							// client's child proc pid
}clientVect[MAX_PENDING];					// vector of connected clients

void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)	// handler for SIGINT (Ctrl+C) withinsignal
{
    if (sig==SIGINT)
    {	 
	if(ind > 1)
	{
	  if(shutdown(listenSock, SHUT_RDWR) < 0)		// deny connection
	    fprintf(stderr, "PID: %d shutdown listenSock signal errno: %d\n", getpid(), errno);
	  if(close(listenSock) < 0)			
		  fprintf(stderr, "PID: %d close listenSock signal errno: %d\n", getpid(), errno);
	  else
		  ind-=2;
	}
	if(ind)   
	{
		if(shutdown(workSock, SHUT_RDWR) < 0)		// deny connection
			fprintf(stderr, "PID: %d shutdown workSock signal errno: %d\n", getpid(), errno);
		if(close(workSock) < 0)		
			fprintf(stderr, "PID: %d close listenSock signal errno: %d\n", getpid(), errno);	
		else
			ind--;
	}
	if(ftell(file) >= 0)					// check is file open
		  fclose(file);
    }
}

void hdl_SIGURG(int sig, siginfo_t *siginfo, void *context)	// handler for OOB data received signal (server)
{
  if(sig==SIGURG)
    OOB = 1;
}

int startServerTcp(char *hostName, char *port)
{
	long long	fileSize = 1;
	struct 		sockaddr_in hostAddr, clientAddr;	// this machine + client	
	int		clientAddrLen;
	int 		so_reuseaddr = 1;			// for setsockop SO_REUSEADDR set enable
    	int		childPid, status, retChild;   		// parameters for child process
    	int 		otherClient = 0;			// if other client already send this file
    	char		filePath[MAX_FILEPATH_LENGHT];
	int 		i = 2;					// for loops
	int 		highDescSocket, retVal = 0;		// for select/poll
	for(i = 0; i < MAX_PENDING; i++)			// initial vector of connected clients
	  clientVect[i].pid = -1;	
	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));			// convert host byte order -> network byte order
	/*
	if(!htons(inet_aton(hostName, hostAddr.sin_addr.s_addr))// new func convert IPv4 char* -> IPv4 bin 
	{	  						// (+ host byte order -> network byte order too)
		fprintf(stderr, "inet_aton errno: %d\n", errno);
        	return -1;		
	}
	*/	
    	hostAddr.sin_addr.s_addr = inet_addr(hostName);		// old func convert IPv4 char* -> IPv4 bin
								// (+ host byte order -> network byte order too) 
	if(((listenSock) = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)		
   	{
        	fprintf(stderr, "socket errno: %d\n", errno);
        	return -1;
    	}    
	ind += 2;
	setsockopt(listenSock, SOCK_STREAM, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
								// reuse ADDR when socket in TIME_WAIT condition
    	if(bind((listenSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
								// socket is associated with IPv4 address
    	{
        	fprintf(stderr, "bind errno: %d\n", errno);
        	return -1;
    	}
    	if(listen((listenSock), MAX_PENDING) < 0)		// set socket to listen status
	{
		fprintf(stderr, "listen errno: %d\n", errno);
        	return -1;		
	}
	fcntl(listenSock, F_SETFL, O_NONBLOCK);			// set socket to NON_BLOCKING
	/*
	fd_set tempSet, workSet;		
	struct timeval time_out;				// timeout
								
	
	highDescSocket = listenSock;				// set high socket descriptor for select func
	FD_ZERO (&workSet);
	FD_SET (listenSock,&workSet);
	tempSet = workSet;
	*/
	struct pollfd tempSet;
	int time_out = 40;					// 40 milisec
	tempSet.fd = listenSock;
	tempSet.events = POLLIN;
	highDescSocket = 1;
	clientAddrLen = sizeof(clientAddr);
	while(1)								
    	{
	  if(nClients < MAX_PENDING)
	  {	
	    /*
	    tempSet = workSet;					// set tmpSet; some OS chanche tmpSet after select
	    time_out.tv_sec = 0; time_out.tv_usec = 10000;	// set timeout; some OS chanche time_out after select
	    
		while((retVal = select(highDescSocket+1,&tempSet,NULL,NULL,&time_out)) < 0)
		{
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			  {
			    fprintf(stderr, "select errno: %d\n", errno);	
								// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
								//
			    return -1;    			// errno == 4 means EINTR == Interrupted system call
			  }
		}
	    */  
		while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
		{
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			  {
			    fprintf(stderr, "select errno: %d\n", errno);	
								// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
								//
			    return -1;    			// errno == 4 means EINTR == Interrupted system call
			  }
		}
								// wait for incomming connections on listenSock within time_out
		if(retVal)  
		{
		  //printf("server_accept_wait_for_client\n");
		  if(((workSock) = accept((listenSock), 
		    (struct sockaddr*)&clientAddr, &clientAddrLen)) < 0)				
		  {						// wait for client NON_BLOCKING
			  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			  {
			    fprintf(stderr, "accept errno: %d\n", errno);	
								// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
								// (no available connections on NON_BLOCKING socket)
			    return -1;    			// errno == 4 means EINTR == Interrupted system call
			  }
		  }
		  //printf("server_worksock: %d, nClients: %d\n", workSock, nClients);
		  if(workSock >= 0)
		  {
		    /*
		    FD_ZERO (&temp);
		    FD_SET (workSock,&temp);		  
		    if(select(0,NULL,NULL,&temp,&time_out) != 1)// timeout
		      break;    
		    */
		    while(recv((workSock), (char*)&filePath, MAX_FILEPATH_LENGHT*sizeof(char), MSG_WAITALL)
									      < MAX_FILEPATH_LENGHT*sizeof(char))
								 // receive filePath from client
		    {
			fprintf(stderr, "recv filePath errno: %d\n", errno);
								 // errno == 4 means EINTR == Interrupted system call 
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
			  return -1;				// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		    }
		    /*
		    if(select(0,NULL,NULL,&temp,&time_out) != 1)// timeout
		      break;
		    */
		    while(recv((workSock), (char*)&fileSize, sizeof(long long), MSG_WAITALL) < sizeof(long long))
								// receive fileSize from client
		    {
		      fprintf(stderr, "recv fileSize errno: %d\n", errno);
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) 
			      return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
								// errno == 4 means EINTR == Interrupted system call
		    }
		    for(i = 0; i < MAX_PENDING; i++)		// check if file is already downloading from other client
		    {
		      if(clientVect[i].pid != -1)
		      {
			if(!strcmp(clientVect[i].filePath, filePath))
			{
			  otherClient = 1;
			  break;
			}
		      }		      
		    }
		    if(otherClient)				// then send filePointer
		    {						// that point to EOF (filePointer==fileSize)
		      while(send(workSock, (char*)&fileSize, sizeof(long long), 0) < sizeof(long long))
		      {
			fprintf(stderr, "send fileSize errno: %d\n", errno);
								// errno == 4 means EINTR == Interrupted system call 
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		      } 
		      otherClient = 0;
		    }
		    else
		    {    
		      switch((childPid = fork()))
		      {
			      case -1: 				// error fork
				      {
					      perror("fork");
					      return -1;				
				      }	
			      case 0 : 				// child process
				      {			
					      status = serverProcessingTcp(workSock, &filePath, fileSize);	  
					      return status;	
				      }			
			      default :								// parent process
				      {		
					      for(i = 0; i < MAX_PENDING; i++)			// check if file is 
					      {							// already downloading from other client
						if(clientVect[i].pid == -1)
						{
						  strcpy((clientVect[i].filePath), filePath);	// add filePath to clientVect
						  clientVect[i].addr = clientAddr.sin_addr;	// add client's sin_addr
						  clientVect[i].pid = childPid;			// add pid
						  break;
						}		      
					      }
					      nClients++;
					      break;				
				      }
		      }       
		    }
		  }
	    }
	  }
		if(nClients)					// if any child proc exists
		{						// -1 == wait for all child proc 
		  switch((childPid = waitpid(-1, &status, WNOHANG))) 
		  {						// WNOHANG == return control immediately
		    case -1:					// error waitpid
		    {
		      perror("waitpid");
		      return -1;
		    }
		    case  0:					// no child proc terminated now
		    {
		      break;
		    }
		    default:					// child proc terminated
		    {		      
		      for(i = 0; i < MAX_PENDING; i++)		
		      {
			if(clientVect[i].pid == childPid)
			{
			  clientVect[i].pid = -1;		// set pid as -1
			  break;
			}		      
		      }
		      nClients--;
		      if (WIFEXITED (status))
			retChild = WEXITSTATUS (status);	// get child return value
		      break;
		    }		
		  }
		}
	  
		//printf("-\n");
	}
	return 0;	
}



int serverProcessingTcp(int workSock, char *oldFilePath, long long oldFileSize)
{

  long long	fileSize = oldFileSize;
  long long 	filePointer = 0;  
  char 		buf[BUFFER_SIZE];				// buffer for incomming
  int 		readBytes = 1;					// count of	
  int 		clientFirstPacket = 1;				// client first packet indicator
  char 		c[64] = {0};					// for terminal input
  char		filePath[MAX_FILEPATH_LENGHT];
  long long 	localFileSize = 0;
  uint8_t 	bufOOBin;
  struct sigaction recvOOB;
  recvOOB.sa_sigaction =&hdl_SIGURG;
  recvOOB.sa_flags = SA_SIGINFO;
  if(sigaction(SIGURG, &recvOOB, NULL) < 0)			// set handler for SIGURG signal (server)
  {
    fprintf(stderr, "PID: %d sigaction recvOOB errno: %d\n", getpid(), errno);
    return -1;
  }  
  fcntl(workSock, F_SETOWN, getpid()); 				// set pid which will be 
								// recieve SIGIO and SIGURG on descriptor' events
  strcpy(filePath, oldFilePath);				// copy filePath
  ind = 1;	
  printf("PID: %d file '%s' %lld bytes start downloading\n", getpid(), filePath, fileSize);
  while(filePointer < fileSize && readBytes)							
  {
    
    if(OOB)							// if OOB data is received
    {
      //puts("recv OOB");					
      OOB = bufOOBin = 0;					// recv sometimes return to perror "Resource temporarily unavailable"
      while(recv(workSock, &bufOOBin, sizeof(bufOOBin), MSG_OOB) < 0)		
      {			    					// SOMETIMES SET ERRNO TO 11 OR 4
	fprintf(stderr, "PID: %d recv SIGURG errno: %d\n", getpid(), errno);
								// errno == 4 means EINTR == Interrupted system call 
	if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)		
	  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
      }
      printf("PID: %d signal SIGURG. OOB data received: %d\nPID: %d file '%s' %lld bytes for download left\n", 
	     getpid(), bufOOBin, getpid(), filePath, (fileSize - filePointer));
    }
    
    
    //printf("server_select %lld %lld\n", filePointer, fileSize);
    /*if(select(0,NULL,NULL,&temp,&time_out) < 0)
    {
      puts("server timeout 10s reached");
      break;
    }     */     
    //printf("server_recv data\n");
								// check if file exist (filename gets from client message)
        if(clientFirstPacket)					// if first packet from client
	{		
	  clientFirstPacket = 0;
	  /*
	  while((readBytes = recv((workSock), (char*)filePath, MAX_FILEPATH_LENGHT*sizeof(char), 0)) < 0)
								// receive filePath from client
	  {
		  fprintf(stderr, "PID: %d recv file errno: %d\n", getpid(), errno);
								// errno == 4 means EINTR == Interrupted system call 
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)			
		    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		  }
	  //printf("server_readbytes num %d, message: %s\n", readBytes, filePath);
	  while((readBytes = recv((workSock), (char*)&fileSize, sizeof(long long), MSG_WAITALL)) < sizeof(long long))
								// receive fileSize from client
	  {
	    fprintf(stderr, "PID: %d recv fileSize errno: %d\n", getpid(), errno);
	    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)// errno == 4 means EINTR == Interrupted system call 
		    return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
	  }
	  filePath[readBytes] = '\0';
	  */	  
	  //printf("server_recv fileSize %lld\n",  fileSize);
	  //if(!readBytes)
	  //	  break;
	  //printf("server_first_realloc\n"); 
	 
	  
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
	      printf("PID: %d Do you want to redownload %s file? Y/N\n", getpid(), filePath);
	      fgets(c, sizeof(c)*sizeof(char), stdin);		// input
	      if(c[0] == 'Y')					// press 'Y'
	      {
		file = fopen(filePath, "wb");			// create file
		filePointer = 0;					
	      }
	      else						// press 'N'
	      {	
		filePointer = fileSize;				// send filePointer that point to EOF (filePointer==fileSize)
	      }
	    }
	    else
	    {
	      printf("PID: %d Resume downloading %lld bytes %s file? Y/N\nY = resume, N = redownload\n",
								getpid(), (fileSize-localFileSize), filePath);
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
	  while(send(workSock, (char*)&filePointer, sizeof(long long), 0) < sizeof(long long))	
	  {
	    fprintf(stderr, "PID: %d send filePointer fileSize errno: %d\n", getpid(), errno);
								// errno == 4 means EINTR == Interrupted system call 
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
	fprintf(stderr, "PID: %d recv data errno: %d\n", getpid(), errno);				  
								// errno == 4 means EINTR == Interrupted system call 
	if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
	  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
      }
      //printf("server_readbytes %d\n", readBytes);	
      //printf("server_fwrite filePointer %lld\n",  filePointer);
      fwrite((char*)buf, readBytes, 1, file);			// write data to file
      filePointer += readBytes;
	// printf("server_fwrite2 filePointer2 %lld\n",  filePointer);
    }
  }
  //printf("server_fclose\n");
  if(ftell(file) >= 0)						// check is file open
    fclose(file);		
  //printf("server_close_socket\n");
  if(filePointer == fileSize)
    printf("PID: %d file '%s' %lld bytes successful received\n", getpid(), filePath, fileSize);
  else
  {
    printf("PID: %d some errors with file '%s' %lld bytes\nPID: %d %lld bytes only received\n", 
	   getpid(), filePath, fileSize, getpid(), filePointer);
  }
  if(shutdown(workSock, SHUT_RDWR) < 0)				// deny connection
  {
    fprintf(stderr, "PID: %d shutdown workSock errno: %d\n", getpid(), errno);
    return -1;
  }
  if(close((workSock)) < 0)					// close connection
  {
    fprintf(stderr, "PID: %d close workSock errno: %d\n", getpid(), errno);
    return -1;
  }
  ind--;
  return 0;
}


int startServerUdp(char *hostName, char *port)
{
  return 0;
}

int main(int argc, char *argv[])
{
	if(argc < 4 || 
	  ((strcmp(argv[1], "tcp")) && strcmp(argv[1], "udp")))			// check command-line arguments
	{
		puts(ARG_ERROR_MESS);
		perror("invalid command-line arguments");
		return -1;
	}	
	struct sigaction closeTerm;
	closeTerm.sa_sigaction =&hdl_SIGINT;
	closeTerm.sa_flags = SA_SIGINFO;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)				// set handler for SIGINT signal (CTRL+C)
	{
		perror("main sigaction closeTerm");
		return -1;
	}		
	if(!strcmp(argv[1], "tcp"))						// tcp
	{	  	   
	  startServerTcp(argv[2], argv[3]);
	}
	else									// udp
	{
	  startServerUdp(argv[2], argv[3]);
	}	 
	if(ind > 1)
	{
		if(shutdown(listenSock, SHUT_RDWR) < 0)				// deny connection
		{
			fprintf(stderr, "PID: %d shutdown listenSock main errno: %d\n", getpid(), errno);
			return -1;
		}
		if(close(listenSock) < 0)		
		{
			fprintf(stderr, "PID: %d close listenSock main errno: %d\n", getpid(), errno);	
			return -1;
		}
		ind-=2;
	}
	if(ind)
	{
		if(shutdown(workSock, SHUT_RDWR) < 0)				// deny connection
		{
			fprintf(stderr, "PID: %d shutdown workSock main errno: %d\n", getpid(), errno);
			return -1;
		}
		if(close(workSock) < 0)		
		{
			fprintf(stderr, "PID: %d close listenSock main errno: %d\n", getpid(), errno);	
			return -1;					
		}
	}
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);
    	return 0;
}
