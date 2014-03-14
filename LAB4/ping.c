#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>


#define BUFFER_MULT		128					// buffer multiplier
#define	BUFFER_SIZE  		1024					// stock buffer size in bytes
#define ARG_ERROR_MESS		"\n[srcIpv4] [dstIpv4]\n"

struct sockaddr_in		myHostAddr, dstHostAddr, srcHostAddr;
char 				*myIpv4, *dstIpv4, *srcIpv4; 
int 				rawSock, icmpSock;
long 				packetNumber = 0;


#define ICMP_ECHO   		8					// echo request

#define ICMP_ECHOREPLY          0       				// echo reply

#define IP_MAXPACKET    	65535           			// maximum packet size



unsigned short cksum(unsigned short *addr, int len)			// calculate checkSum, function from Pynkin lections
{
  unsigned short result;
  unsigned short sum = 0;
  /* Поле контрольная сумма представляет собой
  16-разрядное дополнение по модулю 1
  контрольной суммы всего ICMP-сообщения,
  начиная с поля тип. */
  while (len > 1) /* складываем все двухбайтовые слова */
  {
    sum += *addr++; 
    len -= 2;
  }  
  if (len == 1) /* если остался лишний байт , прибавляем его к  сумме */
    sum += *(unsigned char*) addr;
  sum = (sum >> 16) + (sum & 0xFFFF); /* добавляем  перенос */
  sum += (sum >> 16);  /* еще раз */
  result = ~sum;  /*  инвертируем результат */	
  return result;
}

int echoRequestSend(pid_t pid)						// send ICMP datagram
{
  char			ipBuf[IP_MAXPACKET] = {0};
  int 			icmpLen, ipLen;
  uint16_t		ckSumIcmp;
  struct icmp 		*icmp;						// define in usr/include/netinet/ip_icmp.h
  struct ip		*ip;
/* initial ip and icmp pointers */ 
  ip   = (struct ip*)ipBuf;		
  icmp = (struct icmp*)(ipBuf + sizeof(struct ip));
/* initial icmp fields */  
  icmp->icmp_type = ICMP_ECHO;						// define in ip_icmp.h as 0
  icmp->icmp_code = 0;
  icmp->icmp_id = pid;							// use pid as id
  icmp->icmp_seq = packetNumber++;
  gettimeofday((struct timeval *)icmp->icmp_data,  NULL);		// creation time
  icmpLen = 8 + 56;							// sizeof(icmp_header) == 8 bytes +
									// sizeof(icmp_data) == 56 bytes
  icmp->icmp_cksum = 0;
/* initial ip fields */  
  ip->ip_v	= 4;							// version
  ip->ip_hl	= 5;							// header lenght in 32 bit words
  ip->ip_tos	= 0;							// type of service
  ipLen = sizeof(struct ip) + icmpLen;					// ip header + ip data lenght
  ip->ip_len	= htons(ipLen);						// host to network byte order  
  ip->ip_id = htons(321);						// id packet
  // flags (3 pcs) does not defined in struct ip
  ip->ip_off = htons(0);						// fragment offset
  ip->ip_ttl = 255;							// time to live
  ip->ip_p = IPPROTO_ICMP;						// protocol
  ip->ip_sum = 0;							// checkSum only for ip header  
  ip->ip_src = srcHostAddr.sin_addr;					// src ip address
  ip->ip_dst = dstHostAddr.sin_addr;					// dst ip address
/* checkSums calculate */ 		
  icmp->icmp_cksum = cksum((unsigned short *)icmp, icmpLen) - 1; 	// icmp checkSum calculate for icmp header and icmp data
  ip->ip_sum = cksum((unsigned short *)ip, ipLen-icmpLen);		// ip checkSum calculate only for ip header
/* send ip datagram */  
  while(sendto(rawSock, ip, ipLen, 0,
    (struct sockaddr *)&dstHostAddr, sizeof(dstHostAddr)) < ipLen)	// wait, while sent all
  {
    //fprintf(stderr, "pid: %d, echoRequestSend sendto errno: %d\n", pid, errno);
									// errno == 4 means EINTR == Interrupted system call 
    if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
      return -1;							// errno == 11 means EAGAIN or EWOULDBLOCK == Try again	
  }
  return 0;
}

int echoReplyProcessing(pid_t pid)					// ip packet disassembly
{
  
  return 0;
}

