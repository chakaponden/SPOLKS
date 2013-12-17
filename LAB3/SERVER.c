/* many clients (limit MAX_PENDING) send files
 * server receive and save local
 * if some clients send file with the same fileName
 * then server drop last client connection with same fileName
 * and continue to download from first client with same fileName
 * pressing 'SPACE' key on client side == send OOB data '5' to server
 * CTRL+C == default force terminate server
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

// #define SO_REUSEPORT 15						// need if kernel < 3.9
// [CMD TO CHECK KERNEL VER]:
// uname -r 


#define	MAX_PENDING		16				// max pending client connections
#define	BUFFER_SIZE  		1024				// for incomming
#define ARG_ERROR_MESS		"\n[protocol] [ip] [port]\n\n[protocol] need to be 'tcp'/'udp'\n"
#define	MAX_FILEPATH_LENGHT	64				// in bytes

int 		workSock, listenSock;				// socket descriptors
int 		ind = 0;					// indicator that any socket in open
int 		nClients = 0;					// count of connected clients
FILE*		file;
pid_t		parentPid = -1;
//int 		OOB = 0;					// indicator for OOB (see signal handles)
int a;


struct clientInf
{
  struct in_addr	addr;					// client addr
  unsigned short   	port;
  char 			filePath[MAX_FILEPATH_LENGHT];		// fileName that client send
  pid_t 		pid;					// client's child proc pid
  int			status;					// 0 == waiting for filePath
								// 1 == waiting for fileSize
								// 2 == waiting for data from client
}clientVect[MAX_PENDING];					// vector of connected clients

void hdl_SIGINT_TCP(int sig, siginfo_t *siginfo, void *context)	// handler for SIGINT (Ctrl+C) withinsignal
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
	exit(0);
    }
}

void hdl_SIGINT_UDP(int sig, siginfo_t *siginfo, void *context)	// handler for SIGINT (Ctrl+C) withinsignal
{
    if (sig==SIGINT)
    {	 
        if(parentPid == getpid())
	{
	  if(ind)   
	  {
		  if(close(workSock) < 0)		
			  fprintf(stderr, "PID: %d close listenSock signal errno: %d\n", getpid(), errno);	
		  else
			  ind--;
	  }
	}
	if(ftell(file) >= 0)					// check is file open
		  fclose(file);
	exit(0);
    }
}
/*
void hdl_SIGURG(int sig, siginfo_t *siginfo, void *context)	// handler for OOB data received signal (server)
{
  if(sig==SIGURG)
    OOB = 1;
}
*/
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
	{
	  clientVect[i].pid = -1;	
	  clientVect[i].status = 0;
	}
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
	setsockopt(listenSock, SOL_SOCKET, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
								// reuse ADDR when socket in TIME_WAIT condition
	/*setsockopt(listenSock, SOL_SOCKET, 
						SO_REUSEPORT, &so_reuseaddr, sizeof so_reuseaddr);
								// reuse PORT when socket in TIME_WAIT condition*/
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
			    fprintf(stderr, "poll errno: %d\n", errno);	
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



int serverProcessingTcp(int oldWorkSock, char *oldFilePath, long long oldFileSize)
{

  long long	fileSize = oldFileSize;
  long long 	filePointer = 0;  
  char 		buf[BUFFER_SIZE];				// buffer for incomming
  int 		readBytes = 1;					// count of	
  int 		workSock = oldWorkSock;
  int 		clientFirstPacket = 1;				// client first packet indicator
  char 		c[64] = {0};					// for terminal input
  int		highDescSocket, retVal;				// for select()
  char		filePath[MAX_FILEPATH_LENGHT];
  long long 	localFileSize = 0;
  uint8_t 	bufOOBin;
  int		recvFlag = 0;
  /*
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
  */
  strcpy(filePath, oldFilePath);				// copy filePath
  ind = 1;  
  /*
  fd_set tempSet, workSet;		
  struct timeval time_out;					// timeout	
  highDescSocket = workSock;					// set high socket descriptor for select func
  FD_ZERO (&workSet);
  FD_SET (workSock,&workSet);
  tempSet = workSet;
  */
  fcntl(workSock, F_SETFL, O_NONBLOCK);				// set socket to NON_BLOCKING
  struct pollfd tempSet;
  int time_out = 0;						// 0 milisec
  tempSet.fd = workSock;
  tempSet.events = POLLPRI;
  highDescSocket = 1;
  printf("PID: %d file '%s' %lld bytes start downloading\n", getpid(), filePath, fileSize);  
  while(filePointer < fileSize && readBytes)							
  {
    /*
    tempSet = workSet;						// set tmpSet; some OS chanche tmpSet after select
    time_out.tv_sec = 0; time_out.tv_usec = 0;			// set timeout; some OS chanche time_out after select
	
    while((retVal = select(highDescSocket+1,NULL,NULL,&tempSet,&time_out)) < 0)
    {								// wait for OOB data on workSock within time_out
      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
	      {
		fprintf(stderr, "select errno: %d\n", errno);	
								// errno == 11 means EAGAIN or EWOULDBLOCK == Try again						    
		return -1;    					// errno == 4 means EINTR == Interrupted system call
	      }
    }
    */
    while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
    {
      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
      {
	fprintf(stderr, "PID: %d poll errno: %d\n", getpid(), errno);	
					    // errno == 11 means EAGAIN or EWOULDBLOCK == Try again
	return -1;    			// errno == 4 means EINTR == Interrupted system call
      }
    }
    if(retVal)							// if OOB data is received
    {
      //puts("recv OOB");					
      //OOB = bufOOBin = 0;					// recv sometimes return to perror "Resource temporarily unavailable"
      while(recv(workSock, &bufOOBin, sizeof(int), MSG_OOB) < 0)		
      {			    					// SOMETIMES SET ERRNO TO 11 OR 4
	fprintf(stderr, "PID: %d recv OOB errno: %d\n", getpid(), errno);
								// errno == 4 means EINTR == Interrupted system call 
	if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)		
	  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
      }
      printf("PID: %d OOB data received: %d\nPID: %d file '%s' %lld bytes for download left\n", 
	     getpid(), bufOOBin, getpid(), filePath, (fileSize - filePointer));
    }
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
      if((filePointer + BUFFER_SIZE*sizeof(char)) <= fileSize)
	recvFlag = MSG_WAITALL;
      else
	recvFlag = 0;      
      while((readBytes = recv(workSock, (char*)&buf, BUFFER_SIZE*sizeof(char), recvFlag)) < 0)
				    // receive data from client
      {								// errno == 4 means EINTR == Interrupted system call 
	if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
	{
	  fprintf(stderr, "PID: %d recv data errno: %d\n", getpid(), errno);
	  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
	}
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
	long long	fileSize = 1;
	struct 		sockaddr_in hostAddr, clientAddr;	// this machine + client	
	int		clientAddrLen;
	int 		so_reuseaddr = 1;			// for setsockop SO_REUSEADDR set enable
    	int		childPid, status, retChild;   		// parameters for child process
    	int 		otherClient = 0, oldClient = 0;		// if other client already send this file
    	char		filePath[MAX_FILEPATH_LENGHT];
	char 		buf[BUFFER_SIZE];				// buffer for incomming
	int 		i;					// for loops
	long 		recvBuf = MAX_PENDING*(BUFFER_SIZE*sizeof(char)*6000);
	int 		highDescSocket, retVal = 0;		// for select/poll
	int 		confirmMess = 0;
	int		readBytes = 0;
	for(i = 0; i < MAX_PENDING; i++)			// initial vector of connected clients
	{
	  clientVect[i].pid = -1;	
	  clientVect[i].status = 0;
	}
	 struct timespec tim, tim2;
		      tim.tv_sec = 0;
		      tim.tv_nsec = 800000L;
	parentPid = getpid();
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
	if(((workSock) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)		
   	{
        	fprintf(stderr, "socket errno: %d\n", errno);
        	return -1;
    	}    
	ind++;
	setsockopt(workSock, SOL_SOCKET, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
	/*setsockopt(workSock, SOL_SOCKET, 
						SO_REUSEPORT, &so_reuseaddr, sizeof so_reuseaddr);
								// reuse PORT when socket in TIME_WAIT condition*/
	setsockopt(workSock, SOL_SOCKET, 
						SO_RCVBUF, &recvBuf, sizeof recvBuf);
	
	setsockopt(workSock, SOL_SOCKET, 
						SO_SNDBUF, &recvBuf, sizeof recvBuf);
	
	
    	if(bind((workSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
								// socket is associated with IPv4 address
    	{
        	fprintf(stderr, "bind errno: %d\n", errno);
        	return -1;
    	}
    	//fcntl(workSock, F_SETFL, O_NONBLOCK);			// set socket to NON_BLOCKING
	struct pollfd tempSet;
	int time_out = 0;					// 40 milisec
	tempSet.fd = workSock;
	tempSet.events = POLLIN;
	highDescSocket = 1;
	clientAddrLen = sizeof(clientAddr);
	while(1)								
    	{
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
			  clientVect[i].status = 0;		// set status as 0
			  break;
			}		      
		      }
		      nClients--;
		      /*
		      //system("clear");		      
		      for(i = 0; i < MAX_PENDING; i++)		
			      printf("[%d]: %d %d ", i, clientVect[i].pid, clientVect[i].status);		      
		      printf("\n\n");
		      */
		      if (WIFEXITED (status))
			retChild = WEXITSTATUS (status);	// get child return value
		      /*
		      struct timespec tim, tim2;
		      tim.tv_sec = 0;
		      tim.tv_nsec = 800000L;							// sleep time in nanosec
		      if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		  {
		      printf("nano sleep system call failed");
		      printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			perror("nano sleep system call failed");
			printf("errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		      }
		  }*/
		      break;
		    }		
		  }
		}
	  if(nClients < MAX_PENDING)
	  {	
		while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
		{
		  fprintf(stderr, "poll errno: %d\n", errno);	
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			  {
								// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
								//
			    return -1;    			// errno == 4 means EINTR == Interrupted system call
			  }
		}
		
		// wait for incomming connections on listenSock within time_out
		//printf("retVal: %d\n", retVal);
		if(retVal)  
		{
		    
		    while((readBytes = recvfrom(workSock, (char*)&buf, BUFFER_SIZE*sizeof(char), MSG_PEEK, 
						(struct sockaddr*)&clientAddr, &clientAddrLen)) < 0)
				    // receive data from client
		    {								// errno == 4 means EINTR == Interrupted system call 
		      fprintf(stderr, "recvFrom filePath OK3 errno: %d\n", errno);
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			fprintf(stderr, "recvFrom filePath OK3 bad3 errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		      }
		    }			    
		    for(i = 0; i < MAX_PENDING; i++)				// check if client already establish connection
		    {
		      //printf("%d\n", i);
		      if(clientVect[i].status)
		      {		
			//printf("status %d\n", i);
			if((clientVect[i].addr.s_addr == clientAddr.sin_addr.s_addr) &&	// check port && ip	
						      (clientVect[i].port == clientAddr.sin_port))			
			{							// if client already establish connection and			  
			  //printf("ip %d\n", i);
			  switch(clientVect[i].pid)
			  {
			    case -1:						// recv fileSize
			    {	
			      //puts("fileSize");
			      if(readBytes == sizeof(long long))
			      {
				while((readBytes = recvfrom(workSock, (char*)&fileSize, sizeof(long long), MSG_WAITALL, 	// remove from queue
						(struct sockaddr*)&clientAddr, &clientAddrLen)) < sizeof(long long))
				    // receive data from client
				{								// errno == 4 means EINTR == Interrupted system call 
				  fprintf(stderr, "recvFrom filePath OK2 errno: %d\n", errno);
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				  {
				    fprintf(stderr, "recvFrom fileSize OK errno: %d\n", errno);
				    return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				  }
				}	
				//puts("fileSize1");
				//printf("recv '%s' fileSize: %lld\n", clientVect[i].filePath, fileSize);
				switch((childPid = fork()))
				{
					case -1: 				// error fork
						{
							perror("fork");
							return -1;				
						}	
					case 0 : 				// child process
						{			
							status = serverProcessingUdp(workSock, hostAddr, fileSize, clientAddr, i);	  
							return status;	
						}			
					default :								// parent process
						{		
							clientVect[i].pid = childPid;			// add child pid    
							nClients++;
							break;				
						}
				}			      
				confirmMess = 1012;				
				clientVect[i].status++;
				oldClient = 1;
				//puts("old ok 1");
			      }
			      else
			      {
				
				while((recvfrom(workSock, (char*)&fileSize, readBytes, MSG_WAITALL, 	// remove from queue
						(struct sockaddr*)&clientAddr, &clientAddrLen)) < readBytes)
				    // receive data from client
				{								// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				  {
				    fprintf(stderr, "recvFrom fileSize NOT_OK bad0 errno: %d\n", errno);
				    return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				  }
				}
				
				//puts("fileSize BAD");
				confirmMess = 3333;				// request fileSize
				oldClient = 1;
				//puts("old not 1");
			      }
			      break;
			    }
			    default:						// recv data
			    {
			       if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
			      {				// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				  {
				    fprintf(stderr, "nanosleep errno: %d\n", errno);
				    return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				  }
			      }
			      //puts("data other1");
			      //printf("RECV DATA, '%s' port: %d\n", clientVect[i].filePath, clientVect[i].port);
			      oldClient = 2;
			      //puts("old not 2");
			      break;
			    }	
			  }
			  break;			  
			}
		      }      
		    }
		    if(!oldClient)						// if filePath recveived
		    {		
		      //printf("filePath, readBytes: %d\n", readBytes);
		       if(readBytes == MAX_FILEPATH_LENGHT*sizeof(char))
		       {	
			  while((readBytes = recvfrom(workSock, (char*)&filePath, MAX_FILEPATH_LENGHT*sizeof(char), MSG_WAITALL, 	// remove from queue
				    (struct sockaddr*)&clientAddr, &clientAddrLen)) < MAX_FILEPATH_LENGHT*sizeof(char))
			      // receive data from client
			  {								// errno == 4 means EINTR == Interrupted system call 
			    fprintf(stderr, "recvFrom filePath OK1 errno: %d\n", errno);
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			    {
			      fprintf(stderr, "recvFrom filePath OK1 bad1 errno: %d\n", errno);
			      return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			    }
			  }			  
			  //printf("filePath recv: %s\n", filePath);
			  for(i = 0; i < MAX_PENDING; i++)			// search if this filePath already downloading
			  {							// from other client
			    if(clientVect[i].status > 1)
			    {
			      if(!strcmp(clientVect[i].filePath, filePath))
			      {
				confirmMess = 5510;				// recv filePath already exist
				otherClient = 1;				// and recv from other client
				break;
			      }	
			    }
			  }
			  if(!otherClient)
			  {
			    for(i = 0; i < MAX_PENDING; i++)		// search for free element in clientVect[MAX_PENDING]
			    {							
			      if(clientVect[i].status == 0)
			      {			    
				strcpy((clientVect[i].filePath), filePath);	// add filePath to clientVect
				clientVect[i].addr = clientAddr.sin_addr;	// add client's sin_addr
				clientVect[i].port = clientAddr.sin_port;	// add client's port
				confirmMess = 8102;
				clientVect[i].status++;			// update status == wait for fileSize
				break;
			      }		      
			    }
			  }
			  else
			    otherClient = 0;
			  //puts("filePath1");
			  			  
		       }		       
		       else
		       {	

			 //puts("filePath BAD1");
			 while((recvfrom(workSock, (char*)&filePath, readBytes, MSG_WAITALL, 	// remove from queue
				    (struct sockaddr*)&clientAddr, &clientAddrLen)) < 0)
			      // receive data from client
			  {								// errno == 4 means EINTR == Interrupted system call 
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			    {
			      fprintf(stderr, "recvFrom filePath NOT_OK errno: %d\n", errno);
			      return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			    }
			  }
			  //puts("filePath BAD2");

			 confirmMess = 6666;						// request filePath
			 //printf("6666 readBytes: %d\n", readBytes);			 
			 //oldClient = 2;
			  
		       }
		       
		    }
		    if(oldClient < 2)
		    {
			//printf("send confirmMess1: %d\n", confirmMess);					// send confirmMess
		     while(sendto(workSock, (char*)&confirmMess, sizeof(int), MSG_WAITALL, 
			(struct sockaddr*)&clientAddr, clientAddrLen) < sizeof(int))
		      {			
			fprintf(stderr, "sendto confirmMess: %d errno: %d\n", confirmMess, errno);
								// errno == 4 means EINTR == Interrupted system call 
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		      } 
		      //printf("send confirmMess: %d\n", confirmMess);		      
		      //confirmMess = 1;
		    }	    
		  //}
		}
	  }
	  oldClient = 0;
	  //puts("old 0");
		
	}	
	return 0;
}

int serverProcessingUdp(int oldWorkSock, struct sockaddr_in oldHostAddr, long long oldFileSize, struct sockaddr_in oldClientAddr, int index)
{

  //int 		workSock = oldWorkSock;
  struct 	sockaddr_in clientAddr = oldClientAddr;
  struct 	sockaddr_in clientAddrRecv;
  struct 	sockaddr_in hostAddr = oldHostAddr;
  int		clientAddrLen = sizeof(clientAddr);
  int		clientAddrRecvLen = sizeof(clientAddrRecv);
  long long	fileSize = oldFileSize;
  long long 	filePointer = 0;  
  char 		buf[BUFFER_SIZE];				// buffer for incomming
  int 		readBytes = 1;					// count of	
  int 		clientFirstPacket = 1;				// client first packet indicator
  char 		c[64] = {0};					// for terminal input
  int		highDescSocket, retVal;				// for select()
  char		filePath[MAX_FILEPATH_LENGHT];
  long long 	localFileSize = 0;
  uint8_t 	bufOOBin;
  long long	recvFlag = 0;
  int 		recvMess = 0;
  int 		count = 0;
  long long	otherPacket = 0;
  long long 	maxOtherPacket = (BUFFER_SIZE*MAX_PENDING*100);
  			  struct timespec tim, tim2;
		      tim.tv_sec = 0;
		      tim.tv_nsec = 800000L;							// sleep time in nanosec
  /*
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
  */
  strcpy(filePath, clientVect[index].filePath);			// copy filePath
  ind = 1;  
  /*
  if(((workSock) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)		
   	{
        	fprintf(stderr, "socket errno: %d\n", errno);
        	return -1;
    	}    
	ind++;
	setsockopt(workSock, SOL_SOCKET, 
					SO_REUSEADDR, &readBytes, sizeof readBytes);
								// reuse ADDR when socket in TIME_WAIT condition
	setsockopt(workSock, SOL_SOCKET, 
					SO_REUSEPORT, &readBytes, sizeof readBytes);
								// reuse PORT when socket in TIME_WAIT condition
    	if(bind((workSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
								// socket is associated with IPv4 address
    	{
        	fprintf(stderr, "bind errno: %d\n", errno);
        	return -1;
    	}
    	
    	  fcntl(workSock, F_SETFL, O_NONBLOCK);				// set socket to NON_BLOCKING
    	  */
  /*
  fd_set tempSet, workSet;		
  struct timeval time_out;					// timeout	
  highDescSocket = workSock;					// set high socket descriptor for select func
  FD_ZERO (&workSet);
  FD_SET (workSock,&workSet);
  tempSet = workSet;
  */

  struct pollfd tempSet, dataSet;
  int time_out = 0;						// 0 milisec
  int data_time_out = 500;					// 0.1 sec
  dataSet.fd = workSock;
  dataSet.events = POLLIN;
  
  tempSet.fd = workSock;
  tempSet.events = POLLPRI;
  highDescSocket = 1;
  printf("PID: %d file '%s' %lld bytes start downloading port: %d\n", getpid(), filePath, fileSize, clientVect[index].port); 
  while(filePointer < fileSize)							
  {
    /*
    tempSet = workSet;						// set tmpSet; some OS chanche tmpSet after select
    time_out.tv_sec = 0; time_out.tv_usec = 0;			// set timeout; some OS chanche time_out after select
	
    while((retVal = select(highDescSocket+1,NULL,NULL,&tempSet,&time_out)) < 0)
    {								// wait for OOB data on workSock within time_out
      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
	      {
		fprintf(stderr, "select errno: %d\n", errno);	
								// errno == 11 means EAGAIN or EWOULDBLOCK == Try again						    
		return -1;    					// errno == 4 means EINTR == Interrupted system call
	      }
    }
    */
    //printf("poll OOB\n");    
    /*
    while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
    {
      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
      {
	fprintf(stderr, "PID: %d ppoll errno: %d\n", getpid(), errno);	
					    // errno == 11 means EAGAIN or EWOULDBLOCK == Try again
	return -1;    			// errno == 4 means EINTR == Interrupted system call
      }
    }
     printf("retVa OOB %d\n", retVal);
    if(retVal)							// if OOB data is received
    {
      //puts("recv OOB");					
      //OOB = bufOOBin = 0;					// recv sometimes return to perror "Resource temporarily unavailable"
      while(recvfrom(workSock, &bufOOBin, sizeof(bufOOBin), MSG_OOB | MSG_WAITALL,
	(struct sockaddr*)&clientAddrRecv, &clientAddrRecvLen) < 0)		
      {			    					// SOMETIMES SET ERRNO TO 11 OR 4
	  if((clientAddrRecv.sin_addr.s_addr == clientAddr.sin_addr.s_addr) && 
			    (clientAddrRecv.sin_port == clientAddr.sin_port))
	{
	  fprintf(stderr, "PID: %d recvFrom OOB errno: %d\n", getpid(), errno);
								  // errno == 4 means EINTR == Interrupted system call 
	  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)		
	    return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
	}
	else
	  break;
      }
      if((clientAddrRecv.sin_addr.s_addr == clientAddr.sin_addr.s_addr) && 
			  (clientAddrRecv.sin_port == clientAddr.sin_port))
      {
	printf("PID: %d OOB data received: %d\nPID: %d file '%s' %lld bytes for download left\n", 
	      getpid(), bufOOBin, getpid(), filePath, (fileSize - filePointer));
      }
    }
								// check if file exist (filename gets from client message)
	printf("clientFirstPacket %d\n", clientFirstPacket);
	*/
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
	  //	break;
	  retVal = 0;	  
	  while(retVal != 1)							// send num bytes already received (filePointer)
		{
		  //printf("send filePointer: %lld\n", filePointer);
		  while(sendto(workSock, (char*)&filePointer, sizeof(long long), MSG_WAITALL,  
				  (struct sockaddr*)&clientAddr, clientAddrLen) < sizeof(long long))
		  {													// errno == 4 means EINTR == Interrupted system call 
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		    {
			  fprintf(stderr, "PID: %d sendto filePointer: %lld errno: %d\n", getpid(), filePointer, errno);
										// errno == 4 means EINTR == Interrupted system call 
			  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		    }
		  }   
		  //printf("poll\n");
		  while((retVal = poll(&dataSet, highDescSocket, data_time_out)) < 0)
		  {
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			    {
			      fprintf(stderr, "PID: %d poll confirm filePointer errno: %d\n", getpid(), errno);	
								  // errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			      return -1;    			// errno == 4 means EINTR == Interrupted system call
			    }
		  }
		  //printf("retVal1: %d\n", retVal);
		  if(retVal)
		  {
		    while((readBytes = recvfrom(workSock,(char*)&recvMess, sizeof(int), MSG_PEEK, 
			    (struct sockaddr*)&clientAddrRecv, &clientAddrRecvLen)) < 0)
		    {	
		      if((clientAddrRecv.sin_addr.s_addr == clientVect[index].addr.s_addr) && 
				  (clientAddrRecv.sin_port == clientVect[index].port))
		      {
			//printf("PID: recv confirm recv filePointer errno: %d\n", getpid(), errno);
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
			{
			      fprintf(stderr, "PID: %d recvFrom confirm filePointer: %lld errno: %d\n", getpid(), filePointer, errno);
			      return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}
		      }
		    }		    
		    //printf("recvMess: %d\n", recvMess);
		    if((clientAddrRecv.sin_addr.s_addr == clientVect[index].addr.s_addr) && 
				  (clientAddrRecv.sin_port == clientVect[index].port))
		    {
		      count = otherPacket = 0;
			while((readBytes = recvfrom(workSock,(char*)&recvMess, sizeof(int), 0, 		// remove from queue
			    (struct sockaddr*)&clientAddrRecv, &clientAddrRecvLen)) < 0)
			{			 
			    //printf("PID: recv confirm recv filePointer errno: %d\n", getpid(), errno);
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
				  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}	
			
			if(readBytes != sizeof(int))				// datagram is corrupted
			{								// try again
			  retVal = 0;
			  readBytes = 0;		      
			}			 
			else
			{			  
			    if(recvMess != 9713)
			      retVal = 0;						// if filePointer not confirmed by client		
			}		  
		      }
		      else
			otherPacket++;
		  }
		  else
		    count++;		    
		  if(count == 60 || otherPacket == maxOtherPacket)
		    {
		      fprintf(stderr, "PID: %d timeout %lld milisec otherPacket: %lld\nconfirm filePointer: %lld errno: %d\n",
					      getpid(), (long long int)(count*data_time_out), otherPacket, filePointer, errno);
		      return -1;
		    }
		  
		}				
	  //puts("out init");
	  //printf("server_send num bytes: %d, mess: %lld\n",  readBytes, filePointer);
	  data_time_out = 10000;
	}
    else
    {
    //puts("server recv data");	

		recvMess = -1;
		    //printf("poll data\n");
		    while((retVal = poll(&dataSet, highDescSocket, data_time_out)) < 0)
		    {
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			      {
				fprintf(stderr, "PID: %d poll data errno: %d\n", getpid(), errno);	
								    // errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				return -1;    			// errno == 4 means EINTR == Interrupted system call
			      }
		    }
		    //printf("retVal data: %d\n", retVal);
		    if(retVal)
		    {
		      
		      while((readBytes = recvfrom(workSock, (char*)&buf, BUFFER_SIZE*sizeof(char), MSG_PEEK, 
							    (struct sockaddr*)&clientAddrRecv, &clientAddrRecvLen)) < 0)
		      {	
			if((clientAddrRecv.sin_addr.s_addr == clientVect[index].addr.s_addr) && 
				    (clientAddrRecv.sin_port == clientVect[index].port))
			{
			  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
			  {
				fprintf(stderr, "PID: %d recvfrom data + MSG_PEEK errno: %d\n", getpid(), errno);	
				return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
			}
		      }
		      //printf("rercBytes data: %d\n", readBytes);
			if((clientAddrRecv.sin_addr.s_addr == clientVect[index].addr.s_addr) && 
				    (clientAddrRecv.sin_port == clientVect[index].port))	// check ip and port
			{
			    count = otherPacket = 0;			    
			    if(filePointer + readBytes <= fileSize)
				recvFlag = readBytes;
			    else
			      recvFlag = fileSize - filePointer;
			    while((readBytes = recvfrom(workSock, (char*)&buf, recvFlag, MSG_WAITALL, 		// remove from queue
							      (struct sockaddr*)&clientAddrRecv, &clientAddrRecvLen)) < recvFlag)
			    {
				if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
				{
				      fprintf(stderr, "PID: %d recvfrom data + REMOVE_QUEUE errno: %d\n", getpid(), errno);
				      return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
				}			      
			    }
			    if(readBytes > recvFlag)
			    {
			      fprintf(stderr, "PID: %d recvfrom data + REMOVE_QUEUE readBytes > recvFlag errno: %d\n", getpid(), errno);
				      return -1;
			    }	
			    //if(recvFlag == 0 && filePointer == fileSize)
			     // printf("readBytes: %d\n", readBytes);
			  recvMess = readBytes;				// data received successfull
			  //printf("readBytes: %d", readBytes);
			  while(sendto(workSock, (char*)&recvMess, sizeof(int), MSG_WAITALL,   	
					(struct sockaddr*)&clientAddr, clientAddrLen) < sizeof(int)) 
			  {						// send confirm recv filePointer to server		// errno == 4 means EINTR == Interrupted system call 
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			    {
				  fprintf(stderr, "PID: %d send confirm data: %d errno: %d\n", getpid(), recvMess, errno);
				  return -1;				// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			    }
			  }
		      if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		  {				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			fprintf(stderr, "PID: %d nanosleep errno: %d\n", getpid(), errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		      }
		  }
			   //printf("send confirm data: %d\n", recvMess);
			}
			else
			  otherPacket++;
		    }
		    else
		      count++;
		    if(count == 3 || otherPacket == maxOtherPacket)
		    {
			fprintf(stderr, 
	"PID: %d timeout %lld milisec otherPacket: %lld\nrecvFrom data filePointer: %lld errno: %d readBytes: %d, port: %d\n",
 	getpid(), (long long int)(count*data_time_out), otherPacket, filePointer, errno, readBytes, clientAddrRecv.sin_port);
			return -1;
		    }
		      
		  if(recvMess > 0)
		  {
		    
		    //printf("server_readbytes %d\n", readBytes);	
		    //printf("server_fwrite filePointer %lld\n",  filePointer);
		    fwrite((char*)buf, readBytes, 1, file);			// write data to file
		    filePointer += readBytes;
		    fseek(file, filePointer, SEEK_SET);
		    //printf("write data filePointer: %lld\n", filePointer);
		    //printf("server_fwrite filePointer %lld, recvMess: %d\n",  filePointer, recvMess);
		  }
    }           
      //printf("filePointer: %lld, fileSize: %lld, readBytes: %d\n", filePointer, fileSize, readBytes);
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
  ind--;		    
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
	if(!strcmp(argv[1], "tcp"))						// tcp CTRL+C signal
	  closeTerm.sa_sigaction =&hdl_SIGINT_TCP;
	else
	  closeTerm.sa_sigaction =&hdl_SIGINT_UDP;				// udp CTRL+C signal
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
	if(!strcmp(argv[1], "tcp") || parentPid == getpid())
	{
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
	}
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);
	//fprintf(stdout, "PID: %d exit\n", getpid());	
    	return 0;
}
