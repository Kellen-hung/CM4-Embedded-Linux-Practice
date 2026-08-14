#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "uevent.h"

int uevent_init(void)
{
    int sock_fd;
    struct sockaddr_nl addr;

    sock_fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

    if (sock_fd < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1;

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

int uevent_receive(int sock_fd, char *buffer, int size)
{
    int recv_len;

    recv_len = recv(sock_fd, buffer, size - 1, 0);

    if (recv_len < 0)
        return -1;

    buffer[recv_len] = '\0';

    return recv_len;
}

int is_hdmi_event(char *buffer, int len)
{
    int i = 0;
    int is_drm = 0;
    int is_hotplug = 0;

    while (i < len)
    {
        if (strcmp(&buffer[i], "SUBSYSTEM=drm") == 0)
            is_drm = 1;

        if (strcmp(&buffer[i], "HOTPLUG=1") == 0)
            is_hotplug = 1;

        i += strlen(&buffer[i]) + 1;
    }

    return is_drm && is_hotplug;
}