/* !NEED NCURSES LIB! SEE readme.txt!
 * [how to compile]:
 * gcc CLIENT.c -lncurces
 * 
 * many clients (limit MAX_PENDING) send files
 * server receive and save local
 * if some clients send file with the same fileName
 * then server drop last client connection with same fileName
 * and continue to download from first client with same fileName
 * pressing 'SPACE' key on client side == send OOB data '5' to server
 * CTRL+C == default force terminate client
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
#include <ncurses.h>								// async/non_blocking input


#define	BUFFER_SIZE  		1024						// for incomming
#define ARG_ERROR_MESS		"\n[protocol] [ip] [port] [filename or full filepath]\n\n[protocol] need to be 'tcp'/'udp'\n"
#define	MAX_FILEPATH_LENGHT	64						// in bytes

int 		listenSock;							// socket descriptors
int 		ind = 0;							// indicator that any socket in open
long long	fileSize = 2;
long long 	filePointer = 0;
FILE*		file;
int 		OOB = 0;							// indicator for OOB (see signal handles)
uint8_t 	bufOOB = 5;							// OOB byte that is send to server
int 		tcp1 = 0;


void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)			// handler for SIGINT (Ctrl+C) signal
{
    if (sig==SIGINT)
    {	 
      if(tcp1)									// if use tcp
	endwin();								// end work with ncurses lib
	if(ind)
	{
	  if(tcp1)								// if use tcp
	  {
	    if(shutdown(listenSock, SHUT_RDWR) < 0)				// deny connection
	      perror("func shutdown listenSock signal");
	  }
	  if(close(listenSock) < 0)			
		  perror("sgn close listenSock signal");
	  ind--;
	}	
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);	
	exit(0);
    }
}
/*
void hdl_SIGTSTP(int sig, siginfo_t *siginfo, void *context)			// handler for SIGTSTP (Ctrl+Z) signal (client)
{
  if(sig==SIGTSTP)
    OOB = 1;
}
*/

