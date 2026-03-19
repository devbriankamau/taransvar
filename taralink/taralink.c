#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <linux/netlink.h>

#include "mariadb/mysql.h"


#include <stdbool.h>
#include <sys/syscall.h>
#include <netdb.h>
#include <sys/types.h>

#include <netinet/in.h>


//#include <linux/byteorder/big_endian.h>
//#include <linux/byteorder/little_endian.h>
#include <stdint.h>

#define USE_MYSQL

#define C_BUFF_SIZE 4090
/* maximum payload size*/
#define MAX_PAYLOAD 1024 

#define NETLINK_USER 31

#define CONFIG_FILENAME "configfile.txt"

static clock_t lastPing = 0;
static char szWgetBuff[2000];

int configFileExists(void);
MYSQL *getConnection();

struct _SocketData {
  int sock_fd;
  struct msghdr msg;
  struct sockaddr_nl src_addr, dest_addr;
  struct nlmsghdr *nlh;
  struct iovec iov;
};

struct _SocketData *pSockData = NULL;   //Get rid of this....

enum et_wgetCategories {e_wget_assistanceRequest, e_wget_other};
typedef enum et_wgetCategories et_wgetCategories;
#define IPADDRESS(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]

struct _SocketData *getSockData();
int getKernelSocket(struct _SocketData *pSockData);
void sendMessage(struct _SocketData *pSockData, char *lpMsg);
int isConfigurationRequest(char *lpPayload, int *lpSequenceNumber);
void checkHackReports(void);
unsigned long minutesSincePing();
void setPing();
void checkReportStatus(char *lpPayload);
void addWarningRecord(char *szWarning); //Defined in module_functions.c
void checkRequestAssistance(void);
int addPendingWgetOk(et_wgetCategories eCategory, char *lpUrl, int nRegardingId);
char *wget(char *lpUrl, char *szBuff, int nBuffSize);
void handleTrafficReportFromKernel(char *lpPayload, int nDataLength);


#include "module_send_configuration.c"


#include "../tarakernel/module_globals.h" 

//*now compiling one by one..
#include "module_timer.c"
#include "module_request_assistance.c"
#include "module_hack_reports.c"
#include "module_traffic_report.c"
#include "module_msg_from_kernel.c"
#include "module_functions.c"
//*/



//#define UDP_PORT 5555         now using TARALINK_LISTENING_TO_PORT (tarakernel/module_globals.h)
#define NETLINK_USER 31   /* example only */

int fd = 0;

//New communication functions..
int create_netlink_socket(void);
int create_udp_socket(int port);
void handle_udp(int udp_fd);
void handle_netlink(int nl_fd);
int send_to_kernel(int fd, const void *data, size_t len);


//Compatibility functions....
void sendMessage(struct _SocketData *pSockData, char *lpMsg)
{
    send_to_kernel(fd, lpMsg, strlen(lpMsg));
}//sendMessage()


struct _SocketData *getSockData()
{
  struct _SocketData *pSockData = malloc(sizeof(struct _SocketData));
  //NOTE! Check if can have structure in _SocketData and not just the pointer...
  pSockData->nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
  return pSockData;
}

int getKernelSocket(struct _SocketData *pSockData)
{
  //ot:struct sockaddr_nl src_addr;
  int count=0;
  //int socket_fd;
  while ((pSockData->sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_USER)) < 0)
  {
    printf("Unable to open socket... You should confirm that Taransvar kernel module (\"tarakernel\") is running (\"sudo lsmod | grep tarakernel\").\nWaiting (%d)....\n", ++count);
    sleep(10);
  }

  memset(&pSockData->src_addr, 0, sizeof(pSockData->src_addr));
  pSockData->src_addr.nl_family = AF_NETLINK;
  pSockData->src_addr.nl_pid = getpid(); /* self pid */

  bind(pSockData->sock_fd, (struct sockaddr*)&pSockData->src_addr, sizeof(pSockData->src_addr));
  return pSockData->sock_fd;
}
MYSQL *getConnection()
{
      MYSQL *conn;
      conn = mysql_init(0);
	char *server ="localhost";
	char *user = "scriptUsrAces3f3";
	char *password = "rErte8Oi98e-2_#";
	char *database = "taransvar";
	  conn = mysql_init(NULL);

        if (configFileExists())
          return 0;

	  /* Connect to database */
	if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(conn));
		
	    printf("Unable to connect to DB. Aborting... \n");
	    exit(1);
	}
	return conn;
}

int configFileExists(void)
{
	FILE *file;

	if ((file = fopen(CONFIG_FILENAME, "r")))
	{
		fclose(file);
		return 1;
	}
	return 0;
}

