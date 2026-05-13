#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <linux/if.h> 
#include <sys/socket.h>

char *get_ipstr_by_ifname(char *ifname,char *ipstr){
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char *addr = NULL;

    if(ipstr == NULL){
        perror("ipstr is null");
        return NULL;
    }
    // 获取所有网络接口信息
    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        return NULL;
    }

    // 遍历所有接口
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        // 跳过非 IPv4 地址或无地址的接口
        if (ifa->ifa_addr == NULL)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        // 过滤回环接口（可选）
        if (!(ifa->ifa_flags & IFF_LOOPBACK)) {
            sa = (struct sockaddr_in *)ifa->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
            printf("netcard: %s, IP addr: %s\n", ifa->ifa_name, addr);
			if(ifa->ifa_name != NULL && strcmp(ifa->ifa_name,ifname) == 0){
				strcpy(ipstr,addr);
                break;
			}
        }
    }
    // 释放资源
    freeifaddrs(ifap);
    return addr;
}


char *get_default_gw_ipstr_by_curip(char *ip_str){
    if (ip_str == NULL) {
        fprintf(stderr, "input ip_str is null\n");
        return NULL;
    }
    char *last_dot = strrchr(ip_str, '.');
    if (last_dot == NULL) {
        fprintf(stderr, "invalid ip str\n");
        return NULL;
    }

    strcpy(last_dot + 1, "1");
    return ip_str;
}