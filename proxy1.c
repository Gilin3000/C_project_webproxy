#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *proxy_port;

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void make_request_to_server(int ptsfd, char *url, char *host, char *port, char *method, char *version, char *filename);

int main(int argc, char **argv)
{
    int listenfd, connfd; // 듣기소켓, 연결소켓 받아오기
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    if (argc != 2) // 에러검출
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    proxy_port = argv[1];

    while (1)
    {
        clientlen = sizeof(clientaddr);
        // Accept 실행 > 연결 받음 > 트랜잭션 수행 > 연결 닫음. (계속해서 반복)
        // accept함수를 통해 연결 요청한 clientfd와 연결
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // Getnameinfo : ip 주소를 도메인주소로 바꿔줌
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        // client에서 받은 요청을 처리하는 doit 함수 진행
        //  한 개의 HTTP 트랜잭션을 처리
        //  요청 받은 데이터를 클라이언트에게 보내줌
        doit(connfd);
        Close(connfd);
    }
}
void doit(int fd)
{
    // 프록시는 get요청만 받으면 됨. 정적인지 동적인지 몰라도됨 서버입장에서 프록시는 클라이언트 임. is_static 필요X
    struct stat sbuf; // (필요함? 일단 가져옴)
    // sbuf 에 메타데이터 정보가 저장됨 (필요함)
    //
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], url[MAXLINE], version[MAXLINE], port[MAXLINE], host[MAXLINE], filename[MAXLINE];
    char response[MAX_OBJECT_SIZE];
    rio_t client_rio, server_rio;
    char *p;

    Rio_readinitb(&client_rio, fd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s\n", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    strcpy(url, uri);
    if ((p = strchr(uri, '/')))
    {
        *p = '\0';
        sscanf(p + 2, "%s", host);
    }
    else
    {
        strcpy(host, uri);
    }

    if ((p = strchr(host, ':')))
    {
        *p = '\0';
        sscanf(host, "%s", host);
        sscanf(p + 1, "%s", port);
    }

    if ((p = strchr(port, '/')))
    {
        *p = '\0';
        sscanf(port, "%s", port);
        sscanf(p + 1, "%s", filename);
    }

    if (strcasecmp(method, "GET") != 0)
    {
        sprintf(buf, "GET요청이 아닙니다.\r\n");
        Rio_writen(fd, buf, strlen(buf));
        return;
    }

    int ptsfd = Open_clientfd(host, port);
    Rio_readinitb(&server_rio, ptsfd);

    printf("=======Receive Request From Client=======\n");
    read_requesthdrs(&client_rio);
    printf("=======Send Request To Server=======\n");
    make_request_to_server(ptsfd, url, host, port, method, version, filename);
    printf("=======Receive Request To Server=======\n");
    Rio_readnb(&server_rio, response, MAX_OBJECT_SIZE);
    printf("%s", response);
    printf("=======Send Response To Client=======\n");
    Rio_writen(fd, response, MAX_OBJECT_SIZE);
    printf("%s", response);
    Close(ptsfd);
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE); // 텍스트 줄을 rp에서 읽고 buf에 복사한 후 널 문자로 종료시킨다. 최대 MAXLINE - 1개의 바이트를 읽는다.
    printf("%s", buf);
    while (strcmp(buf, "\r\n"))
    { // 첫번째 매개변수(buf)와 두번째 매개변수('\r\n')가 같을 경우 0을 반환
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

void make_request_to_server(int ptsfd, char *url, char *host, char *port, char *method, char *version, char *filename)
{
    char buf[MAXLINE];

    if (strlen(filename) == 0)
    {
        strcpy(url, "/\n");
    }
    else
    {
        strcpy(url, "/");
        strcat(url, filename);
    }
    // printf("url : %s \n", url);
    sprintf(buf, "%s %s %s\r\n", method, url, version);
    sprintf(buf, "%sHost: %s:%s\r\n", buf, host, port);
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);
    Rio_writen(ptsfd, buf, strlen(buf));
    printf("%s", buf);
}