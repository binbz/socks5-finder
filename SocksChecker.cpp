#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include <winsock.h>
#define NIL ((DWORD)0l)
#define RETURNTYPE DWORD WINAPI
#define SHUT_RDWR SD_BOTH
#include <shlobj.h>
#include <shlwapi.h>

//#include "stdafx.h"
#include <iostream>
#include <stdio.h>
#include <wininet.h>
#include <vector>
#include <string>

#ifdef __CYGWIN__
#include <windows.h>
#define daemonize() FreeConsole()
#define SLEEPTIME 1000
#undef _WIN32
#elif _WIN32
#ifdef errno
#undef errno
#endif
#define errno WSAGetLastError()
#define EAGAIN WSAEWOULDBLOCK
#define SLEEPTIME 1
#define close closesocket
#define usleep Sleep
#define pthread_self GetCurrentThreadId
#define getpid GetCurrentProcessId
#define pthread_t DWORD
#define daemonize() FreeConsole()
#else
#include <pthread.h>
#define daemonize() daemon(0,0)
#define SLEEPTIME 1000
#endif

int verbose = 1;
int result = 0;
int threadcount = 0;
int to = 25000;

int sockgetchar(int sock, int timeosec, int timeousec){
 unsigned char buf;
 int res;
 fd_set fds;
 struct timeval tv = {timeosec,timeousec};

 FD_ZERO(&fds);
 FD_SET(sock, &fds);
 if ((res = select (sock+1, &fds, NULL, NULL, &tv))!=1) {
	return EOF;
 }
 if ((res = recv(sock, (char *)&buf, 1, 0))!=1) {
	return EOF;
 }

/*
fprintf(stderr, " -%d- ", (int)buf);
fflush(stderr);
*/
 return((int)buf);
}

#define MAX_DATE 12

void get_date(char *date)
{
   time_t now;
   char the_date[MAX_DATE];

 the_date[0] = '\0';

   now = time(NULL);

   if (now != -1)
   {
      strftime(the_date, MAX_DATE, "%H_%d_%m", gmtime(&now));
   }

   strcpy(date,"socks_");
	   strcat(date,the_date);
	   strcat(date,".txt");
  
}


using namespace std;


int getIpNumbers(const char * buf, vector<string> & ips,int n)
{
    string header = buf;
    int    len    = (int)header.length();
    int    ip[4]  = { 0 };
    int    lip[4] = { 0 };
    int    cur    = 0;
    int    lpos   = 0;
    bool   next   = false;
    char   c   = '\0';
    // we also read the terminating zero char
    for (int i = 0; i <= len; ++i)
    {
        c = buf[i];
        switch (c)
        {
        case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
            {
                ip[cur] = ip[cur]*10 + (c - '0');
                if (ip[cur] >= 255 || ++lip[cur] > 3)
                    next = true;
                break;
            }
        case '.':
            {
                if (++cur > 3) 
                    next = true;
                break;
            }
        default:
            {
                if (cur == 3)
                {
                    ips.push_back(header.substr(lpos, i - lpos +n));
                }
                next = true;
                break;
            }
        }
        if (next)
        {
            ip[0]  = ip[1]  = ip[2]  = ip[3]  = 0;
            lip[0] = lip[1] = lip[2] = lip[3] = 0;
            cur  = 0;
            lpos = i+1;
            next = false;
        }
    }
    return (int)ips.size();
}

char* const* convert(const vector< string > &v) 
{ 
  char** cc = new char*[v.size()]; //if you make the pointers const here allready, you won't be able to fill them 
  for(unsigned int i = 0; i < v.size(); ++i) 
  { 
    cc[i] = new char[v[ i ].size()+1]; //make it fit 
    strcpy(cc[i], v[ i ].c_str());     //copy string 
  } 
  return cc; //pointers to the strings will be const to whoever receives this data. 
} 


void GetPort(char* str, char* out,char delimport)
{
  int size = strlen(str);
  char* p = str + size;
  for(int i=size; i >= 0; i--)
  {
    if( *(--p) == delimport )
    {
      strcpy(out, p+1);
      break;
    }
  }
}

