// <edit>
#include "llnetcanary.h"
#include "linden_common.h"
#include "llerror.h"
#ifdef _MSC_VER
#include <winsock2.h>
static WSADATA trapWSAData;
#define socklen_t int
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#if LL_DARWIN
#ifndef _SOCKLEN_T
#define _SOCKLEN_T
typedef int socklen_t;
#endif
#endif

extern int errno; //error number
#define SOCKADDR_IN struct sockaddr_in
#define SOCKADDR struct sockaddr
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SD_BOTH 2
#define closesocket close
#endif
SOCKADDR_IN trapLclAddr;
LLNetCanary::LLNetCanary(int requested_port)
{
	mGood = TRUE;
	int nRet;
	int hSocket;
	int snd_size = 400000;
	int rec_size = 400000;
	int buff_size = 4;
#ifdef _MSC_VER
	if(WSAStartup(0x0202, &trapWSAData))
	{
		S32 err = WSAGetLastError();
		WSACleanup();
		llwarns << "WSAStartup() failure: " << err << llendl;
		mGood = FALSE;
		return;
	}
#endif
	// Get a datagram socket
    	hSocket = (int)socket(AF_INET, SOCK_DGRAM, 0);
    	if (hSocket == INVALID_SOCKET)
	{
#ifdef _MSC_VER
		S32 err = WSAGetLastError();
		WSACleanup();
#else
		S32 err = errno;
#endif
		mGood = FALSE;
		llwarns << "socket() failure: " << err << llendl;
		return;
	}

	// Name the socket (assign the local port number to receive on)
	trapLclAddr.sin_family      = AF_INET;
	trapLclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	trapLclAddr.sin_port        = htons(requested_port);
	//llinfos << "bind() port: " << requested_port << llendl;
	nRet = bind(hSocket, (struct sockaddr*) &trapLclAddr, sizeof(trapLclAddr));
	if (nRet == SOCKET_ERROR)
	{
#ifdef _MSC_VER
		S32 err = WSAGetLastError();
		WSACleanup();
#else
		S32 err = errno;
#endif
		mGood = FALSE;
		llwarns << "bind() failed: " << err << llendl;
		return;
	}

	// Find out what address we got
	SOCKADDR_IN socket_address;
	socklen_t socket_address_size = sizeof(socket_address);
	getsockname(hSocket, (SOCKADDR*) &socket_address, &socket_address_size);
	mPort = ntohs(socket_address.sin_port);
	//llinfos << "got port " << mPort << llendl;
	
	// Set socket to be non-blocking
#ifdef _MSC_VER
	unsigned long argp = 1;
	nRet = ioctlsocket (hSocket, FIONBIO, &argp);
#else
	nRet = fcntl(hSocket, F_SETFL, O_NONBLOCK);
#endif
	if (nRet == SOCKET_ERROR) 
	{
#ifdef _MSC_VER
		S32 err = WSAGetLastError();
		WSACleanup();
#else
		S32 err = errno;
#endif
		mGood = FALSE;
		llwarns << "Failed to set socket non-blocking, Err: " << err << llendl;
		return;
	}

	// set a large receive buffer
	nRet = setsockopt(hSocket, SOL_SOCKET, SO_RCVBUF, (char *)&rec_size, buff_size);
	if (nRet)
	{
		llinfos << "Can't set receive buffer size!" << llendl;
	}
	// set a large send buffer
	nRet = setsockopt(hSocket, SOL_SOCKET, SO_SNDBUF, (char *)&snd_size, buff_size);
	if (nRet)
	{
		llinfos << "Can't set send buffer size!" << llendl;
	}
	//getsockopt(hSocket, SOL_SOCKET, SO_RCVBUF, (char *)&rec_size, &buff_size);
	//getsockopt(hSocket, SOL_SOCKET, SO_SNDBUF, (char *)&snd_size, &buff_size);
	//LL_DEBUGS("AppInit") << "startNet - receive buffer size : " << rec_size << LL_ENDL;
	//LL_DEBUGS("AppInit") << "startNet - send buffer size    : " << snd_size << LL_ENDL;

	mSocket = hSocket;
}
LLNetCanary::~LLNetCanary()
{
	if(mGood)
	{
		if(mSocket)
		{
			shutdown(mSocket, SD_BOTH);
			closesocket(mSocket);
		}
#ifdef _MSC_VER
		WSACleanup();
#endif
	}
}
// </edit>
