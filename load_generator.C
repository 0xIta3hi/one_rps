#include<stdio.h>
#include<time.h>
#include<stdbool.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<string.h>

bool send_request(int sock){
    char *request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    char buf[1024] = {0};
    if(send(sock, request, strlen(request), 0) < 0){
        return false;
    }

    if(recv(sock, buf, sizeof(buf),0) < 0){
        return false;
    }
    return true;
    usleep(1);
}


int main(){
    struct timespec start, current;

}