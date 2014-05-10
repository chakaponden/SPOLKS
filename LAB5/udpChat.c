/*
 * keys mapping:
 * F1 == send IGMP join multicast group
 * F2 == send IGMP drop multicast group
 * F3 == reconnect  new multicast group
 * requires 'xterm' app for execute
 * all ipv4 multicast addr:
 * 224.0.0.0 - 239.255.255.255
 * multicast addr for test: 234.5.6.7
 */

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
#include <termios.h>

#define	BUFFER_SIZE  		1024
#define ARG_ERROR_MESS		"./a.out [interface] [dstPort] [dstIpv4]"

int 		inOut[2];						// pipe in-out
int 		udpSock;						// socket descriptor
struct 		sockaddr_in remoteAddr;					// remote addr
struct 		sockaddr_in recvFromAddr;				// machine, from which datagram is received
struct 		sockaddr_in sendToAddr;					// send address
struct 		sockaddr_in anyAddr;					// any address
struct 		ip_mreq mreq;
int		multicastEnable = 0;
long 		sendBufLen = BUFFER_SIZE;
long		recvBufLen = BUFFER_SIZE * 5;
int 		ppid;
static struct 	termios stored_settings;
char		interface[32];
int 		childPid;
int 		on = 1;
     
void set_keypress(void)
{
  struct termios new_settings;
  tcgetattr(0,&stored_settings);
  new_settings = stored_settings;
  /* Disable canonical mode, and set buffer size to 1 byte */
  new_settings.c_lflag &= (~ICANON);
  new_settings.c_cc[VTIME] = 0;
  new_settings.c_cc[VMIN] = 1;   
  tcsetattr(0,TCSANOW,&new_settings);
  return;
}     
void reset_keypress(void)
{
  tcsetattr(0,TCSANOW,&stored_settings);
  return;
}


