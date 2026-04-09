//soc_syslog.c
//Functions for receiving syslog (UDP 514) messages from firewalls or others. 

//To test:  echo '<34>1 2026-04-09T08:33:12Z firewall1 app1 1234 ID47 blocked connection from 1.2.3.4' | nc -u 10.10.10.10 514

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_SOC_MSG 4096

typedef struct
{
    char szSenderIp[INET_ADDRSTRLEN];
    unsigned short nSenderPort;

    int nPri;
    int nFacility;
    int nSeverity;

    char szHostname[256];
    char szTag[128];
    char szMessage[2048];
    char szRaw[MAX_SOC_MSG];

    int bIsSyslog;
} SOC_SYSLOG_MSG;


/* --------------------------------------------------------- */
/* Helpers                                                   */
/* --------------------------------------------------------- */

static void initSocSyslogMsg(SOC_SYSLOG_MSG *pMsg)
{
    memset(pMsg, 0, sizeof(*pMsg));
    pMsg->nPri = -1;
    pMsg->nFacility = -1;
    pMsg->nSeverity = -1;
}

static void trimCrlf(char *s)
{
    if (!s)
        return;

    size_t len = strlen(s);

    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
        len--;
    }
}

static int parseSyslogPri(const char *lpMsg, int *pnPri, const char **lpAfterPri)
{
    if (!lpMsg || lpMsg[0] != '<')
        return 0;

    const char *p = lpMsg + 1;
    int pri = 0;

    if (!isdigit((unsigned char)*p))
        return 0;

    while (*p && isdigit((unsigned char)*p))
    {
        pri = pri * 10 + (*p - '0');
        p++;
    }

    if (*p != '>')
        return 0;

    if (pnPri)
        *pnPri = pri;

    if (lpAfterPri)
        *lpAfterPri = p + 1;

    return 1;
}