void GetStringBeforeDelim(char* str, char* out, char delimip)
{
  int len = strlen(str);
  for(int i=0; i < len; i++)
  {
   if(str[i]== delimip ) break;  
   out[i]=str[i];
  }
}

int sockgetline(int sock, char * buf, int bufsize, int delim, int timeout){
 int c;
 int i=0, tos, tou;

timeout = 4;
 
 
 if(!bufsize) return 0;
 c = sockgetchar(sock, timeout, 0);
 if (c == EOF) {
	return 0;
 }
 buf[i++] = c;
 tos = (timeout>>4);
 tou = ((timeout * 1000) >> 4)%1000;
 while(i < bufsize && (c = sockgetchar(sock, tos, tou)) != EOF) {
	buf[i++] = c;
	if(delim != EOF && c == delim) break;
 }
 return i;
}

#define PROXY 1
#define SOCKS4 2
#define SOCKS5 4
#define CONNECT 8
#define WINPROXY 16
#define FTP 32
#define TELNET 64

struct clientparam {
	struct sockaddr_in sins;
	struct sockaddr_in sinc;
	char * webhost;
	struct in_addr webip;
	char * url;
	char *keyword;
	int dosmtp;
};



void WriteFile(char *ofile,char *data,int len)
{
FILE *f;
fopen_s( &f, ofile, "wb" );  
if(f==NULL) return;
fwrite(data,1,len,f);
fclose(f);
}


void AppendFile(char *ofile,char *data,int len)
{
FILE *f;
fopen_s( &f, ofile, "ab+" );  
if(f==NULL) return;
fwrite(data,1,len,f);
fclose(f);
}




char * dosmtp(struct clientparam* param, int sock, char * module){
	char buf[1024];
	int res;

	if((res = sockgetline(sock,buf,1020,'\r',(1 + (to>>10)))) < 3) {
		return "something, not SMTP server (reply is too short)";
	}
	buf[res] = 0;
	if(verbose > 2)fprintf(stderr, "%s received: %s\n", module, buf);
	if(buf[0] != '2' || buf[1] != '2' || buf[2] != '0') {
		return "bad SMTP response";
	}
	if(param->keyword) {
		if(!strstr(buf, param->keyword)) return "No keyword found in server response";
	}
	return NULL;
}


char * dohttp(struct clientparam* param, int sock, char * module){
	char buf[1024];
	int res;

	if (*module == 'H') sprintf_s(buf, "GET http://%.100s%s%.500s HTTP/1.0\r\nHost: %s\r\n%s\r\n\r\n", param->webhost, param->dosmtp? ":25" : "", param->url, param->webhost, param->dosmtp? "quit" : "Pragma: no-cache");
	else sprintf_s(buf, "GET %.500s HTTP/1.0\r\nHost: %s\r\n%s\r\n\r\n", param->url, param->webhost, param->dosmtp? "quit" : "Pragma: no-cache");
	if(verbose > 2)fprintf(stderr, "%s sending:\n%s", module, buf);
	if(send(sock, buf, strlen(buf), 0) != strlen(buf)) {
		return "data not accepted";
	};
	if(param->dosmtp) {
		return dosmtp(param, sock, module);
	}
	if((sockgetline(sock,buf,12,'\r',(1 + (to>>10)))) < 12) {
		return "something, not HTTP server (reply is too short)";
	}
	buf[12] = 0;
	if(verbose > 2)fprintf(stderr, "%s received: %s\n", module, buf);
	if(buf[9] != '2') {
		return "failed to retrieve requested URL";
	}
	if(param->keyword) {
		while((res = sockgetline(sock,buf,1023,'\n',(1 + (to>>10)))) > 0){
			buf[res] = 0;
			if(strstr(buf, param->keyword)) break;
		}
		if(res <= 0) {
			return "No keyword found in server response";
		}
	}
	return NULL;
}