char* getMyIpv4(char *iface)					// inferface
{
  struct ifreq		ifr;					// for get myIP addr
  struct 		sockaddr_in thisHostAddr;
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

void hdl_SIGINT_PARENT(int sig, siginfo_t *siginfo, void *context)		// handler for SIGINT (Ctrl+C)
{
    if (sig == SIGINT)
    {
      if(multicastEnable)
      {
	if (setsockopt(udpSock,IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
	  if(errno != 99)
	  {
		perror("setsockopt drop multicast group");
		fprintf(stderr, "errno: %d\n", errno);
	  }
	}
      }
      shutdown(udpSock, SHUT_RDWR);
      if(close(udpSock) < 0)						// close connection
	fprintf(stderr, "pid: %d, close udpSock signal errno: %d\n", getpid(), errno);  
      exit(0);
    }
}

void hdl_SIGVTALRM_PARENT(int sig, siginfo_t *siginfo, void *context)		// handler for SIGVTALRM == new multicast group
{
    if (sig == SIGVTALRM)
    {
      if(multicastEnable)
      {
	kill(getpid(), SIGUSR2);
	on = 0;	
      }
    }
}

void hdl_SIGUSR1_PARENT(int sig, siginfo_t *siginfo, void *context)		// handler for SIGUSR1 == join multicast group
{
    if (sig == SIGUSR1)
    {
      if(multicastEnable)
      {
	if (setsockopt(udpSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
	  if(errno == 98)
	    fprintf(stdout, "!!! You already join multicast group !!!\n");
	  else
	  {
	    perror("setsockopt join multicast group: ");
	    fprintf(stderr, "errno: %d\n", errno);
	  }
	}
	else
	  fprintf(stdout, "*** Join multicast group successful! ***\n");
      }
    }
}

void hdl_SIGUSR2_PARENT(int sig, siginfo_t *siginfo, void *context)		// handler for SIGUSR2 == drop multicast group
{
    if (sig == SIGUSR2)
    {
      if(multicastEnable)
      {
	if (setsockopt(udpSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
	  if(errno == 99)
	    fprintf(stdout, "!!! You did not join multicast group !!!	\n");
	  else
	  {
	    perror("setsockopt drop multicast group: ");
	    fprintf(stderr, "errno: %d\n", errno);
	  }
	}
	else
	  fprintf(stdout, "*** Drop multicast group successful! ***\n");
      }
    }
}

void hdl_SIGINT_CHILD(int sig, siginfo_t *siginfo, void *context)		// handler for SIGINT (Ctrl+C)
{
    if (sig == SIGINT)
    {
      kill(ppid, SIGINT);
      exit(0);
    }
}


int init(char *myHostAddr, char *remotePort, char *remoteHostName)
{
	int	soOptionOn = 1;							// for setsockop set option to enable
	bzero(&sendToAddr,sizeof(struct sockaddr_in));
	bzero(&recvFromAddr,sizeof(struct sockaddr_in));
	bzero(&remoteAddr,sizeof(struct sockaddr_in));	
	
	sendToAddr.sin_family = AF_INET;
	if(!multicastEnable)							// broadcast
	{
	  sendToAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);			// htonl(INADDR_BROADCAST); htonl(INADDR_ANY);
	  remoteAddr.sin_addr.s_addr =  htonl(INADDR_ANY);
	}
	else									// multicast
	{
	  sendToAddr.sin_addr.s_addr = inet_addr(remoteHostName);
	  remoteAddr.sin_addr.s_addr =  inet_addr(remoteHostName);;
	}
	sendToAddr.sin_port = htons(atoi(remotePort));
	
	if(((udpSock) = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)		
	{
		perror("socket udpSocket: ");			// errno == 4 means EINTR == Interrupted system call 
		fprintf(stderr, "errno: %d\n", errno);		// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
		return -1;
	}	
	remoteAddr.sin_family = AF_INET;	
	remoteAddr.sin_port = htons(atoi(remotePort));
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
	/* use setsockopt() to request that the kernel join a multicast group */
	/* when setsockoption IP_ADD_MEMBERSHIP - then send IGMP message to router from which datagram stream will be send
	 * IGMP message for add this IP addr to destination multicast group
	 * if you want several multicst group from one socket, then you need
	 * to bind to addr '0.0.0.0' and manually determine destination addr of each received datagram
	 * or you need to use IGMPv3 and specify sender's addr for each multicast group 
	 */
	if(multicastEnable)
	{
	  mreq.imr_multiaddr.s_addr=inet_addr(remoteHostName);
	  mreq.imr_interface.s_addr=inet_addr(myHostAddr);
	  if (setsockopt(udpSock,IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	  {
		perror("setsockopt join multicast group");
		fprintf(stderr, "errno: %d\n", errno);
		return -1;
	  }
	}	
	return 0;
}
int startSend()
{
  char helloMess[] = "F1 - send IGMP join multicast group\nF2 - send IGMP drop multicast group\nF3 - reconnect  new multicast group\n[ENTER YOUR MESSAGE HERE]\n";
  char inBuf[1024] = { 0 };
  close(inOut[0]);
  /*
  dup2(inOut[1], STDOUT_FILENO);
  close(inOut[1]);
  */
  int i;
  int inputChar;
  set_keypress();
  while(1)
  {
    i = 0;
    write(STDOUT_FILENO, helloMess, strlen(helloMess));
    while((inBuf[i] = (char)getchar()) != '\n')
    {      
      switch(inBuf[i])
      {
	case 80:			// F1 == send IGMP join group
	{
	  fprintf(stdout, "\b\b\b\b    \b\b\b\b");
	  kill(ppid, SIGUSR1);
	  inBuf[i] = '0';
	  break;
	}
	case 81:			// F2 == send IGMP drop group
	{
	  fprintf(stdout, "\b\b\b\b    \b\b\b\b");
	  kill(ppid, SIGUSR2);
	  inBuf[i] = '0';
	  break;
	}
	case 82:			// F3 == connect new multicast
	{
	  //fprintf(stdout, "\b\b\b\b    \b\b\b\b");
	  system("clear");
	  kill(ppid, SIGVTALRM);
	  write(STDOUT_FILENO, "[ENTER MULTICAST IP]\n", 22);
	  reset_keypress();
	  fscanf(stdin, "%s", inBuf);
	  write(inOut[1], inBuf, strlen(inBuf));	
	  system("clear");
	  write(STDOUT_FILENO, "[ENTER MULTICAST PORT]\n", 23);
	  fscanf(stdin, "%s", inBuf);
	  write(inOut[1], inBuf, strlen(inBuf));	
	  system("clear");
	  write(STDOUT_FILENO, "[ENTER YOUR MESSAGE HERE]\n", strlen(helloMess));
	  set_keypress();
	  i = 0;
	  inBuf[i] = '0';
	  break;
	}
	default:
	{	
	  i++;
	  break;
	}	
      }
    //write(STDOUT_FILENO, inBuf, strlen(inBuf));
    }
    inBuf[i] = '\0';    
    write(inOut[1], inBuf, strlen(inBuf));
    system("clear");    
  }
  close(inOut[1]);
  return 0;
}

int startRecv()
{  
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
  while(on != 0)
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

	if(multicastEnable)
	  sprintf(recvFromAddrChar, "[%s:%d]: ", inet_ntoa(recvFromAddr.sin_addr), ntohs(recvFromAddr.sin_port));
	else
	  sprintf(recvFromAddrChar, "[%s]: ", inet_ntoa(recvFromAddr.sin_addr));
	write(STDOUT_FILENO, recvFromAddrChar, strlen(recvFromAddrChar));      
	write(STDOUT_FILENO, readSocketBuff, readSocketBytes);    
	write(STDOUT_FILENO, "\n ", 1);
      }
    }
  }
  shutdown(udpSock, SHUT_RDWR);
  if(close(udpSock)< 0)
  {
    perror("close: ");
    return -1;
  }
  return 0;
}

int reinit()
{
	char readStdinBuff[sendBufLen];
	char addr[32];
	char port[8];
	long readStdinBytes;	
		
	readStdinBytes = read(STDIN_FILENO, readStdinBuff, sendBufLen);
	readStdinBuff[readStdinBytes] = '\0';
	strcpy(addr, readStdinBuff);
	
	readStdinBytes = read(STDIN_FILENO, readStdinBuff, sendBufLen);
	readStdinBuff[readStdinBytes] = '\0';	
	strcpy(port, readStdinBuff);
		fseek(stdin,0,SEEK_END);
	if(init(getMyIpv4(interface), port, addr) < 0)
	{
	  fprintf(stdout, "*** %s:%s failed restart udp chat multicast %s:%s ***\n",
		  getMyIpv4(interface), port, addr, port);
	  return -1;
	  	
	}
	else
	{
	  fprintf(stdout, "*** %s:%s restart udp chat multicast %s:%s ***\n",
		  getMyIpv4(interface), port, addr, port);
	  
	}
	return 0;
}

int main(int argc, char *argv[])
{
  if(argc < 3)
  {
    puts(ARG_ERROR_MESS);
    return -1;
  }
  if(argc == 5 && !strcmp(argv[1], "childInput"))
  {
    struct sigaction closeTerm;
    closeTerm.sa_flags = SA_SIGINFO;	
    closeTerm.sa_sigaction = &hdl_SIGINT_CHILD;
    if(sigaction(SIGINT, &closeTerm, NULL) < 0)			// set handler for SIGINT signal (CTRL+C)
    {
      fprintf(stderr, "pid: %d, sigaction closeTerm", getpid());
      return -1;
    }		
    inOut[0] = atoi(argv[2]);
    inOut[1] = atoi(argv[3]);
    ppid = atoi(argv[4]);
    startSend();
  }
  else
  {
    pipe(inOut);    
    switch(childPid = fork())
    {
      case -1:
      {
	perror("fork error: ");
	fprintf(stderr, "errno: %d\n", errno);
	break;
      }
      case 0:
      {
	char stdIn[6], stdOut[6], oldPpid[6];
	sprintf(stdIn,"%d",inOut[0]);
	sprintf(stdOut,"%d",inOut[1]);
	sprintf(oldPpid,"%d",getppid());
	char *argList[] = { "xterm", "-e", argv[0], "childInput", stdIn, stdOut, oldPpid, NULL };
	execvp("xterm", argList);
	break;
      }
      default:
      {
	struct sigaction closeTerm;
	closeTerm.sa_flags = SA_SIGINFO;	
	closeTerm.sa_sigaction = &hdl_SIGINT_PARENT;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)			// set handler for SIGINT signal (CTRL+C)
	{
	  fprintf(stderr, "pid: %d, sigaction closeTerm", getpid());
	  return -1;
	}		
	system("clear");
	if(argc == 4)							// multicast
	{
	  strcpy(interface, argv[1]);
	  multicastEnable = 1;
	  init(getMyIpv4(argv[1]), argv[2], argv[3]);
	  
	  struct sigaction joinMulticastGroup;
	  joinMulticastGroup.sa_flags = SA_SIGINFO;	
	  joinMulticastGroup.sa_sigaction = &hdl_SIGUSR1_PARENT;
	  if(sigaction(SIGUSR1, &joinMulticastGroup, NULL) < 0)		// set handler for SIGUSR1 signal joinMulticastGroup
	  {
	    fprintf(stderr, "pid: %d, sigaction joinMulticastGroup", getpid());
	    return -1;
	  }		
	  struct sigaction dropMulticastGroup;
	  dropMulticastGroup.sa_flags = SA_SIGINFO;	
	  dropMulticastGroup.sa_sigaction = &hdl_SIGUSR2_PARENT;
	  if(sigaction(SIGUSR2, &dropMulticastGroup, NULL) < 0)		// set handler for SIGUSR2 signal dropMulticastGroup
	  {
	    fprintf(stderr, "pid: %d, sigaction dropMulticastGroup", getpid());
	    return -1;
	  }	  
	  struct sigaction newMulticastGroup;
	  newMulticastGroup.sa_flags = SA_SIGINFO;	
	  newMulticastGroup.sa_sigaction = &hdl_SIGVTALRM_PARENT;
	  if(sigaction(SIGVTALRM, &newMulticastGroup, NULL) < 0)	// set handler for SIGVTALRM signal newMulticastGroup
	  {
	    fprintf(stderr, "pid: %d, sigaction newMulticastGroup", getpid());
	    return -1;
	  }	  
	  fprintf(stdout, "*** %s:%s start udp chat multicast %s:%s ***\n",
		  getMyIpv4(argv[1]), argv[2], argv[3], argv[2]);
	}
	else								// broadcast
	{
	  init(getMyIpv4(argv[1]), argv[2], NULL);
	  fprintf(stdout, "*** %s:%s start udp chat broadcast ***\n", getMyIpv4(argv[1]), argv[2]);
	}
	close(inOut[1]);
	dup2(inOut[0], STDIN_FILENO);
	close(inOut[0]);
	while(on)
	{
	    startRecv();  
	    on = reinit()+1;
	}
	break;
      }
    }
  }
  kill(childPid, SIGINT);
  kill(getpid(), SIGINT);
  return 0;
}