unsigned long minutesSincePing()
{
  //  return 0;
        if (!lastPing)
            return 1000;
            
        unsigned long nSecSincePing = time(NULL) - lastPing;
        return (unsigned long) (nSecSincePing / 60);//(1000*60)); //(1000*1000*60));
}

void setPing()
{
      lastPing = time(NULL);//clock();
}

void testingTesting(char *lpPayload, int nSize)
{
    char cBuf[200];
    bufferToHex(lpPayload, nSize, cBuf, 200);
    printf("**** Received: %s\n", lpPayload);
    printf("%s\n", cBuf);
}

int isConfigurationRequest(char *lpPayload, int *lpSequenceNumber)
{
	char *lpSeparator;  
	char *lpSearchKey = "configuration ";

        //printf("Checking if conf request: %s\n", lpPayload);  

	//Check if it's module requesting configuration at format:
	//	configuration <batch number>?...
	lpSeparator = strstr(lpPayload, lpSearchKey);
	if (lpSeparator == lpPayload && *(lpPayload + strlen(lpPayload)-1) == '?')
	{
		//	#READ_CONFIGURATION
		//cReply[strlen(lpPayload)-1] = 0;
		*lpSequenceNumber = atoi(lpPayload+strlen(lpSearchKey));
		return 1;
	}
	return 0;
}	  

void checkReportStatus(char *lpPayload)
{
	if (strstr(lpPayload, "Prerout:"))
	{
		//Received statistics from tarakernel .. send it to global server every 15 minutes(?)
		unsigned long nMinutes = minutesSincePing(); 
		if (nMinutes >= 15)
		{
                        MYSQL *conn;
			MYSQL_RES *res = 0;
			MYSQL_ROW row;
			char cNickName[255];	
			int nThreadId;
			int bFoundData = 0;
			int bSetupOk = 1;

			conn = getConnection();
	      
			char *lpSQL = "select coalesce(systemNick, inet_ntoa('adminIP')), inet_ntoa(adminIP), inet_ntoa(globalDb1ip), inet_ntoa(globalDb2ip), inet_ntoa(globalDb3ip) from setup";
		
			if (mysql_query(conn, lpSQL)) {
			        lpSQL = "select systemNick from inet_ntoa(adminIP) from setup";
				if (mysql_query(conn, lpSQL)) {
			        	strcpy(cNickName, "Unable to read setup");
			        	bSetupOk = 0; //Don't read the result below....
			    } else 
				{
			        
					fprintf(stderr, "taralink: %s\n", mysql_error(conn));
            	    addWarningRecord("***** ERROR ***** taralink couldn't read setup");
            	    return;
				}
			}
			
			if (bSetupOk)
			{
				res = mysql_use_result(conn);
				if ((row = mysql_fetch_row(res)) != NULL)
				{
			        strcpy(cNickName, row[0]);
				} else {
			        strcpy(cNickName, "Unable to read setup");
				}
  		        mysql_free_result(res);
			}
	      
	        char *lpFound;
	        while ((lpFound = strchr(lpPayload, '\t')))
	            *lpFound = '_';
	        while ((lpFound = strchr(lpPayload, '\r')))
	            *lpFound = '_';
	        while ((lpFound = strchr(lpPayload, '\n')))
	            *lpFound = '_';
	    	while ((lpFound = strchr(lpPayload, ' ')))
	            *lpFound = '_';
	        setPing();
    	    char szUrl[255];
    	              
            CURL *payloadCurl = curl_easy_init();
            if(!payloadCurl) {
                printf("******* ERROR: curl_easy_init() returned false in checkReportStatus().. Aborting..\n");
                return;
            }
                      
            char *lpPayloadEncoded = curl_easy_escape(payloadCurl, lpPayload, strlen(lpPayload));
            if(!lpPayloadEncoded) {
                printf("******* ERROR: curl_easy_init() returned false in checkReportStatus().. Aborting..\n");
                return;
            }

            CURL *nickNamecurl = curl_easy_init();
            if(!nickNamecurl) {
                printf("******* ERROR: curl_easy_init() returned false in checkReportStatus().. Aborting..\n");
                return;
            }
                      
            char *lpNickNameUrlEncoded = curl_easy_escape(nickNamecurl, cNickName, strlen(cNickName));
            if(!lpNickNameUrlEncoded) {
                printf("******* ERROR: curl_easy_init() returned false in checkReportStatus().. Aborting..\n");
                return;
            }

            printf("\nSending status to global DB servers:\n");
            for (int n = 0; n < 3; n++)
            {
                char *lpGlobalDbIp = row[n+2];
                if (lpGlobalDbIp && strlen(lpGlobalDbIp) > 7)
                {
  		            sprintf(szUrl, "http://%s/script/config_update.php?f=ping&nick=%s&status=%s", lpGlobalDbIp, lpNickNameUrlEncoded, lpPayloadEncoded);
  		            *szWgetBuff = 0;
		            wget(szUrl, szWgetBuff, sizeof(szWgetBuff));  //Using global static buffers because reply doesn't come immediately.
		            printf("%s\n", szUrl);
		        } 
				else 
				{
					if (lpGlobalDbIp && *lpGlobalDbIp)
		            	printf("****** Skipping wrong IP address for global DB server: %s\n", lpGlobalDbIp);
      	                //addWarningRecord(conn, szBuf);
      	        }
		                  
            }
		    mysql_close(conn);
		      
		    curl_free(lpPayloadEncoded);
            curl_easy_cleanup(payloadCurl);
		    curl_free(lpNickNameUrlEncoded);
            curl_easy_cleanup(nickNamecurl);
	    }
 	    printf("Minutes (status): %lu (%s)\n", nMinutes, szWgetBuff);
	}
	else {
	          addWarningRecord("Trying to send status to DB server but it has wrong format...");
	}
}







