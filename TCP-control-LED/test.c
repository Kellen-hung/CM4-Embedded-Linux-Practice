#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(void)
{
    int server_fd;
    int client_fd;
    int recv_len;

    char buffer[100];

    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        printf("socket failed\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("bind failed\n");
        return 1;
    }

    if (listen(server_fd, 1) < 0)
    {
        printf("listen failed\n");
        return 1;
    }

    printf("Waiting for connection on port 5000...\n");

    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0)
    {
        printf("accept failed\n");
        return 1;
    }

    printf("Client connected!\n");

    while (1)
    {
        recv_len = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (recv_len <= 0)
        {
            printf("Client disconnected\n");
            break;
        }

        buffer[recv_len] = '\0';

        printf("Received: %s\n", buffer);
    }

    close(client_fd);
    close(server_fd);

    return 0;
}