#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <gpiod.h>
#include <signal.h>
#include <errno.h>

volatile sig_atomic_t running = 1;

struct gpiod_line_request *gpio_request;

void signal_handler(int sig)
{
    running = 0;
}

int gpio_init(const char *chip_path, unsigned int gpio)
{
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;

    chip = gpiod_chip_open(chip_path);
    if (!chip)
    {
        printf("gpiod_chip_open() failed\n");
        
        return -1;
    }
        

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();

    if (!settings || !line_cfg)
    {
        printf("gpiod_line_settings_new() or gpiod_line_config_new() failed\n");
        
        return -1;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    if (gpiod_line_config_add_line_settings(line_cfg, &gpio, 1, settings))
    {
        printf("gpiod_line_config_add_line_settings() failed\n");
        
        return -1;
    }

    gpio_request = gpiod_chip_request_lines(chip, NULL, line_cfg);

    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    if (!gpio_request)
    {
        printf("gpiod_chip_request_lines() failed\n");
        
        return -1;
    }

    return 0;
}

void gpio_write(unsigned int gpio, int value)
{
    if (value)
        gpiod_line_request_set_value(gpio_request, gpio, GPIOD_LINE_VALUE_ACTIVE);
    else
        gpiod_line_request_set_value(gpio_request, gpio, GPIOD_LINE_VALUE_INACTIVE);
}

void update_hdmi_status(void)
{
    FILE *fp;
    char status[32];

    fp = fopen("/sys/class/drm/card1-HDMI-A-1/status", "r");

    if (!fp)
    {
        printf("Failed to open HDMI status\n");
        
        return;
    }

    if (fgets(status, sizeof(status), fp))
    {
        status[strcspn(status, "\r\n")] = '\0';

        if (strcmp(status, "connected") == 0)
        {
            printf("HDMI connected\n");
            
            gpio_write(26, 1);
        }
        else if (strcmp(status, "disconnected") == 0)
        {
            printf("HDMI disconnected\n");
            
            gpio_write(26, 0);
        }
    }

    fclose(fp);
}

int is_hdmi_event(char *buffer, int len)
{
    int i = 0;
    int is_drm = 0;
    int is_hotplug = 0;

    while (i < len)
    {
        // printf("buffer: %s\n", &buffer[i]);
        if (strcmp(&buffer[i], "SUBSYSTEM=drm") == 0)
            is_drm = 1;

        if (strcmp(&buffer[i], "HOTPLUG=1") == 0)
            is_hotplug = 1;

        i += strlen(&buffer[i]) + 1;
    }

    return is_drm && is_hotplug;
}

int main(void)
{
    int sock_fd;
    int recv_len;
    char buffer[4096];

    struct sockaddr_nl addr;

    setvbuf(stdout, NULL, _IOLBF, 0);

    // signal
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // gpio
    if (gpio_init("/dev/gpiochip0", 26) != 0)
    {
        printf("GPIO init failed\n");
        
        return 1;
    }

    // kernel - userspace socket
    sock_fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

    if (sock_fd < 0)
    {
        printf("socket failed\n");
        
        return 1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1;

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("bind failed\n");
        return 1;
    }

    // first read status (power on)
    update_hdmi_status();

    printf("Waiting for HDMI hotplug event...\n");
    

    while (running)
    {
        recv_len = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
        // printf("running: %d\n", running);

        if (recv_len < 0)
        {
            if (errno == EINTR)
                continue;

            perror("recv");
            break;
        }

        buffer[recv_len] = '\0';

        if (is_hdmi_event(buffer, recv_len))
        {
            printf("HDMI event detected\n");
            
            update_hdmi_status();
        }
    }

    gpio_write(26, 0);

    close(sock_fd);

    if (gpio_request)
    {
        gpiod_line_request_release(gpio_request);
    }

    printf("\nProgram stopped!\n");
    

    return 0;
}