int startClientTcp(char *hostName, char *port, char *argFilePath)
{
  
	struct 		sockaddr_in hostAddr;    				// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for outcomming
	int 		readBytes;						// count of	
	int 		sendBytes;
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	uint8_t 	bufOOBin;	
	int 		highDescSocket;
    	char		filePath[MAX_FILEPATH_LENGHT];
	strcpy(filePath, argFilePath);
	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order
	hostAddr.sin_addr.s_addr = inet_addr(hostName);				// old func convert IPv4 char* -> IPv4 bin 
										// (+ host byte order -> network byte order too) 
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
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		if(((listenSock) = socket(AF_INET, SOCK_STREAM, 0)) < 0)		
		{
			perror("func socket");
			return -1;
		}  
		ind++;
		//hostAddr.sin_addr.s_addr = inet_addr(hostName);		// old func convert IPv4 char* -> IPv4 bin 
										// (+ host byte order -> network byte order too) 
		setsockopt(listenSock, SOL_SOCKET, 
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
		  printw("func send filePath");
		  printw("errno: %d\n", errno);					// errno == 4 means EINTR == Interrupted system call 
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		  {
			perror("func send filePath");
			printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		  }
		}   								
				
		//FD_ZERO (&temp);
		//FD_SET (listenSock,&temp);
		
		
		//printf("client_filePointer: %lld\n", filePointer);						
		
		//printf("client_send_fileSize %lld\n", fileSize);
		while(send(listenSock, (char*)&fileSize, sizeof(long long), 0) < sizeof(long long))
		{								// send fileSize
		  printw("func send fileSize");
		  printw("errno: %d\n", errno);
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
		  {
			perror("func send fileSize");
			printf("errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		  }
		} 								
		
		//puts("client_recv_filePointer");
										// recv filePointer from server
		while(recv(listenSock,(char*)&filePointer, sizeof(long long), MSG_WAITALL) < sizeof(long long))
		{									      			  
		  printw("func recv filePointer");
		  printw("errno: %d\n", errno);
		  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	// errno == 4 means EINTR == Interrupted system call 
		  {
			perror("func recv filePointer");
			printf("errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		  }
		}		
		
		//printf("client_recvFilePointer_fromServer %lld\n", filePointer);
		fseek(file, filePointer, SEEK_SET);				// go to addr in file, according filePointer	
		initscr();							// init ncurses lib
		timeout(0);							// set timeout for ncurces input		
		do
		{
		  readBytes = fread(&buf, sizeof(char), BUFFER_SIZE, file);
		  OOB = getch(); 
		  if(OOB == ' ')						// if space is pressed
		  {
		    OOB = 0;							// send OOB data
		    while(send(listenSock, &bufOOB, sizeof(bufOOB), MSG_OOB) < 0)
		    {		      
		      printw("func send SIGTSTP");
		      printw("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			perror("func send SIGTSTP");
			printf("errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		      }
		      
		    }	
		    clear();
		    printw("'SPACE' key is pressed. OOB data sended: %d\nfile '%s' %lld bytes for send left\n", 
			   bufOOB, filePath, (fileSize - filePointer));
		    if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		    {
			printw("nano sleep system call failed");
			printw("errno: %d\n", errno);
										// errno == 4 means EINTR == Interrupted system call 
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			{
			  perror("nano sleep system call failed");
			  printf("errno: %d\n", errno);
			  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}
		    }
		  }		  
		  //printf("client_send_fread %d\n", readBytes);
		  /*if(select(0,NULL,&temp,NULL,&time_out))
		  {
		    	  bufOOBin = 0;int deleteln()
			  if(recv(workSock, &bufOOBin, sizeof(bufOOBin), MSG_OOB | MSG_WAITALL) < 0)	
			  {							// recv sometimes return to 
			    printf("errno %d\n", errno);			// perror "Resource temporarily unavailable"
			    perror("func recv select");			    
			  }
			  printf("OOB data received: %d\n", bufOOBin);
		  }*/ 		
		  //printf("client_select filePointer %lld\n", filePointer);	// send data to server
		  while((sendBytes = send(listenSock, (char*)&buf, readBytes, 0)) < 0)
		  {
		    printw("func send data");
		    printw("errno: %d\n", errno);
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		    {
		      perror("func send data");
		      printf("errno: %d\n", errno);
		      return -1;
		    }
		  } 
		  filePointer += sendBytes;
		  if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		  {
		      printw("nano sleep system call failed");
		      printw("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			perror("nano sleep system call failed");
			printf("errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		      }
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

int startClientUdp(char *hostName, char *port, char *argFilePath)
{
	struct 		sockaddr_in hostAddr, hostAddrRecv;			// this machine 
	char 		buf[BUFFER_SIZE];					// buffer for outcomming
	int 		readBytes;						// count of	
	int 		sendBytes;
	int 		so_reuseaddr = 1;					// for setsockop SO_REUSEADDR set enable
	uint8_t 	bufOOBin;	
	int 		highDescSocket;
	int 		recvBytes = 0;
    	char		filePath[MAX_FILEPATH_LENGHT];
	int		hostAddrLen = sizeof(hostAddr);
	int		hostAddrRecvLen = sizeof(hostAddrRecv);
	int 		recvMess = 0;
	int 		retVal = 0;
	int 		count = 0;
	int		clientFirstPacket = 1;
	hostAddrRecv.sin_family = AF_INET;
	hostAddrRecv.sin_port = 0;
	hostAddrRecv.sin_addr.s_addr = 0;	
	strcpy(filePath, argFilePath);
	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));					// convert host byte order -> network byte order
	hostAddr.sin_addr.s_addr = inet_addr(hostName);				// old func convert IPv4 char* -> IPv4 bin 
										// (+ host byte order -> network byte order too) 
	filePath[strlen(filePath)] = '\0';
	file = fopen(filePath, "rb");						// open file for read
	fseek(file, 0L, SEEK_END);						
	fileSize = ftell(file);							// get file size
	fseek(file, 0L, SEEK_SET);						
	//fd_set temp;		
	//struct timeval time_out; time_out.tv_sec = 0; time_out.tv_usec = 0;
	struct timespec tim, tim2;
	tim.tv_sec = 0;
	tim.tv_nsec = 8000000L;							// sleep time in nanosec
	
	struct pollfd tempSet;
	int time_out = 30000;							// 30000 milisec = 30 sec	
	tempSet.events = POLLIN;
	highDescSocket = 1;
	while(1)								// if any key is pressed -> exit from while loop
    	{		
		if(((listenSock) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)		
		{
			perror("socket");
			return -1;
		}  
		ind++;		
		fcntl(listenSock, F_SETFL, O_NONBLOCK);				// set socket to NON_BLOCKING
		//hostAddr.sin_addr.s_addr = inet_addr(hostName);		// old func convert IPv4 char* -> IPv4 bin 
										// (+ host byte order -> network byte order too) 
		setsockopt(listenSock, SOL_SOCKET, 
						SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
										// reuse ADDR when socket in TIME_WAIT condition

		
		
		tempSet.fd = listenSock;				  
		//puts("client_connect");
		/*
		if(connect(listenSock, (struct sockaddr*) &hostAddr, sizeof(hostAddr)) < 0)
		{
		  perror("func connect");
		  return -1;		
		}
		*/
		//printf("client_send_filePath1 %s\n", filePath);
		while(retVal != 1)
		{
		  //printf("client_send_filePath2 %s\n", filePath);
										// send filePath to server
		  while(sendto(listenSock, (char*)&filePath, MAX_FILEPATH_LENGHT*sizeof(char), 0, 
				  (struct sockaddr*)&hostAddr, hostAddrLen) < MAX_FILEPATH_LENGHT*sizeof(char))
		  {								// send filePath					
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		    {
			  perror("sendto filePath\n");
			  printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
			  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		    }
		  }   
		  //printf("OK, pool wait\n");
										// wait for confirmMess from server
		  while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
		  {
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			    {
			      fprintf(stderr, "poll send filePath errno: %d\n", errno);	
										// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			      return -1;    					// errno == 4 means EINTR == Interrupted system call
			    }
		  }
		  //printf("pool retVal: %d\n", retVal);
		  if(retVal)
		  {
		    count = 0;
		    //printf("recvfrom\n");
										// recv confirmMess from server
		    while((readBytes = recvfrom(listenSock,(char*)&recvMess, sizeof(int), 0, 
			    (struct sockaddr*)&hostAddrRecv, &hostAddrRecvLen)) < 0)
		    {	
		      if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				  (hostAddrRecv.sin_port == hostAddr.sin_port))
		      {
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			{
			      perror("recvFrom confirm filePath\n");		// errno == 4 means EINTR == Interrupted system call 
			      printf("errno: %d\n", errno);
			      return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}
		      }
		    }
		    //printf("recv readBytes: %d\n", readBytes);
		    if(readBytes != sizeof(int))				// datagram is corrupted
		    {								// try again
		      retVal = 0;
		      readBytes = 1;
		      break;
		    }
		    else
		    {
		      //printf("readBytes == sizeof(int)\n");
		      if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				  (hostAddrRecv.sin_port == hostAddr.sin_port))
		      {
			//printf("recvMess: %d\n", recvMess);
			switch(recvMess)
			{
			  case 8102:						// filePath confirmed by server
			  {
			    retVal = 1;
			    break;
			  }
			  case 5510:						// file with the same filePath, file
			  {							// already download from other client
			    return -1;
			  }
			  default:						// try again
			  {
			    retVal = 0;
			    break;
			  }
			}
		      }
		      else							// datagram received from another ip:port
			retVal = 0;
		    }
		  }	
		  else
		  {		    
		    count++;
		    if(count == 2)						// timeout wait for confirmMess from server
		    {
		      printf("send filePath '%s', no confirm from server %lld milisec\n", filePath, (long long int)(count*time_out));
		      return -1;
		    }
		  }
		}
		//printf("server_confirm_filePath retVal %d\n", retVal);
		retVal = 0;		
		recvMess = 0;								
		while(retVal != 1)						
		{
		  //printf("sendto\n");
										// send fileSize to server
		  while(sendto(listenSock, (char*)&fileSize, sizeof(long long), 0,  
				  (struct sockaddr*)&hostAddr, hostAddrLen) < sizeof(long long))
		  {								// send fileSize
		    //printf("func send fileSize");
		    //printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		    {
			  perror("sendto fileSize\n");
			  printf("errno: %d\n", errno);				// errno == 4 means EINTR == Interrupted system call 
			  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		    }
		  }   
		  //printf("poll\n");
										// wait for confirmMess from server
		  while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
		  {
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			    {
			      fprintf(stderr, "poll sendto fileSize errno: %d\n", errno);	
										// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			      return -1;    					// errno == 4 means EINTR == Interrupted system call
			    }
		  }
		  //printf("retVal: %d\n", retVal);
		  if(retVal)
		  {
		    count = 0;
		    //printf("recvFrom\n");
										// recv confirmMess from server
		    while((readBytes = recvfrom(listenSock,(char*)&recvMess, sizeof(int), 0, 
			    (struct sockaddr*)&hostAddrRecv, &hostAddrRecvLen)) < 0)
		    {	
		      if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				  (hostAddrRecv.sin_port == hostAddr.sin_port))
		      {
			//printf("func recv filePointer");
										//printf("errno: %d\n", errno);
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			{
			      perror("recvFrom confirm fileSize\n");		// errno == 4 means EINTR == Interrupted system call 
			      printf("errno: %d\n", errno);
			      return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}
		      }
		    }		    
		    if(readBytes != sizeof(int))				// datagram is corrupted
		    {								// try again
		      retVal = 0;
		      readBytes = 0;		      
		    }		    	
		    else
		    {
		      //printf("recvMess1: %d\n", recvMess); 
		      if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				  (hostAddrRecv.sin_port == hostAddr.sin_port))	// check ip and port
		      {
			if(recvMess == 1012)
			  retVal = 1;						// fileSize confirmed by server
			else
			  retVal = 0;						// try again			
		      }
		      else							// datagram received from another ip:port
			retVal = 0;
		      }
		  }
		  else
		  {		    
		    count++;
		    if(count == 2)						// timeout wait for confirmMess from server
		    {
		      printf("send fileSize '%lld', no confirm from server %lld milisec\n", fileSize, (long long int)(count*time_out));
		      return -1;
		    }
		  }
		}
		retVal = 0;
		readBytes = 0;
		//printf("client_filePointer: %lld\n", filePointer);						
		
		//printf("client_send_fileSize %lld\n", fileSize);
		
		//puts("client_recv_filePointer");
		time_out = 60 * 1000;						// wait for confirm download
										// on server side within 1 min
		while(retVal != 1)						// recv filePointer from server
		{
		  
		  //printf("poll\n"); 
										// wait for filePointer from server
		  while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
		  {
		    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			    {
			      fprintf(stderr, "poll recvFrom filePointer errno: %d\n", errno);	
										// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			      return -1;    					// errno == 4 means EINTR == Interrupted system call
			    }
		  }
		  //printf("retVal: %d\n", retVal); 
		  if(retVal)
		  {
		    count = 0;
										// recv filePointer from server
		    while((readBytes = recvfrom(listenSock,(char*)&filePointer, sizeof(long long), 0, 
			    (struct sockaddr*)&hostAddrRecv, &hostAddrRecvLen)) < 0)
		    {	
		      if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				  (hostAddrRecv.sin_port == hostAddr.sin_port))
		      {
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			{
			      perror("recvFrom filePointer\n");
			      printf("errno: %d\n", errno);			// errno == 4 means EINTR == Interrupted system call 
			      return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}
		      }
		    }
		    //printf("recv filePointer: %lld\n", filePointer);
		    if(readBytes != sizeof(long long))				// datagram is corrupted
		    {								// try again
		      retVal = 0;
		      readBytes = 0;
		    }
		    else
		    {
		      if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				  (hostAddrRecv.sin_port == hostAddr.sin_port))	// check ip and port
		      {
			recvMess = 9713;
			while(sendto(listenSock, (char*)&recvMess, sizeof(int), 0,  
				      (struct sockaddr*)&hostAddr, hostAddrLen) < sizeof(int))
			{							// send confirm recv filePointer to server
			  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			  {
				perror("sendto confirm filePointer\n");
				printf("errno: %d\n", errno);			// errno == 4 means EINTR == Interrupted system call 
				return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
			} 
			retVal = 1;						// filePointer received successfull			
		      }
		      else							// datagram received from another ip:port
			retVal = 0;
		    }		      
		  }
		  else
		  {		    
		    
		      printf("recv filePointer '%s', no confirm from server %d milisec\n", filePath, time_out);
		      return -1;
		  }
		}
		readBytes = 0;
		time_out = 10000;
		//printf("client_recvFilePointer_fromServer %lld\n", filePointer);			
		do 								// loop - send file data 
		{
		  if(retVal)
		  {
		    fseek(file, filePointer, SEEK_SET);				// go to addr in file, according filePointer
		    readBytes = fread(&buf, sizeof(char), BUFFER_SIZE, file);
		    //printf("readBytes file: %d\n", readBytes);
		  }
		  //OOB = getch(); 
		  /*
		  if(OOB == ' ')						// if space is pressed
		  {
		    OOB = 0;							// send OOB data
		    while(sendto(listenSock, &bufOOB, sizeof(bufOOB), MSG_OOB,
					  (struct sockaddr*)&hostAddr, hostAddrLen) < 0)
		    {		      				// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			perror("sendto OOB\n");
			printf("errno: %d\n", errno);
			return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		      }
		      
		    }	
		    //clear();
		    printf("'SPACE' key is pressed. OOB data sended: %d\nfile '%s' %lld bytes for send left\n", 
			   bufOOB, filePath, (fileSize - filePointer));
		    if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		    {							// errno == 4 means EINTR == Interrupted system call 
			if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			{
			  perror("nanosleep\n");
			  printf("errno: %d\n", errno);
			  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			}
		    }
		  }		  
		  //printf("client_send_fread %d\n", readBytes);
		  */
		//retVal = 0;
		//recvMess = 0;
		  if(readBytes)
		  {
		    while((sendBytes = sendto(listenSock, (char*)&buf, readBytes, MSG_WAITALL, 
				    (struct sockaddr*)&hostAddr, hostAddrLen)) < readBytes)
		    {								// send readBytes
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			    perror("sendto data\n");
			    printf("errno: %d\n", errno);			// errno == 4 means EINTR == Interrupted system call 
			    return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		      }
		    } 		  
		    //printf("filePointer: %lld sendBytes: %d, readBytes: %d\n", filePointer, sendBytes, readBytes);
		    if(nanosleep(&tim , &tim2) < 0)   				// sleep in nanosec
		      {							// errno == 4 means EINTR == Interrupted system call 
			  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			  {
			    perror("nanosleep\n");
			    printf("errno: %d\n", errno);
			    return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
		      }
		    //printf("poll data\n");
										// wait for recvBytes from server == count of bytes 
										// that is received by server
		    while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
		    {	
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			      {
				fprintf(stderr, "poll data errno: %d\n", errno);	
										// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				return -1;    					// errno == 4 means EINTR == Interrupted system call
			      }
		    }
		    //printf("retVal data %d\n", retVal);
		    if(retVal)
		    {		    
										// recv recvBytes from server
		      while((recvBytes = recvfrom(listenSock,(char*)&recvMess, sizeof(int), MSG_WAITALL, 
			      (struct sockaddr*)&hostAddrRecv, &hostAddrRecvLen)) < (sizeof(int)))
		      {	
			if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				    (hostAddrRecv.sin_port == hostAddr.sin_port))
			{
			  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			  {
				perror("recvFrom confirm data\n");		// errno == 4 means EINTR == Interrupted system call 
				printf("errno: %d\n", errno);
				return -1;					// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
			  }
			}
		      }
		      //printf("recvMess data %d, readBytes: %d\n", recvMess, readBytes);
		      if(recvBytes != sizeof(int))				// datagram is corrupted
			retVal = 0;						// try again
		      else
		      {
			if((hostAddrRecv.sin_addr.s_addr == hostAddr.sin_addr.s_addr) && 
				    (hostAddrRecv.sin_port == hostAddr.sin_port))	// check ip and port
			{
			    retVal = 1;						// data confirmed by server	  
			  if(recvMess == 6666)
			  {
			    puts("get 6666");					// FOR DEBUG!!! SERVER DONT WORK
			    return -1;						// WITH MANY CLIENTS!!! WAAA!!
			  }								
			}
			else							// datagram received from another ip:port
			  retVal = 0;
			}
		    }
		    if(retVal)
		    {
			//printf("filePointer: %lld recvMess: %d\n", filePointer, recvMess);
			filePointer += recvMess;	
			count = 0;
		    }
		    else
		    {
		      count++;
		      if(count == 6)						// timeout wait for recvBytes from server
		      {
			printf("send data filePointer: %lld, no confirm from server %lld milisec\n",
			       filePointer, (long long int)(count*time_out));
			return -1;
		      }
		    }
		  }
		  //printf("client_select filePointer %lld\n", filePointer);	// send data to server
		  //puts("client_send_fieFragment");
		//printf("filePointer: %lld, fileSize: %lld, readBytes: %d\n", filePointer, fileSize, readBytes);
		}while(readBytes);
		//puts("client_close_socket");
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
	/*
	sendOOB.sa_sigaction =&hdl_SIGTSTP;
	sendOOB.sa_flags = SA_SIGINFO;
	if(sigaction(SIGTSTP, &sendOOB, NULL) < 0)				// set handler for SIGTSTP signal (CTRL+Z) (client)
	{
	  perror("main sigaction sendOOB");
	  return -1;
	}
	*/
	if(!strcmp(argv[1], "tcp"))
	{	 
	  tcp1 = 1;
	  startClientTcp(argv[2], argv[3], argv[4]);				// tcp
	}
	else
	{
	  startClientUdp(argv[2], argv[3], argv[4]);				// udp
	}
	if(tcp1)
	  endwin();								// end work with ncurces
	if(ind)
	{
	  if(tcp1)								// if tcp
	  {		
		if(shutdown(listenSock, SHUT_RDWR) < 0)				// deny connection
		{
			perror("func shutdown listenSock main");
			return -1;
		}
	  }
		if(close(listenSock) < 0)		
		{
			perror("main close listenSock main");	
			return -1;
		}
		ind--;
	}
	if(ftell(file) >= 0)							// check is file open
		  fclose(file);								
    	return 0;
}
