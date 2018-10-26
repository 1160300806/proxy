#include <stdio.h>
#include <iostream>
#include <winsock2.h>
#include <stdlib.h>
#include <windows.h>
#include <string>
#include <string.h>
#include <process.h>
#include <thread>
#include <cstring>
#define HTTP_PORT 80
using namespace std;

char * host =new char[1024];
int nTimeout=1000;
struct HttpHeader
{
    char method[4] = {'\0'};  // POST or GET
    char url[1024] = {'\0'} ;  // 请求的url
    char host[1024] = {'\0'}; //目标主机
    char cookie[10240] = {'\0'};
    HttpHeader()
    {
        ZeroMemory(this,sizeof(HttpHeader));
    }
};
void string_replace(std::string &strBig, const std::string &strsrc, const std::string &strdst)
{
    std::string::size_type pos = 0;
    std::string::size_type srclen = strsrc.size();
    std::string::size_type dstlen = strdst.size();

    while( (pos=strBig.find(strsrc, pos)) != std::string::npos )
    {
        strBig.replace( pos, srclen, strdst );
        pos += dstlen;
    }
}
char* fish(char*buf,char*sourcehost,char*desthost)
{
    string strContent = buf;
    printf("asdf %s\n",strContent);
    if(strContent.find(sourcehost)!= std::string::npos )
    {
        string strsrc=sourcehost;
        string strdst=desthost;
        string_replace(strContent, strsrc, strdst);
        printf("asdf %s\n",strContent);
        if(strContent.find("Cookie:")!= std::string::npos)
        {
            size_t index=strContent.find("Cookie");
            strContent.erase(index,strContent.length()-index);
        }
        strcpy(buf,strContent.c_str());
    }
    return buf;
}
bool ConnectToServer(SOCKET *serverSocket,char *host)
{
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);
    HOSTENT *hostent = gethostbyname(host);
    if(!hostent)
    {
        return FALSE;
    }
    in_addr Inaddr=*( (in_addr*) *hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
    *serverSocket = socket(AF_INET,SOCK_STREAM,0);
    if(*serverSocket == INVALID_SOCKET)
    {
        return FALSE;
    }
    setsockopt(*serverSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&nTimeout, sizeof(int));
    setsockopt(*serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&nTimeout, sizeof(int));
    if(connect(*serverSocket,(SOCKADDR *)&serverAddr,sizeof(serverAddr))
            == SOCKET_ERROR)
    {
        closesocket(*serverSocket);
        return FALSE;
    }
    return TRUE;
}
void analyze(HttpHeader * httpHeader,char*buf)
{
    char *p;
    const char * delim = "\r\n";
    p = strtok(buf,delim);//提取第一行
    if(p[0] == 'G') //GET 方式
    {
        memcpy(httpHeader->method,"GET",3);
        memcpy(httpHeader->url,p+4,strlen(p)-13);
    }
    else if(p[0] == 'P')  //POST 方式
    {
        memcpy(httpHeader->method,"POST",4);
        memcpy(httpHeader->url,p+5,strlen(p)-14);
    }
    p = strtok(NULL,delim);
    while(p)
    {
        switch(p[0])
        {
        case 'H'://Host
            memcpy(httpHeader->host,p+6,strlen(p)-6);
            break;
        case 'C'://Cookie
            if(strlen(p) > 8)
            {
                char header[8];
                ZeroMemory(header,sizeof(header));
                memcpy(header,p,6);
                if(!strcmp(header,"Cookie"))
                {
                    memcpy(httpHeader->cookie,p+8,strlen(p)-8);
                }
            }
            break;
        default:
            break;
        }
        p = strtok(NULL,delim);
    }
}

void loop_thread(SOCKET* accept_socket,bool website_flag,bool client_flag,bool fish_flag)
{
    int buflen=0;
    SOCKET *serverSocket=new SOCKET;
    char buf[65500] = "\0";
    char buf2[65500]= "\0";
    if(*accept_socket!=INVALID_SOCKET)
    {
        buflen =recv(*accept_socket,buf,65500,0);
        cout << buf << endl;
        if(buflen == SOCKET_ERROR)
        {
            //printf("client recv failed\n");
            return ;
        }
    }
    //解析头部文件
    HttpHeader httpHeader=HttpHeader();
    char *CacheBuffer;
    CacheBuffer = new char[buflen + 1];
    ZeroMemory(CacheBuffer,buflen + 1);
    memcpy(CacheBuffer,buf,buflen);
    if(fish_flag==true)
    {
        CacheBuffer=fish(CacheBuffer,"jwts.hit.edu.cn","xg.hit.edu.cn");
    }
    analyze(&httpHeader,CacheBuffer);
    //printf("cachebuffer:%s\n",CacheBuffer);
    //printf("length:%d",sizeof(CacheBuffer));
    //网站过滤
    if(strcmp(httpHeader.host,"jwts.hit.edu.cn")==0&&website_flag==true)
    {
        printf("the website has been banned.\n");
        shutdown(*serverSocket,2);
        shutdown(*accept_socket,2);
        closesocket(*accept_socket);
        closesocket(*serverSocket);
        return;
    }
    //用户过滤
    if(strcmp(httpHeader.cookie,"name=value; JSESSIONID=c5LnbRGDs22shcqV7KG4kyX2Q2njNV8qTW3byBv9LCgkyH1N5JLK!-1891391065; clwz_blc_pst=201330860.24859")==0&&client_flag==true)
    {
        printf("the client has been banned.\n");
        shutdown(*serverSocket,2);
        shutdown(*accept_socket,2);
        closesocket(*accept_socket);
        closesocket(*serverSocket);
        return;
    }
    delete CacheBuffer;
    //与internet连接
    if (ConnectToServer(serverSocket,httpHeader.host))
    {
        std::cout << "Connect Succeed" << endl;
    }
    send(*serverSocket,buf,buflen,0);
    //接受来自internet的回应
    while(true)
    {
        int buflen2=0;
        buflen2 = recv(*serverSocket,buf2,65500,0);
        if(buflen2==SOCKET_ERROR || buflen2 == 0)
        {
            //printf("server recv failed\n");
            break;
        }
        else
        {
            send(*accept_socket,buf2,buflen2,0);
        }
    }
    shutdown(*serverSocket,2);
    shutdown(*accept_socket,2);
    //将回应发送给本地

    closesocket(*accept_socket);
    closesocket(*serverSocket);
    //delete serverSocket;
}
int main()
{
    WSADATA data;
    WORD w=MAKEWORD(2,2);
    WSAStartup(w,&data);
    SOCKET s=INVALID_SOCKET;
    s=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(10240);
    addr.sin_addr.S_un.S_addr=INADDR_ANY;
    bind(s,(sockaddr*)&addr,sizeof(addr));
    listen(s,5);
    printf("Start\n");
    while(true)
    {
        sockaddr_in addr2;
        int n=sizeof(addr2);
        SOCKET accept_socket=INVALID_SOCKET;
        accept_socket=accept(s,(sockaddr*)&addr2,&n);
        setsockopt(accept_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&nTimeout, sizeof(int));
        setsockopt(accept_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&nTimeout, sizeof(int));
        if(accept_socket!=INVALID_SOCKET)
        {
            printf("%connect succeed\n",inet_ntoa(addr2.sin_addr));
            std::thread t = std::thread(loop_thread,&accept_socket,false,false,true);
            t.join();
        }
        //接受来自本地的访问
        //else
            //printf("connect failed\n");
    }
    shutdown(s, 2);
    closesocket(s);
    WSACleanup();
}

