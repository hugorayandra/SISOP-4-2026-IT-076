#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

int main()
{
    int sock;

    struct sockaddr_in server_addr;

    char buffer[4096];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock < 0)
    {
        printf("Socket Error\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr)) < 0)
    {
        printf("Connection Failed\n");
        return 1;
    }

    printf("CONNECTED TO SERVER\n");

    while(1)
    {
        memset(buffer, 0, sizeof(buffer));

        fgets(buffer, sizeof(buffer), stdin);

        send(sock, buffer, strlen(buffer), 0);

        memset(buffer, 0, sizeof(buffer));

        recv(sock, buffer, sizeof(buffer), 0);

        printf("%s\n", buffer);
    }

    close(sock);

    return 0;
}