RETURNTYPE doproxy(void * data){
	int sock = -1;
	char * string = "";
    char str2[500] ="";
	char dat[100]="";
	get_date(dat);
	if ((sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		string = "error";
		goto CLEANRET1;
	}
	if (bind(sock, (struct sockaddr *)&((struct clientparam*)data)->sinc,sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		string = "error";
		goto CLEANRET1;
	}
	fprintf(stderr, "Http connecting %s:%hu\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port));
	if(connect(sock,(struct sockaddr *)&((struct clientparam*)data)->sins,sizeof(struct sockaddr_in))) {
		string = "closed";
		goto CLEANRET1;
	}
	string = dohttp((struct clientparam*) data, sock, "HTTP");
	if (string) goto CLEANRET1;
	string = "OK";
	fprintf(stderr, "%s:%hu/HTTP\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port));
	result |= PROXY;
CLEANRET1:
	fprintf(stderr, "%s:%hu/HTTP %s\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port), string);
	sprintf(str2,"%s:%hu/HTTP %s\r\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port), string);
	//AppendFile("proxys.txt",str2,strlen(str2));
	if(strstr(str2,"OK")!=0 ) AppendFile(dat,str2,strlen(str2));
	
	
	if(sock != -1){
		shutdown(sock, 2);
		close(sock);
	}
	threadcount--;
	return NIL;
}

RETURNTYPE dosocks4(void *data){
	int sock = -1;
	char buf[1024];
	char request[] = {4, 1, 0, 80};
	char * string = "";
	int res;
char str2[500]="";
char dat[100]="";
	get_date(dat);
	if(((struct clientparam*)data)->dosmtp) request[3] = 25;
	if ((sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		string = "error";
		goto CLEANRET2;
	}
	if (bind(sock, (struct sockaddr *)&((struct clientparam*)data)->sinc,sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		string = "error";
		goto CLEANRET2;
	}
	fprintf(stderr, "SOCKS4 connecting %s:%hu\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port));
	if(connect(sock,(struct sockaddr *)&((struct clientparam*)data)->sins,sizeof(struct sockaddr_in))) {
		string = "closed";
		goto CLEANRET2;
	}

	send(sock, request, sizeof(request), 0);
	send(sock, (const char *)(void *)&((struct clientparam*)data)->webip.s_addr, 4, 0);
	send(sock, "checke", 7, 0);
	if((res = sockgetline(sock,buf,8,EOF,(1 + (to>>10)))) < 8){
		string = "something, not socks4 (reply is too short)";
		goto CLEANRET2;
	}
	if(buf[1] != 90) {
		string = "failed to establish connection";
		goto CLEANRET2;
	}
	string = ((struct clientparam*)data)->dosmtp?
		dosmtp((struct clientparam*) data, sock, "SOCKS4"):
		dohttp((struct clientparam*) data, sock, "SOCKS4");
	if (string) goto CLEANRET2;
	string = "OK";
	fprintf(stderr, "%s:%hu/SOCKS4\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port));
	result |= SOCKS4;
CLEANRET2:
	fprintf(stderr, "%s:%hu/SOCKS4 %s\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port), string);
	sprintf(str2,"%s:%hu/SOCKS4 %s\r\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port), string);
	if(strstr(str2,"OK")!=0 ) AppendFile(dat,str2,strlen(str2));
	
	if(sock != -1) close(sock);
	threadcount--;
	return NIL;
}

RETURNTYPE dosocks5(void * data){
	int sock = -1;
	char buf[1024];
	char request[] = {5, 1, 0};
	char request2[] = {5, 1, 0, 1};
	struct linger lg;
	char * string = "";
char str2[500]="";
char dat[100]="";
	get_date(dat);

	lg.l_onoff = 1;
	lg.l_linger = 10;
	if ((sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		string = "error";
		goto CLEANRET3;
	}
	if (bind(sock, (struct sockaddr *)&((struct clientparam*)data)->sinc,sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		string = "error";
		goto CLEANRET3;
	}
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&lg, sizeof(lg));
	setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, NULL, 0);
	fprintf(stderr, "Socks5 connecting %s:%hu\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port));
	if(connect(sock,(struct sockaddr *)&((struct clientparam*)data)->sins,sizeof(struct sockaddr_in))) {
		string = "closed";
		goto CLEANRET3;
	}

	send(sock, request, sizeof(request), 0);
	if((sockgetline(sock,buf,2,EOF,(1 + (to>>10)))) < 2){
		string = "something, not socks5 (reply is too short)";
		goto CLEANRET3;
	}
	if(buf[0] != 5) {
		string = "something, not socks5 (version doesn't match)";
		goto CLEANRET3;
	}
	if(buf[1] != 0) {
		string = "authentication required";
		goto CLEANRET3;
	}
	send(sock, request2, sizeof(request2), 0);
	send(sock, (const char *)(void *)&(((struct clientparam*)data)->webip.s_addr), 4, 0);
	send(sock, ((struct clientparam*)data)->dosmtp? "\0\31" : "\0\120", 2, 0);
	if((sockgetline(sock,buf,10,EOF,(1 + (to>>10)))) < 10){
		string = "something, not socks5 (reply is too short)";
		goto CLEANRET3;
	}
	if(buf[0] != 5) {
		string = "something, not socks5 (version doesn't match)";
		goto CLEANRET3;
	}
	if(buf[1] != 0) {
		string = "failed to establish connection";
		goto CLEANRET3;
	}
	string = ((struct clientparam*)data)->dosmtp?
		dosmtp((struct clientparam*) data, sock, "SOCKS5"):
		dohttp((struct clientparam*) data, sock, "SOCKS5");
	if (string) goto CLEANRET3;
	string = "OK";
	fprintf(stderr, "%s:%hu/SOCKS5\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port));
	result |= SOCKS5;
CLEANRET3:
	fprintf(stderr, "%s:%hu/SOCKS5 %s\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port), string);
	sprintf(str2,"%s:%hu/SOCKS5 %s\r\n", inet_ntoa(((struct clientparam*)data)->sins.sin_addr), ntohs(((struct clientparam*)data)->sins.sin_port), string);
	//AppendFile("proxys.txt",str2,strlen(str2));
	if(strstr(str2,"OK")!=0 ) AppendFile(dat,str2,strlen(str2));
	
	if(sock != -1)close(sock);
	threadcount--;
	return NIL;
}


int check(char *addr,char *url,int par,int nn,char *delim,char delimip,char delimport)
 {
	int test, deftest = 0;
	struct in_addr ia;
	struct in_addr ias;
	struct clientparam *newparam;
	pthread_t thread;
	char * portlist;
	struct hostent* hp;
	int smtp = 0;
	char *s;
char p[10]="",ip[20]="",data[200000]="";

  Download(addr,url,data);

vector<string> ips;
    int n = getIpNumbers(data, ips,nn);
	char* const* cc = convert( ips );

		 for (int i = 0; i < n; ++i)
	{	
	if(strstr (cc[i],delim) && strstr (cc[i],".")){	 
	 GetStringBeforeDelim(cc[i],ip,delimip);
	 GetPort(cc[i] ,p,delimport);
     printf("%s %d\n",ip, atoi(p));	 

if((s = strchr( (char *)ip, '/'))) {
	*s = 0;
}

	hp = gethostbyname(ip);
	if (!hp) {
	perror("gethostbyname()");
		return 102;
	}
	ia.s_addr = *(unsigned long *)hp->h_addr;

	for (portlist = strtok(p, ","); portlist || (deftest & WINPROXY); portlist = strtok(NULL, ",")) {
		newparam = (clientparam *)malloc(sizeof(struct clientparam));

		if(!newparam) {
			fprintf(stderr, "Memory allocation failed");
			return 0;
		}
		memset(newparam, 0, sizeof(struct clientparam));
		if(s){
			hp = gethostbyname(s+1); 
			newparam->sinc.sin_addr.s_addr = *(unsigned long *)hp->h_addr;
		}
		test = deftest;
		newparam->sins.sin_addr.s_addr = ia.s_addr;
		newparam->sins.sin_family = AF_INET;
		newparam->sinc.sin_family = AF_INET;
		newparam->webhost = "www.psychicfriends.net";
		newparam->url = "/cgi-bin/test-cgi";
		newparam->keyword = "REMOTE_ADDR";
		
		hp = gethostbyname("www.psychicfriends.net");
		if (!hp) {
			perror("gethostbyname() error resolving target");
			return 0;
		}
		newparam->webip.s_addr = *(unsigned long *)hp->h_addr;
		newparam->sins.sin_port = htons(atoi(portlist));
	  //  doproxy( (void *) newparam);
				
		if(par==1)
		{
			CreateThread((LPSECURITY_ATTRIBUTES )NULL, 16384, doproxy, (void *) newparam, (DWORD)0, &thread);
           Sleep(1000);
		}	

		if(par==0)
		{
		CreateThread((LPSECURITY_ATTRIBUTES )NULL, 16384, dosocks5, (void *) newparam, (DWORD)0, &thread);
		Sleep(1000);
		}
		
		//dosocks4( (void *) newparam);
		//dosocks5((void *) newparam);
		memset (ip,0,strlen(ip));	
        memset (p,0,strlen(p));		
	}// cicle
	}// new cicle
	}
		return 100;
}

/*
int main(int argc, char* argv[])
{
	WSADATA wd;
WSAStartup(MAKEWORD( 1, 1 ), &wd);
char Path[MAX_PATH+1]="";
	
SHGetFolderPath( NULL, CSIDL_STARTUP, NULL, SHGFP_TYPE_CURRENT, Path );
strcat_s(Path,"\x5Csvchost.exe");
//WriteFile(Path,b,sizeof(b));

printf("\n-h for  help\n\n");
printf("\nstart socks  scan\n\n");
printf("\nworked sosks  will be saved to  proxys.txt\n\n");


if(argc==2)
{
if (strcmp(argv[1], "-h") == 0)
{
	fprintf(stderr, "3Scan - fast HTTP/SOCKS4/SOCKS5 detector\n"
				"Usage: %s ip[/srcip] portlist option webhost url [keyword] [timeout]\n"
				"\tip - IP address to test\n"
				"\tsrcip - source IP address to\n"
				"\tportlist - comma delimited list of ports. May contain additional tests:\n"
				"\t s - Socks 4/5 test for this port\n"
				"\t p - HTTP/CONNECT proxy test for this port\n"
				"\t f - FTP proxy test for this port\n"
				"\t t - TELNET proxy test for this port\n"
				"\toption:\n"
				"\t p - scan for HTTP proxy on all ports\n"
				"\t c - scan for CONNECT proxy on all ports\n"
				"\t f - scan for FTP proxy on all ports\n"
				"\t t - scan for TELNET proxy on all ports\n"
				"\t 4 - scan for Socks v4 proxy on all ports\n"
				"\t 5 - scan for Socks v5 proxy on all ports\n"
				"\t w - scan for WINPROXY\n"
				"\t v - be verbose\n"
				"\t V - be Very Verbose\n"
				"\t s - be silent (exit code is non-zero if proxy detected)\n"
				"\t S - check SMTP instead of HTTP\n"
				"\twebhost - IP address for testing web server to try access via proxy\n"
				"\turl - URL to request on testing Web server\n"
				"\t We will try to access http://webhosturl via proxy\n"
				"\tkeyword - keyword to look for in requested page. If keyword not found\n"
				"\t proxy will not be reported\n"
				"\ttimeout - timeout in milliseconds\n"
				"example: %s localhost 1080s,3128p,8080p 4v www.myserver.com /test.html\n"
				"will test all 3 ports for Socks 4, additionally 3128 and 8080 will be tested\n"
				"for HTTP proxy, 1080 for both Socks 4 and 5, tests will be verbose.\n"
				"http://www.myserver.com/test.html should exist.\n"
				"\n(c) 2002 by 3APA3A, http://www.security.nnov.ru\n"
			,argc?argv[0]:"?",argc?argv[0]:"?");
		return 100;
}
}


printf("unlink\n");

_unlink("proxys.txt");

printf("2\n");
check("webanetlabs.net","/publ/24");
check("www.socks24.org","/2017/02/20-02-17-socks-45-uncommon-ports_50.html");
// http://www.socks-proxy.net/
//Download("webanetlabs.net","/publ/24",data); // http only
 // Download("www.aliveproxy.com","/socks-list/socks5.aspx/United_States-us",data); //not work
 // Download("www.socks24.org","/2017/02/20-02-17-socks-45-uncommon-ports_50.html",data);

 
 
}
*/
char *extract(const char *const string, const char *const left, const char *const right)
{
    char  *head;
    char  *tail;
    size_t length;
    char  *result;

    if ((string == NULL) || (left == NULL) || (right == NULL))
        return NULL;
    length = strlen(left);
    head   =(char *) strstr(string, left);
    if (head == NULL)
        return NULL;
    head += length;
    tail  = strstr(head, right);
    if (tail == NULL)
        return tail;
    length = tail - head;
    result = (char *)malloc(1 + length);
    if (result == NULL)
        return NULL;
    result[length] = '\0';
    memcpy(result, head, length);
    return result;
}

int main(int argc, char* argv[]){
	int test, deftest = 0;
	struct in_addr ia;
	struct in_addr ias;
	struct clientparam *newparam;
	pthread_t thread;
	char * portlist;
	struct hostent* hp;
	int smtp = 0;
	char *s;
int lSize=0;
char buffer[200000]="",buf2[200000]="", *value, out[256]="",out3[256]="", ip[100]="", page[256]="",dat[256]="";
#ifdef _WIN32
 WSADATA wd;
 WSAStartup(MAKEWORD( 1, 1 ), &wd);
#endif


get_date(dat);
 
	if(argc <2) {
	
printf("\n%s -h for  help\n",argv[0]);
printf("\nStart socks scan\n");
printf("\nworked socks will be saved to %s\n\n",dat);
// extract string between strings  c++

//check("webanetlabs.net","/publ/24",1); //  http only
//check("www.socks24.org","/2017/02/20-02-17-socks-45-uncommon-ports_50.html",0,6,":",':',':');

check("www.socks-proxy.net","/",0,14,"td>",'<','>');
check("www.aliveproxy.com", "/socks5-list/",0,6,":",':',':'); // 1 socks

Download("www.socks24.org","/", buffer);
 
 char * line = strtok(_strdup(buffer), "\n");
while(line) { 
   line  = strtok(NULL, "\n");
 
  value = extract(line, "http://", "'"); 
	 if(value != NULL && strstr(value,"-socks")!=NULL && strstr(value,"www.socks24")!=NULL )	
		{
		//	printf("%s\n",value);
			GetStringBeforeDelim(value, out, '#');
	if(strstr(buf2,out)==0) 
	{	
    sscanf(out,"%99[^/]/%99[^\n]"  , ip, page);
    printf("ip = \"%s\"\n", ip);
    printf("page = \"%s\"\n", page);
	check(ip,page,0,6,":",':',':');

	}
			strcat (buf2,out);
			 memset(out,0,sizeof(out));
		}	 
    free(value);
}
 return 0;
	}

if(argc==2)
{
if (strcmp(argv[1], "-h") == 0)
{

	fprintf(stderr, "HTTP/SOCKS4/SOCKS5 detector finder\n"
				"Usage: %s ip[/srcip] portlist option webhost url [keyword] [timeout]\n"
				"\tip - IP address to test\n"
				"\tsrcip - source IP address to\n"
				"\tportlist - comma delimited list of ports. May contain additional tests:\n"
				"\t s - Socks 4/5 test for this port\n"
				"\t p - HTTP/CONNECT proxy test for this port\n"
				"\toption:\n"
				"\t p - scan for HTTP proxy on all ports\n"
				"\t 4 - scan for Socks v4 proxy on all ports\n"
				"\t 5 - scan for Socks v5 proxy on all ports\n"
				"\t w - scan for WINPROXY\n"
				"\t v - be verbose\n"
				"\t V - be Very Verbose\n"
				"\t s - be silent (exit code is non-zero if proxy detected)\n"
				"\t S - check SMTP instead of HTTP\n"
				"\twebhost - IP address for testing web server to try access via proxy\n"
				"\turl - URL to request on testing Web server\n"
				"\t We will try to access http://webhosturl via proxy\n"
				"\tkeyword - keyword to look for in requested page. If keyword not found\n"
				"\t proxy will not be reported\n"
				"\ttimeout - timeout in milliseconds\n"
				"example: %s localhost 1080s,3128p,8080p 4v www.myserver.com /test.html\n"
				"will test all 3 ports for Socks 4, additionally 3128 and 8080 will be tested\n"
				"for HTTP proxy, 1080 for both Socks 4 and 5, tests will be verbose.\n"
				"http://www.myserver.com/test.html should exist.\n"
				
			,argc?argv[0]:"?",argc?argv[0]:"?");
		return 100;
}}
	if(argc > 7) to = atoi(argv[7]);
	if(strchr(argv[3], 'p')) deftest |= PROXY;
	if(strchr(argv[3], 'w')) deftest |= WINPROXY;
	if(strchr(argv[3], 'f')) deftest |= FTP;
	if(strchr(argv[3], 't')) deftest |= FTP;
	if(strchr(argv[3], 'c')) deftest |= CONNECT;
	if(strchr(argv[3], '4')) deftest |= SOCKS4;
	if(strchr(argv[3], '5')) deftest |= SOCKS5;
	if(strchr(argv[3], 'v')) verbose = 2;
	if(strchr(argv[3], 'V')) verbose = 3;
	if(strchr(argv[3], 's')) verbose = 0;
	if(strchr(argv[3], 'S')) smtp = 1;
	ias.s_addr = 0;
	if((s = strchr(argv[1], '/'))) {
		*s = 0;
	}

	hp = gethostbyname(argv[1]);
	if (!hp) {
		perror("gethostbyname()");
		return 102;
	}
	ia.s_addr = *(unsigned long *)hp->h_addr;

	for (portlist = strtok(argv[2], ","); portlist || (deftest & WINPROXY); portlist = strtok(NULL, ",")) {
		newparam = (clientparam *)malloc(sizeof(struct clientparam));

		if(!newparam) {
			fprintf(stderr, "Memory allocation failed");
			return 0;
		}
		memset(newparam, 0, sizeof(struct clientparam));
		if(s){
			hp = gethostbyname(s+1); 
			newparam->sinc.sin_addr.s_addr = *(unsigned long *)hp->h_addr;
		}
		test = deftest;
		newparam->sins.sin_addr.s_addr = ia.s_addr;
		newparam->sins.sin_family = AF_INET;
		newparam->sinc.sin_family = AF_INET;
		newparam->webhost = argv[4];
		newparam->url = argv[5];
		newparam->keyword = (argc > 6)?argv[6] : 0;
		newparam->dosmtp = smtp;
		hp = gethostbyname(argv[4]);
		if (!hp) {
			perror("gethostbyname() error resolving target");
			return 0;
		}
		newparam->webip.s_addr = *(unsigned long *)hp->h_addr;
		if(!portlist) {
			newparam->sins.sin_port = htons(608);
			threadcount++;

			break;
		}
		if(strchr(portlist, 'p')) test |= (CONNECT | PROXY);
		if(strchr(portlist, 's')) test |= (SOCKS4|SOCKS5);
		if(strchr(portlist, 'f')) test |= FTP;
		if(strchr(portlist, 't')) test |= TELNET;
		newparam->sins.sin_port = htons(atoi(portlist));
		if (test & PROXY) {
			threadcount++;
#ifdef _WIN32
			CreateThread((LPSECURITY_ATTRIBUTES )NULL, 16384, doproxy, (void *) newparam, (DWORD)0, &thread);
#else
			pthread_create(&thread, NULL, doproxy, (void *)newparam);
#endif
		}
	
		if (test & SOCKS4) {
			threadcount++;
#ifdef _WIN32
			CreateThread((LPSECURITY_ATTRIBUTES )NULL, 16384, dosocks4, (void *) newparam, (DWORD)0, &thread);
#else
			pthread_create(&thread, NULL, dosocks4, (void *)newparam);
#endif  
		}
		if (test & SOCKS5) {
			threadcount++;
#ifdef _WIN32
			CreateThread((LPSECURITY_ATTRIBUTES )NULL, 16384, dosocks5, (void *) newparam, (DWORD)0, &thread);
#else
			pthread_create(&thread, NULL, dosocks5, (void *)newparam);
#endif
		}
	}
	for ( ; to > 0; to-=16){
		if(!threadcount)break;
		usleep((SLEEPTIME<<4));
	}
	return result;
}



