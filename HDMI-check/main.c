#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "gpio.h"
#include "hdmi.h"
#include "uevent.h"

volatile sig_atomic_t running = 1;

void signal_handler(int sig)
{
    running = 0;
}

int main(void)
{
    int sock_fd;
    int recv_len;
    char buffer[4096];
    struct sigaction sa;

    setvbuf(stdout, NULL, _IOLBF, 0);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (gpio_init("/dev/gpiochip0", 26) != 0)
    {
        fprintf(stderr, "GPIO init failed\n");
        return 1;
    }

    sock_fd = uevent_init();

    if (sock_fd < 0)
    {
        fprintf(stderr, "uevent init failed\n");
        gpio_cleanup();
        return 1;
    }

    update_hdmi_status();

    printf("Waiting for HDMI hotplug event...\n");

    while (running)
    {
        recv_len = uevent_receive(sock_fd, buffer, sizeof(buffer));

        if (recv_len < 0)
        {
            if (errno == EINTR)
                continue;

            perror("uevent_receive");
            break;
        }

        if (is_hdmi_event(buffer, recv_len))
        {
            printf("HDMI event detected\n");
            update_hdmi_status();
        }
    }

    close(sock_fd);
    gpio_cleanup();

    printf("Program stopped\n");

    return 0;
}