void hdl_SIGINT(int sig, siginfo_t *siginfo, void *context)		// handler for SIGINT (Ctrl+C)
{
    if (sig == SIGINT)
    {
	if(shutdown(rawSock, SHUT_RDWR) < 0)				// deny connection raw
	  fprintf(stderr, "pid: %d, shutdown rawSock signal errno: %d\n", getpid(), errno);
	if(close(rawSock) < 0)						// close connection raw
	  fprintf(stderr, "pid: %d, close rawSock signal errno: %d\n", getpid(), errno);   
	if(shutdown(icmpSock, SHUT_RDWR) < 0)				// deny connection icmp
	  fprintf(stderr, "pid: %d, shutdown icmpSock signal errno: %d\n", getpid(), errno);
	if(close(icmpSock) < 0)						// close connection icmp
	  fprintf(stderr, "pid: %d, close icmpSock signal errno: %d\n", getpid(), errno);   
	exit(0);
    }
}

void hdl_SIGALARM(int sig, siginfo_t *siginfo, void *context)		// handler for SIGALRM (timer)
{
    if (sig == SIGALRM)
      echoRequestSend(getpid());
}

int startPing(pid_t srcPid)
{	
	struct itimerval	timer;
	struct sigaction	alarm, closeTerm;
	struct ifreq		ifr;					// for get myIP addr
	struct sockaddr_in 	remoteHostAddr;
	/* show timeout
	struct timeval		start, check;
	*/
	int 			readBytes0 = -1;
	int 			readBytes1 = 0;
	uint16_t		tmpPid = 0;
	int 			remoteHostAddrLen = sizeof(remoteHostAddr);;
	int 			soOptionOn = 1;
	long 			recvBufLen = (BUFFER_SIZE*sizeof(char)*BUFFER_MULT);
	long 			sendBufLen = (BUFFER_SIZE*sizeof(char)*BUFFER_MULT);	
	char 			recvBuf[2 * recvBufLen];
	char 			iface[] = "eth0";			// inferface
/* configure sockets */	
	setsockopt(rawSock, SOL_SOCKET, 				// RAW reuse address
		   SO_REUSEADDR, &soOptionOn, sizeof(soOptionOn));
	setsockopt(icmpSock, SOL_SOCKET, 				// ICMP reuse address
		   SO_REUSEADDR, &soOptionOn, sizeof(soOptionOn));
	setsockopt(icmpSock, SOL_SOCKET, 				// ICMP resize receive buffer
		   SO_RCVBUF, &recvBufLen, sizeof(recvBufLen));
	setsockopt(rawSock, SOL_SOCKET, 				// RAW resize send buffer
		   SO_SNDBUF, &sendBufLen, sizeof(sendBufLen));	
	
	setsockopt(rawSock, IPPROTO_IP,					// RAW tell kernel, that we provide IP structure manually
		   IP_HDRINCL, &soOptionOn, sizeof(soOptionOn));	
	/*
	setsockopt(rawSock, SOL_SOCKET,
		   SO_BROADCAST, &soOptionOn, sizeof(soOptionOn));	// broadcast on
	*/	   
	setuid(0);							// get root access
	if(((rawSock) = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) 	// create RAW socket
   	{
	  fprintf(stderr, "pid: %d, RAW socket errno: %d\n", srcPid, errno);
	  return -1;
    	}    
    	if(((icmpSock) = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) 	// create ICMP socket
   	{
	  fprintf(stderr, "pid: %d, ICMP socket errno: %d\n", srcPid, errno);
	  return -1;
    	}
    	setuid(geteuid());						// return to your user
/* initial my local host address */	
	myHostAddr.sin_family = AF_INET;
    	myHostAddr.sin_port = htons(0);					// '0' port == random free port; not matter with ICMP
									// + host byte order -> network byte order	
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name , iface , IFNAMSIZ-1); 			// parce for 'iface' interface
	ioctl(rawSock, SIOCGIFADDR, &ifr);				// get interfaces information
    	myHostAddr.sin_addr.s_addr = 
    	((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;	
	myIpv4 = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
/* init SIGALRM  */	
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 1;					// activation delay == 1 msec
	timer.it_interval.tv_sec = 1;					// signal send interval == 1 sec
	timer.it_interval.tv_usec = 0;
	alarm.sa_flags = SA_SIGINFO;
	alarm.sa_sigaction = &hdl_SIGALARM;
	if(sigaction(SIGALRM, &alarm, NULL) < 0)			// set handler for SIGALRM signal (timer)
	{
	  fprintf(stderr, "pid: %d, sigaction alarm", srcPid);
	  return -1;
	}
/* init SIGINT (press CTRL+C) */	
	closeTerm.sa_flags = SA_SIGINFO;	
	closeTerm.sa_sigaction = &hdl_SIGINT;
	if(sigaction(SIGINT, &closeTerm, NULL) < 0)			// set handler for SIGINT signal (CTRL+C)
	{
	  fprintf(stderr, "pid: %d, sigaction closeTerm", srcPid);
	  return -1;
	}		
/* socket is associated with 'eth0' IPv4 address */	
/*
	if(bind((rawSock), (struct sockaddr *)&myHostAddr, sizeof(myHostAddr)) < 0)
	{
	    fprintf(stderr, "pid: %d, bind errno: %d\n", srcPid, errno);
	    return -1;
	}  	
*/	
	struct pollfd tempSet;
	int highDescSocket = 1;
	int time_out = 4000;						// poll timeout == 4 sec
	int retVal = -1;
	tempSet.fd = icmpSock;
	tempSet.events = POLLIN;					// check input
	setitimer(ITIMER_REAL, &timer, NULL);				// start timer to send SIGALARM signal
	while(1)
	{
	  /* show timeout
	  gettimeofday(&start, NULL);
	  */
	  while((retVal = poll(&tempSet, highDescSocket, time_out)) < 0)
	  {								// errno == 4 means EINTR == Interrupted system call
	      //fprintf(stderr, "pid: %d, startPing poll errno: %d\n", srcPid, errno);
	    /* show timeout
	      gettimeofday(&check, NULL);
	      if(check.tv_sec - start.tv_sec > 4)
	      {
		printf("Request Timed Out: %d ms\n", time_out);
		gettimeofday(&start, NULL);
	      }
	      */
	      if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
	      {
		fprintf(stderr, "pid: %d, startPing poll bad errno: %d\n", srcPid, errno);
		return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
	      }
	  }	
	  if(retVal)							// if input data appeared on socket
	  {								// read, but don't remove from input queue (MSG_PEEK flag ON)
	    while((readBytes0 = recvfrom(icmpSock, (char*)&recvBuf, recvBufLen, MSG_PEEK, 
				    (struct sockaddr*)&remoteHostAddr, &remoteHostAddrLen)) < 0)
									// read reply
	    {								// errno == 4 means EINTR == Interrupted system call
		//fprintf(stderr, "pid: %d, startPing recvFrom0 errno: %d\n", srcPid, errno);
	      /* show timeout
		gettimeofday(&check, NULL);
		if(check.tv_sec - start.tv_sec > 4)
		{
		  printf("Request Timed Out: %d ms\n", time_out);
		  gettimeofday(&start, NULL);
		}
		*/
		if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		{
		  fprintf(stderr, "pid: %d, startPing recvFrom0 bad errno: %d\n", srcPid, errno);
		  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		}
	    }
	    memcpy(&tmpPid, ((recvBuf)+24), sizeof(uint16_t));		// read id == pid of received ICMP datagram
	    // printf("readbytes: %d pid: %d\n", readBytes, tmpPid);	    
	    if(tmpPid == srcPid)					// if pid is the same
	    {								// read datagram + remove from input queue (MSG_PEEK flag OFF)
	      while((readBytes1 = recvfrom(icmpSock, (char*)&recvBuf, recvBufLen, 0, 
				    (struct sockaddr*)&remoteHostAddr, &remoteHostAddrLen)) < readBytes0-readBytes1)
									// read reply
	    {								// errno == 4 means EINTR == Interrupted system call
		//fprintf(stderr, "pid: %d, startPing recvFrom1 errno: %d\n", srcPid, errno);
	      /* show timeout
		gettimeofday(&check, NULL);
		if(check.tv_sec - start.tv_sec > 4)
		{
		  printf("Request Timed Out: %d ms\n", time_out);
		  gettimeofday(&start, NULL);
		}
		*/
		if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)	
		{
		  fprintf(stderr, "pid: %d, startPing recvFrom1 bad errno: %d\n", srcPid, errno);
		  return -1;						// errno == 11 means EAGAIN or EWOULDBLOCK == Try again
		}
	    }
	      echoReplyProcessing(srcPid);				// call ip datagramm disassembly
	    }
	  }
	  else								// no input data on socket == poll timeout exceeded
	  {
	    printf("Request Timed Out: %d ms\n", time_out);
	  }
	}
	return 0;
}


int main(int argc, char *argv[])
{
	if(argc != 3)							// wrong command-line arguments
	{
	  puts(ARG_ERROR_MESS);
 	  fprintf(stderr, "pid: %d, main invalid command-line arguments", getpid());
	  return -1;
	}
	else
	{
/* initial source host address */	
	  srcIpv4 = argv[1];
	  srcHostAddr.sin_family = AF_INET;
	  srcHostAddr.sin_port = htons(0);				// convert host byte order -> network byte order	
	  srcHostAddr.sin_addr.s_addr = inet_addr(srcIpv4);		// old func convert IPv4 char* -> IPv4 bin
									// (+ host byte order -> network byte order too) 
/* initial destination host address */	
	  dstIpv4 = argv[2];
	  dstHostAddr.sin_family = AF_INET;
	  dstHostAddr.sin_port = htons(0);				// convert host byte order -> network byte order	
	  dstHostAddr.sin_addr.s_addr = inet_addr(dstIpv4);		// old func convert IPv4 char* -> IPv4 bin
									// (+ host byte order -> network byte order too)  
	}
	return startPing(getpid());
}