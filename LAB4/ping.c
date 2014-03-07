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
#define ARG_ERROR_MESS		"\n[ip] [pid]\n"
#define	MAX_FILEPATH_LENGHT	64				// in bytes


char		myIpv4[16];					// src Ip
pid_t		myPid;   					// process id
int		myPort;						// port

char		dstIpv4[16];					// dst Ip	

int startServerUdp(char *hostName, char *port)
{
	
	struct 		sockaddr_in myHostAddr;	
	int 		sendSock;
	long 		recvBuf = MAX_PENDING*(BUFFER_SIZE*sizeof(char)*6000);
	long 		sendBuf = MAX_PENDING*(BUFFER_SIZE*sizeof(char)*6000);	
	int 		sOptionOn = 1;			
    	
    	int 		otherClient = 0, oldClient = 0;		// if other client already send this file
    	char		filePath[MAX_FILEPATH_LENGHT];
	char 		buf[BUFFER_SIZE];				// buffer for incomming
	int 		i;					// for loops
	long 		recvBuf = MAX_PENDING*(BUFFER_SIZE*sizeof(char)*6000);
	int 		highDescSocket, retVal = 0;		// for select/poll
	int 		confirmMess = 0;
	int		readBytes = 0;
	
	
	
	
	myPid = getpid();
	
	hostAddr.sin_family = AF_INET;
    	hostAddr.sin_port = htons(atoi(port));			// convert host byte order -> network byte order

    	hostAddr.sin_addr.s_addr = inet_addr(hostName);		// old func convert IPv4 char* -> IPv4 bin
								// (+ host byte order -> network byte order too) 
	if(((sendSock) = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)		
   	{
        	fprintf(stderr, "ICMP socket errno: %d\n", errno);
        	return -1;
    	}    
	ind++;
	setsockopt(workSock, SOL_SOCKET, 
						SO_REUSEADDR, &soOptionOn, sizeof so_reuseaddr);
	setsockopt(workSock, SOL_SOCKET, 
						SO_RCVBUF, &soOptionOn, sizeof recvBuf);
	setsockopt(workSock, SOL_SOCKET, 
						SO_SNDBUF, &soOptionOn, sizeof recvBuf);
	
    	if(bind((workSock), (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0)
								// socket is associated with IPv4 address
    	{
        	fprintf(stderr, "UDP bind errno: %d\n", errno);
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
	   if(nClients)						// if any child proc exists
		{						// -1 == wait for all child proc 
		  switch((childPid = waitpid(-1, &status, WNOHANG))) 
		  {						// WNOHANG == return control immediately
		    case -1:					// error waitpid
		    {
		      perror("UDP waitpid");
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
		      tim.tv_nsec = 800000L;			// sleep time in nanosec
		      if(nanosleep(&tim , &tim2) < 0)   	
		  {
		      printf("nano sleep system call failed");
		      printf("errno: %d\n", errno);		// errno == 4 means EINTR == Interrupted system call 
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			perror("nano sleep system call failed");
			printf("errno: %d\n", errno);
			return -1;				// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
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
		  fprintf(stderr, "UDP poll errno: %d\n", errno);	
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
		    {						// errno == 4 means EINTR == Interrupted system call 
		      fprintf(stderr, "UDP recvFrom filePath OK3 errno: %d\n", errno);
		      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		      {
			fprintf(stderr, "UDP recvFrom filePath OK3 bad3 errno: %d\n", errno);
			return -1;				// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		      }
		    }			    
		    for(i = 0; i < MAX_PENDING; i++)		// check if client already establish connection
		    {
		      //printf("%d\n", i);
		      if(clientVect[i].status)
		      {		
			//printf("status %d\n", i);		// check port && ip
			if((clientVect[i].addr.s_addr == clientAddr.sin_addr.s_addr) &&	
						      (clientVect[i].port == clientAddr.sin_port))			
			{					// if client already establish connection and			  
			  //printf("ip %d\n", i);
			  switch(clientVect[i].pid)
			  {
			    case -1:				// recv fileSize
			    {	
			      //puts("fileSize");
			      if(readBytes == sizeof(long long))
			      {
								// remove from queue
				while((readBytes = recvfrom(workSock, (char*)&fileSize, sizeof(long long), MSG_WAITALL, 	
						(struct sockaddr*)&clientAddr, &clientAddrLen)) < sizeof(long long))
				    // receive data from client
				{				// errno == 4 means EINTR == Interrupted system call 
				  fprintf(stderr, "UDP recvFrom filePath OK2 errno: %d\n", errno);
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				  {
				    fprintf(stderr, "UDP recvFrom fileSize OK errno: %d\n", errno);
				    return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				  }
				}	
				//puts("fileSize1");
				//printf("recv '%s' fileSize: %lld\n", clientVect[i].filePath, fileSize);
				switch((childPid = fork()))
				{
					case -1: 		// error fork
						{
							perror("UDP fork");
							return -1;				
						}	
					case 0 : 		// child process
						{			
							status = serverProcessingUdp(workSock, hostAddr, fileSize, clientAddr, i);	  
							return status;	
						}			
					default :		// parent process
						{		// add child pid 
							clientVect[i].pid = childPid;			   
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
								// remove from queue
				while((recvfrom(workSock, (char*)&fileSize, readBytes, MSG_WAITALL, 	
						(struct sockaddr*)&clientAddr, &clientAddrLen)) < readBytes)
								// receive data from client
				{				// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				  {
				    fprintf(stderr, "UDP recvFrom fileSize NOT_OK bad0 errno: %d\n", errno);
				    return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
				  }
				}
				
				//puts("fileSize BAD");
				confirmMess = 3333;		// request fileSize
				oldClient = 1;
				//puts("old not 1");
			      }
			      break;
			    }
			    default:				// recv data
			    {
			       if(nanosleep(&tim , &tim2) < 0)  // sleep in nanosec
			      {					// errno == 4 means EINTR == Interrupted system call 
				  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
				  {
				    fprintf(stderr, "UDP nanosleep errno: %d\n", errno);
				    return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
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
		    if(!oldClient)				// if filePath recveived
		    {		
		      //printf("filePath, readBytes: %d\n", readBytes);
		       if(readBytes == MAX_FILEPATH_LENGHT*sizeof(char))
		       {					// remove from queue
			  while((readBytes = recvfrom(workSock, (char*)&filePath, MAX_FILEPATH_LENGHT*sizeof(char), MSG_WAITALL, 	
				    (struct sockaddr*)&clientAddr, &clientAddrLen)) < MAX_FILEPATH_LENGHT*sizeof(char))
			      // receive data from client
			  {					// errno == 4 means EINTR == Interrupted system call 
			    fprintf(stderr, "UDP recvFrom filePath OK1 errno: %d\n", errno);
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			    {
			      fprintf(stderr, "UDP recvFrom filePath OK1 bad1 errno: %d\n", errno);
			      return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			    }
			  }			  
			  //printf("filePath recv: %s\n", filePath);
			  for(i = 0; i < MAX_PENDING; i++)	// search if this filePath already downloading
			  {					// from other client
			    if(clientVect[i].status > 1)
			    {
			      if(!strcmp(clientVect[i].filePath, filePath))
			      {
				confirmMess = 5510;		// recv filePath already exist
				otherClient = 1;		// and recv from other client
				break;
			      }	
			    }
			  }
			  if(!otherClient)
			  {
			    for(i = 0; i < MAX_PENDING; i++)	// search for free element in clientVect[MAX_PENDING]
			    {							
			      if(clientVect[i].status == 0)
			      {			    
				strcpy((clientVect[i].filePath), filePath);	// add filePath to clientVect
				clientVect[i].addr = clientAddr.sin_addr;	// add client's sin_addr
				clientVect[i].port = clientAddr.sin_port;	// add client's port
				confirmMess = 8102;
				clientVect[i].status++;				// update status == wait for fileSize
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

			 //puts("filePath BAD1");		// remove from queue
			 while((recvfrom(workSock, (char*)&filePath, readBytes, MSG_WAITALL, 	
				    (struct sockaddr*)&clientAddr, &clientAddrLen)) < 0)
			      // receive data from client
			  {					// errno == 4 means EINTR == Interrupted system call 
			    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
			    {
			      fprintf(stderr, "UDP recvFrom filePath NOT_OK errno: %d\n", errno);
			      return -1;			// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
			    }
			  }
			  //puts("filePath BAD2");

			 confirmMess = 6666;			// request filePath
			 //printf("6666 readBytes: %d\n", readBytes);			 
			 //oldClient = 2;
			  
		       }
		       
		    }
		    if(oldClient < 2)
		    {
			//printf("send confirmMess1: %d\n", confirmMess);	
								// send confirmMess
		     while(sendto(workSock, (char*)&confirmMess, sizeof(int), MSG_WAITALL, 
			(struct sockaddr*)&clientAddr, clientAddrLen) < sizeof(int))
		      {			
			fprintf(stderr, "UDP sendto confirmMess: %d errno: %d\n", confirmMess, errno);
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


int main(int argc, char *argv[])
{
	switch(argc)							// check command-line arguments
	{
	  case 2:							// only dst ip specified
	  {
	    myPid = getPid();
	    strcpy(dstIpv4, argv[1]);
	    dstIpv4[15] = '\0';
	    break;
	  }
	  case 3:							// dst and src ip specified
	  {
	    myPid = getPid();
	    strcpy(dstIpv4, argv[1]);
	    dstIpv4[15] = '\0';
	    break;
	  }
	  case 4: 							// dst and src ip + src pid specified					
	  {
	    
	    break;
	  }
	  default:							// wrong command-line arguments
	  {
	    puts(ARG_ERROR_MESS);
	    perror("main invalid command-line arguments");
	    return -1;
	  }
	}
	
	struct sigaction closeTerm;
	closeTerm.sa_flags = SA_SIGINFO;	
	switch(fork())
	{
	case -1:
	{
	  perror("main fork error");
	  return -1;
	}
	case 0:								// child proc == TCP server
	{
	  a = 1;
	  closeTerm.sa_sigaction =&hdl_SIGINT_TCP;
	  if(sigaction(SIGINT, &closeTerm, NULL) < 0)			// set handler for SIGINT signal (CTRL+C)
	  {
		  perror("TCP main sigaction closeTerm");
		  return -1;
	  }
	  startServerTcp(argv[1], argv[2]);
	  break;
	}
	default:							// parent proc == UDP server
	{
	  a = 0;
	  closeTerm.sa_sigaction =&hdl_SIGINT_UDP;
	  if(sigaction(SIGINT, &closeTerm, NULL) < 0)			// set handler for SIGINT signal (CTRL+C)
	  {
		  perror("UDP main sigaction closeTerm");
		  return -1;
	  }
	  startServerUdp(argv[1], argv[2]);
	  break;
	}
	}	
	if(a || parentPid == getpid())
	{
	  if(ind > 1)
	  {
		  if(shutdown(listenSock, SHUT_RDWR) < 0)		// deny connection
		  {
			  fprintf(stderr, "TCP PID: %d shutdown listenSock main errno: %d\n", getpid(), errno);
			  return -1;
		  }
		  if(close(listenSock) < 0)		
		  {
			  fprintf(stderr, "TCP PID: %d close listenSock main errno: %d\n", getpid(), errno);	
			  return -1;
		  }
		  ind-=2;
	  }
	  if(ind)
	  {
		  if(a)
		  {
		    if(shutdown(workSock, SHUT_RDWR) < 0)		// deny connection
		    {
			    fprintf(stderr, "TCP PID: %d shutdown workSock main errno: %d\n", getpid(), errno);
			    return -1;
		    }
		  }
		  if(close(workSock) < 0)		
		  {
			  if(a)
			    fprintf(stderr, "UDP PID: %d close workSock main errno: %d\n", getpid(), errno);
			  else
			    fprintf(stderr, "TCP PID: %d close workSock main errno: %d\n", getpid(), errno);
			  return -1;					
		  }
	  }
	}
	if(ftell(file) >= 0)						// check is file open
		  fclose(file);
	//fprintf(stdout, "PID: %d exit\n", getpid());	
    	return 0;
}g