//New communication functions
int create_udp_socket(int port)
{
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket(AF_INET)");
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind(udp)");
        close(fd);
        return -1;
    }

    return fd;
}

int create_netlink_socket(void)
{
//    int fd;   - global variable instead...
    struct sockaddr_nl addr;

    //fd = socket(AF_NETLINK, SOCK_RAW, NL_PROTO);
    fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	
    if (fd < 0) {
        perror("socket(AF_NETLINK)");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();   /* userspace pid */
    addr.nl_groups = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind(netlink)");
        close(fd);
        fd = 0;
        return -1;
    }

    return fd;
}

void handle_udp(int udp_fd)
{
    //#define UDP_MSG_PREFIX "UDP_JSON:"    Defined in tarakernel/module_globals.h
    char buf[2048+strlen(UDP_MSG_PREFIX)];
    struct sockaddr_in src;
    socklen_t slen = sizeof(src)-strlen(UDP_MSG_PREFIX);
    strcpy(buf, UDP_MSG_PREFIX);

    int n = recvfrom(udp_fd, buf + strlen(UDP_MSG_PREFIX), sizeof(buf) - 1, 0, (struct sockaddr *)&src, &slen);
    if (n < 0) {
        perror("recvfrom");
        return;
    }

    buf[n+strlen(UDP_MSG_PREFIX)] = '\0';

    printf("\n************************ WARNING ***************************\n\nUDP from %s:%d -> %s\n\n",
           inet_ntoa(src.sin_addr),
           ntohs(src.sin_port),
           buf);

    send_to_kernel(fd, buf, strlen(buf) + 1);
}

void interpretFromKernel(char *lpPayload, int nDataLength);
void interpretFromKernel(char *lpPayload, int nDataLength)
{
   	int nSequenceNumber = -1;   //Implemented in case needs to send in several portions... But that's not an issue yet...

    //printf("About to interpret message from kernel: %s", lpPayload);

    if (isConfigurationRequest(lpPayload, &nSequenceNumber))
    {
        int bReadChangesOnly;

		printf("Configuration requested..\n");
		sentConfiguration(pSockData, nSequenceNumber, 1, bReadChangesOnly = 0);
	} 
    else
	{
	    //This is better way to find keyword at start....
	    char *lpSeparator = strchr(lpPayload, '|');
	    char cKeyword[20];
        if (lpSeparator && lpSeparator - lpPayload < sizeof(cKeyword))
	    {
	       	strncpy(cKeyword, lpPayload, lpSeparator - lpPayload);
	       	cKeyword[lpSeparator - lpPayload] = 0;  //Terminate the string.
	    }
	    else
	       	*cKeyword = 0;
	        
	    //Check if it's status report from Taransvar kernel module
	    //char *lpSearchKey = "status|";
		//lpSeparator = strstr(lpPayload, lpSearchKey);
		//if (lpSeparator == lpPayload)
		if (!strcmp(cKeyword, "status"))
		{
		   	char *lpStatus = lpSeparator+1;//lpPayload+strlen(lpSearchKey); 
			printf("%s\n", lpStatus);
			
			checkReportStatus(lpStatus);
		}
		else
		{
		    if (!strcmp(cKeyword, "TRAFFIC"))
		    {
                char cBuf[400];	//was 200
                int nTrafficLen = strlen(lpSeparator+1); 
                //bufferToHex((char*)lpPayload, (nDataLength>60?60:nDataLength), cBuf, 200);
                //printf("**** Traffic hex dump: %s\n", cBuf);
                strncpy(cBuf, lpSeparator+1, (nTrafficLen>250?250:nTrafficLen+1));	//was (nTrafficLen>50?50:nTrafficLen+1));
                if (nTrafficLen > 250)	//was 50
                {
                    sprintf(cBuf+250," *** truncated, len: %d *** ", nTrafficLen);	//was 50
                    strcpy(cBuf+strlen(cBuf), lpSeparator+1+nTrafficLen-50);
                }

                printf("**** Traffic received: %s\n", cBuf);//lpSeparator+1);
		        
		        //handleTrafficReportFromKernel(lpPayload+strlen(lpSearchKey), nDataLength - strlen(lpSearchKey));
		        handleTrafficReportFromKernel(lpSeparator+1, nDataLength - (strlen(cKeyword)+1));
		    }
    		else 
                if (!strcmp(cKeyword, "CHECK"))
		        {
    		        checkIpAddresses(lpSeparator+1, nDataLength - (strlen(cKeyword)+1));
    		    }
    		    else 
                    if (!strcmp(cKeyword, "DUMMY"))
    		        {
    		          testingTesting(lpSeparator+1, nDataLength - (strlen(cKeyword)+1));
    	        	}
	        		else
	              		printf("Unhandled msg (keyword: %s) from kernel (%d bytes): %s\n", cKeyword, nDataLength, lpPayload);
		}
	}	
}//interpretFromKernel()


