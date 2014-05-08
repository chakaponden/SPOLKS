#include <sys/ioctl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>

#define	BUFFER_SIZE  		1024
#define	LOCAL_PORT 		"49999"
#define ARG_ERROR_MESS		"./a.out [interface] [dstIpv4] [dstPort]"

int inOut[2];								// pipe in-out
int 		udpSock;						// socket descriptor
struct 		sockaddr_in thisHostAddr;				// this machine
struct 		sockaddr_in remoteAddr;					// remote addr
struct 		sockaddr_in recvFromAddr;				// machine, from which datagram is received
struct 		sockaddr_in sendToAddr;					// send address
struct 		sockaddr_in anyAddr;					// any address
long 		sendBufLen = BUFFER_SIZE;
long		recvBufLen = BUFFER_SIZE * 5;


void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)		// handler for SIGINT (Ctrl+C)
{
    if (sig == SIGINT)
    {
	if(close(udpSock) < 0)						// close connection
	  fprintf(stderr, "pid: %d, close udpSock signal errno: %d\n", getpid(), errno);  
	exit(0);
    }
}


char* getMyIpv4(char *iface)					// inferface
{
  struct ifreq		ifr;					// for get myIP addr
  int tmpSock;
  if(((tmpSock) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)		
  {
    perror("socket tmpSocket: ");
    fprintf(stderr, "errno: %d\n", errno);
    return NULL;
  }
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name , iface , IFNAMSIZ-1); 			// parce for 'iface' interface
  ioctl(tmpSock, SIOCGIFADDR, &ifr);				// get interfaces information
  thisHostAddr.sin_addr.s_addr = 
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;	
  if(close(tmpSock) < 0)
  {
    perror("close tmpSocket: ");
    fprintf(stderr, "errno: %d\n", errno);
  }
  return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}