static void copy_trunc(char *dst, size_t dst_sz, const char *src)
{
    //Just to avoid message:  warning: ‘%s’ directive output may be truncated writing up to 4095 bytes into a region of size 256 [-Wformat-truncation=]
    if (!dst || dst_sz == 0)
        return;

    if (!src)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

/*
 * Very lightweight syslog decoder.
 *
 * Handles:
 *   RFC3164-ish:
 *     <34>Oct 11 22:14:15 firewall1 sshd: Failed password for root
 *
 *   RFC5424-ish:
 *     <34>1 2026-04-09T08:33:12Z firewall1 appname 1234 ID47 message text
 */
static void decodeSocSyslog(const char *lpRaw, SOC_SYSLOG_MSG *pOut)
{
    const char *p = NULL;
    int pri = -1;

    if (!parseSyslogPri(lpRaw, &pri, &p))
    {
        snprintf(pOut->szMessage, sizeof(pOut->szMessage), "%s", lpRaw);
        return;
    }

    pOut->bIsSyslog = 1;
    pOut->nPri = pri;
    pOut->nFacility = pri / 8;
    pOut->nSeverity = pri % 8;

    /* ---------- Try RFC5424-ish first ---------- */
    if (isdigit((unsigned char)*p))
    {
        char szVersion[16]   = {0};
        char szTimestamp[64] = {0};
        char szHostname[256] = {0};
        char szApp[128]      = {0};
        char szProcId[64]    = {0};
        char szMsgId[64]     = {0};
        char szRest[2048]    = {0};

        int n = sscanf(p,
                       "%15s %63s %255s %127s %63s %63s %2047[^\n]",
                       szVersion,
                       szTimestamp,
                       szHostname,
                       szApp,
                       szProcId,
                       szMsgId,
                       szRest);

        if (n >= 6)
        {
            snprintf(pOut->szHostname, sizeof(pOut->szHostname), "%s", szHostname);
            snprintf(pOut->szTag, sizeof(pOut->szTag), "%s", szApp);

            if (n == 7)
                snprintf(pOut->szMessage, sizeof(pOut->szMessage), "%s", szRest);

            return;
        }
    }

    /* ---------- Fallback RFC3164-ish ---------- */
    {
        char szCopy[MAX_SOC_MSG];
        snprintf(szCopy, sizeof(szCopy), "%s", p);

        char *lpCursor = szCopy;

        while (*lpCursor == ' ')
            lpCursor++;

        /*
         * Skip standard RFC3164 timestamp "Oct 11 22:14:15"
         * which is normally 15 chars.
         */
        if (strlen(lpCursor) > 15)
        {
            lpCursor += 15;
            while (*lpCursor == ' ')
                lpCursor++;
        }

        /* hostname */
        char *lpHost = lpCursor;
        while (*lpCursor && *lpCursor != ' ')
            lpCursor++;

        if (*lpCursor)
        {
            *lpCursor = '\0';
            copy_trunc(pOut->szHostname, sizeof(pOut->szHostname), lpHost);
            lpCursor++;
        }

        while (*lpCursor == ' ')
            lpCursor++;

        /* tag/app up to ':' or space */
        char *lpTag = lpCursor;
        while (*lpCursor && *lpCursor != ':' && *lpCursor != ' ')
            lpCursor++;

        char cSaved = *lpCursor;
        *lpCursor = '\0';
        copy_trunc(pOut->szTag, sizeof(pOut->szTag), lpTag);
        *lpCursor = cSaved;

        while (*lpCursor && *lpCursor != ':')
            lpCursor++;

        if (*lpCursor == ':')
            lpCursor++;

        while (*lpCursor == ' ')
            lpCursor++;

        copy_trunc(pOut->szMessage, sizeof(pOut->szMessage), lpCursor);
    }
}

static void printSocSyslogMsg(const SOC_SYSLOG_MSG *pMsg)
{
    printf("\n[SOC SYSLOG] ----------------------------------------\n");
    printf("[SOC SYSLOG] From      : %s:%u\n", pMsg->szSenderIp, pMsg->nSenderPort);
    printf("[SOC SYSLOG] IsSyslog  : %s\n", pMsg->bIsSyslog ? "yes" : "no");
    printf("[SOC SYSLOG] PRI       : %d\n", pMsg->nPri);
    printf("[SOC SYSLOG] Facility  : %d\n", pMsg->nFacility);
    printf("[SOC SYSLOG] Severity  : %d\n", pMsg->nSeverity);
    printf("[SOC SYSLOG] Hostname  : %s\n", pMsg->szHostname[0] ? pMsg->szHostname : "(none)");
    printf("[SOC SYSLOG] Tag       : %s\n", pMsg->szTag[0] ? pMsg->szTag : "(none)");
    printf("[SOC SYSLOG] Message   : %s\n", pMsg->szMessage[0] ? pMsg->szMessage : "(none)");
    printf("[SOC SYSLOG] Raw       : %s\n", pMsg->szRaw);
}


/* --------------------------------------------------------- */
/* Main handler                                              */
/* --------------------------------------------------------- */

void handle_soc_syslogmsg(int soc_fd)
{
    char szBuf[MAX_SOC_MSG];
    struct sockaddr_in srcAddr;
    socklen_t nSrcLen = sizeof(srcAddr);

    ssize_t nRead = recvfrom(soc_fd,
                             szBuf,
                             sizeof(szBuf) - 1,
                             0,
                             (struct sockaddr *)&srcAddr,
                             &nSrcLen);

    if (nRead < 0)
    {
        if (errno != EINTR)
            perror("recvfrom(syslog)");
        return;
    }

    szBuf[nRead] = '\0';
    trimCrlf(szBuf);
    printf("Received: %s\n", szBuf);

    SOC_SYSLOG_MSG msg;
    initSocSyslogMsg(&msg);

    inet_ntop(AF_INET, &srcAddr.sin_addr, msg.szSenderIp, sizeof(msg.szSenderIp));
    msg.nSenderPort = ntohs(srcAddr.sin_port);

    snprintf(msg.szRaw, sizeof(msg.szRaw), "%s", szBuf);

    decodeSocSyslog(szBuf, &msg);

    printSocSyslogMsg(&msg);

    /*
     * Hook 1: Save to DB
     *
     * Example:
     * insertSocSyslog(conn,
     *                 msg.szSenderIp,
     *                 msg.nSenderPort,
     *                 msg.nPri,
     *                 msg.nFacility,
     *                 msg.nSeverity,
     *                 msg.szHostname,
     *                 msg.szTag,
     *                 msg.szMessage,
     *                 msg.szRaw);
     */

    /*
     * Hook 2: Decide whether to notify kernel
     *
     * Example:
     * if (strstr(msg.szMessage, "blocked") || strstr(msg.szMessage, "attack"))
     * {
     *     char szKernelMsg[512];
     *     snprintf(szKernelMsg, sizeof(szKernelMsg),
     *              "SOC_LOG %s PRI=%d MSG=%s",
     *              msg.szSenderIp, msg.nPri, msg.szMessage);
     *
     *     send_to_kernel(g_nl_fd, szKernelMsg, strlen(szKernelMsg) + 1);
     * }
     */
}