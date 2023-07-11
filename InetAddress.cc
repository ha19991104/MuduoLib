#include "InetAddress.h"
#include<strings.h> //bzero
#include<string.h> //strlen

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_,sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());  //inet_addr将字符串转成整数ip地址.分10进制，再就接着转成网络字节序
}
std::string InetAddress::toIp() const{
    char buf[64]={0};
    ::inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));
    return buf;
}
std::string InetAddress::toIpPort() const
{
   char buf[64]={0};
   /*
    关于inet_pton 和 inet_ntop
    #include<arpe/inet.h>
    int inet_pton(int family, const char *strptr, void *addrptr); //将点分十进制的ip地址转换为用于网络传输的数值格式
    // 成功返回1，如果不是有效表达式返回0，出错返回-1
    const char * inet_ntop(int family, const void *addrptr, char *strptr, size_t len); // 将用于网络传输的数值格式转换为10进制的ip格式
    //成功返回指向结构体的指针，出错返回NULL
   */
    ::inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf+end,":%u",port);
    return buf;

}
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

// #include<iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout<<addr.toIpPort()<<std::endl;
//     return 0;
// }