int init(char *myHostName, char *myPort, char *remoteHostName, char *remotePort)
{
	int	soOptionOn = 1;							// for setsockop set option to enable
	bzero(&sendToAddr,sizeof(struct sockaddr_in));
	bzero(&thisHostAddr,sizeof(struct sockaddr_in));
	bzero(&recvFromAddr,sizeof(struct sockaddr_in));
	bzero(&remoteAddr,sizeof(struct sockaddr_in));
	thisHostAddr.sin_family = AF_INET;
	thisHostAddr.sin_port = htons(atoi(myPort));				// convert host byte order -> network byte order
	thisHostAddr.sin_addr.s_addr = inet_addr(myHostName);			// old func convert IPv4 char* -> IPv4 bin 
	
	anyAddr.sin_family = AF_INET;
	anyAddr.sin_port = htons(atoi(myPort));				// convert host byte order -> network byte order
	anyAddr.sin_addr.s_addr =  htonl(INADDR_ANY);			// old func convert IPv4 char* -> IPv4 bin
	
	sendToAddr.sin_family = AF_INET;
	sendToAddr.sin_addr.s_addr = inet_addr(remoteHostName);			// htonl(INADDR_BROADCAST); htonl(INADDR_ANY);
	if(((udpSock) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)		
	{
		perror("socket udpSocket: ");			// errno == 4 means EINTR == Interrupted system call 
		fprintf(stderr, "errno: %d\n", errno);		// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		return -1;
	}	
	if(remotePort == NULL)							// if broadcast
	{	  
	  sendToAddr.sin_port = htons(atoi(myPort));				// convert host byte order -> network byte order
	  memcpy(&remoteAddr, &anyAddr, sizeof(remoteAddr));
	  fprintf(stdout, "*** %s:%s start chat in broadcast mode ***\n", myHostName, myPort);
	}
	else									// if multicast
	{ 
	  sendToAddr.sin_port = htons(atoi(remotePort));				// convert host byte order -> network byte order
	  memcpy(&remoteAddr, &sendToAddr, sizeof(remoteAddr));
	  fprintf(stdout, "*** %s:%s start chat in multicast mode with %s:%s ***\n",
		  myHostName, myPort, remoteHostName, remotePort);
	}
	setsockopt(udpSock, SOL_SOCKET, 
		    SO_REUSEADDR, &soOptionOn, sizeof (soOptionOn));	// reuse ADDR when socket in TIME_WAIT condition									
	setsockopt(udpSock, SOL_SOCKET, 				// resize receive buffer
		    SO_RCVBUF, &recvBufLen, sizeof(recvBufLen));
	setsockopt(udpSock, SOL_SOCKET, 				// resize send buffer
		    SO_SNDBUF, &sendBufLen, sizeof(sendBufLen));
	setsockopt(udpSock, SOL_SOCKET,
		   SO_BROADCAST, &soOptionOn, sizeof(soOptionOn));	// broadcast on	
	if(bind((udpSock), (struct sockaddr *)&remoteAddr, sizeof(remoteAddr)) < 0)
	{
	    perror("bind udpSocket: ");				// errno == 4 means EINTR == Interrupted system call 
	    fprintf(stderr, "errno: %d\n", errno);		// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
	    return -1;
	}
	return 0;
}
int startSend()
{
  char helloMess[] = "[ENTER YOUR MESSAGE HERE]\n";
  char inBuf[1024];
  close(inOut[0]);
  /*
  dup2(inOut[1], STDOUT_FILENO);
  close(inOut[1]);
  */
  int readBytes = 0;
  while(1)
  {
    write(STDOUT_FILENO, helloMess, strlen(helloMess));
    readBytes = fscanf(stdin, "%s", inBuf);
    //write(STDOUT_FILENO, inBuf, strlen(inBuf));
    write(inOut[1], inBuf, strlen(inBuf));
    system("clear");
  }
  close(inOut[1]);
  return 0;
}

int startRecv()
{
  close(inOut[1]);
  dup2(inOut[0], STDIN_FILENO);
  close(inOut[0]);
  int retVal;
  socklen_t recvFromAddrLen = sizeof(recvFromAddr);
  int readSocketBytes, readStdinBytes;
  char recvFromAddrChar[INET_ADDRSTRLEN+10];
  char readSocketBuff[recvBufLen];
  char readStdinBuff[sendBufLen];
  struct pollfd inDataSet[2];
  memset(inDataSet, 0 , sizeof(inDataSet));
  inDataSet[0].fd = fileno(stdin);
  inDataSet[0].events = POLLIN;
  
  inDataSet[1].fd = udpSock;
  inDataSet[1].events = POLLIN;
  char buffer[1024];
  ssize_t nbytes;      
  int i;
  while(1)
  {
    while((retVal = poll(inDataSet, 2, 0)) < 0)
    {
      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
      {
	perror("poll: ");				// errno == 4 means EINTR == Interrupted system call 
	fprintf(stderr, "errno: %d\n", errno);		// errno == 11 means EAGAIN or EWOULDBLOCK == Try again		
	return -1;    			
      }
    }
    if(retVal > 0)
    {
      if((inDataSet[0].revents & POLLIN) == POLLIN)			// in data from stdin is ready to read
      {	
	if((readStdinBytes = read(STDIN_FILENO, readStdinBuff, sendBufLen)) > 0)
	{							// read data from stdin
	  while(sendto(udpSock, readStdinBuff, readStdinBytes, 0,  
				    (struct sockaddr*)&sendToAddr, sizeof(sendToAddr)) < 0)
	  {						// send data
	    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
	    {
	      perror("sendTo udpSocket: ");		// errno == 4 means EINTR == Interrupted system call 
	      fprintf(stderr, "errno: %d\n", errno);		// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
	      return -1;
	    }
	  }   
	  /*
	  write(STDOUT_FILENO, "[you]: ", 7);     	// show data on terminal
	  write(STDOUT_FILENO, readStdinBuff, readStdinBytes);
	  write(STDOUT_FILENO, "\n ", 1);     		// show data on terminal
	  */	  
	}      
      }
      if((inDataSet[1].revents & POLLIN) == POLLIN)			// if data from socket is ready to read
      {
	while((readSocketBytes = recvfrom(udpSock,(char*)&readSocketBuff, recvBufLen, 0, 
			      (struct sockaddr*)&recvFromAddr, &recvFromAddrLen)) < 0)
	{							// read data from socket
	  if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
	  {
	    perror("recvFrom udpSocket: ");		// errno == 4 means EINTR == Interrupted system call 
	    fprintf(stderr, "errno: %d\n", errno);	// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
	    return -1;					
	  }
	}
	sprintf(recvFromAddrChar, "[%s:%d]: ", inet_ntoa(recvFromAddr.sin_addr), ntohs(recvFromAddr.sin_port));
	write(STDOUT_FILENO, recvFromAddrChar, strlen(recvFromAddrChar));      
	write(STDOUT_FILENO, readSocketBuff, readSocketBytes);    
	write(STDOUT_FILENO, "\n ", 1);
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  if(argc != 3 && argc != 4)
  {
    puts(ARG_ERROR_MESS);
    return -1;
  }
  if(argc == 4 && !strcmp(argv[1], "childInput"))
  {
    inOut[0] = atoi(argv[2]);
    inOut[1] = atoi(argv[3]);
    startSend();
  }
  else
  {
    pipe(inOut);    
    switch(fork())
    {
      case -1:
      {
	break;
      }
      case 0:
      {
	char stdIn[6], stdOut[6];
	sprintf(stdIn,"%d",inOut[0]);
	sprintf(stdOut,"%d",inOut[1]);
	char *argList[] = { "xterm", "-e", argv[0], "childInput", stdIn, stdOut, NULL };
	execvp("xterm", argList);
	break;
      }
      default:
      {
	struct sigaction closeTerm;
	closeTerm.sa_flags = SA_SIGINFO;	
	closeTerm.sa_sigaction = &hdl_SIGINT;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)			// set handler for SIGINT signal (CTRL+C)
	{
	  fprintf(stderr, "pid: %d, sigaction closeTerm", getpid());
	  return -1;
	}		
	system("clear");
	if(argc == 4)
	  init(getMyIpv4(argv[1]), LOCAL_PORT, argv[2], argv[3]);	
	else
	  init(getMyIpv4(argv[1]), LOCAL_PORT, argv[2], NULL);
	startRecv();
	break;
      }
    }
  }
  return 0;
}