void handle_netlink(int nl_fd)
{
    char buf[4096];
    struct sockaddr_nl src_addr;
    struct iovec iov = {
        .iov_base = buf,
        .iov_len = sizeof(buf),
    };
    struct msghdr msg = {
        .msg_name = &src_addr,
        .msg_namelen = sizeof(src_addr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    int nDataLength = recvmsg(nl_fd, &msg, 0);
    if (nDataLength < 0) {
        perror("recvmsg(netlink)");
        return;
    }

    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    //printf("Netlink message received: %d bytes\n", nDataLength);
    //printf("nlmsg_len=%u nlmsg_pid=%u nlmsg_type=%u nlmsg_flags=0x%x\n", nlh->nlmsg_len, nlh->nlmsg_pid, nlh->nlmsg_type, nlh->nlmsg_flags);

    if (NLMSG_OK(nlh, nDataLength)) {

        int payload_len = NLMSG_PAYLOAD(nlh, 0);
        char *payload = (char *)NLMSG_DATA(nlh);

        //printf("payload length = %d\n", payload_len);
        //printf("payload = %.*s\n", payload_len, payload); 
        
        interpretFromKernel(payload, payload_len);
        //asdf 

    }
    else   
        printf("****** ERROR ****** Payload is not ok!");

    //printf("Netlink message received: %d bytes: \n%s\n", n, buf);
    /* parse nlmsghdr here */
}


int send_to_kernel(int fd, const void *data, size_t len)
{
    struct sockaddr_nl dest_addr;
    struct nlmsghdr *nlh;
    struct iovec iov;
    struct msghdr msg;
    int ret;

    nlh = malloc(NLMSG_SPACE(len));
    if (!nlh)
        return -1;

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;      /* kernel */
    dest_addr.nl_groups = 0;   /* unicast */

    memset(nlh, 0, NLMSG_SPACE(len));
    nlh->nlmsg_len = NLMSG_SPACE(len);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    memcpy(NLMSG_DATA(nlh), data, len);

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    ret = sendmsg(fd, &msg, 0);
    if (ret < 0)
    {
        perror("sendmsg");
        printf("\n***** ERROR sending message to kernel: %s\n", (char*)data);
    }

    free(nlh);
    printf("\n***** Probably successfull sending message to kernel: %s\n", (char*)data);
    return ret;
}



int main(void)
{
    pSockData = getSockData();  //This is no longer in use....

	init_timer();

    int nl_fd = create_netlink_socket();
    int udp_fd = create_udp_socket(TARALINK_LISTENING_TO_PORT);

    if (nl_fd < 0 || udp_fd < 0) {
        if (nl_fd >= 0) close(nl_fd);
        if (udp_fd >= 0) close(udp_fd);
        return 1;
    }

    printf("Listening on netlink fd=%d and UDP port %d fd=%d\n", nl_fd, TARALINK_LISTENING_TO_PORT, udp_fd);

    const char *text = "hello kernel";

    int rc = send_to_kernel(nl_fd, text, strlen(text) + 1);

    if (rc >= 0)
        printf("%s - sent to kernel\n", text);
    else
        printf("Unable to send message to kernel\n");

    while (1) {
        fd_set rfds;
        int maxfd;
        int ret;

        FD_ZERO(&rfds);
        FD_SET(nl_fd, &rfds);
        FD_SET(udp_fd, &rfds);

        maxfd = (nl_fd > udp_fd) ? nl_fd : udp_fd;

        ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (FD_ISSET(nl_fd, &rfds))
            handle_netlink(nl_fd);

        if (FD_ISSET(udp_fd, &rfds))
            handle_udp(udp_fd);
    }

    close(nl_fd);
    close(udp_fd);
    return 0;
}