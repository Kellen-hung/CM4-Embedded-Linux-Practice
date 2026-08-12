#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <gpiod.h>

struct gpiod_line_request *gpio_request;

char FeedBack_Message[100] = "";

int gpio_init(const char *chip_path, unsigned int gpio)
{
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;

    chip = gpiod_chip_open(chip_path);
    if (!chip)
        return -1;

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();

    if (!settings || !line_cfg)
        return -1;

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    if (gpiod_line_config_add_line_settings(line_cfg, &gpio, 1, settings))
        return -1;

    gpio_request = gpiod_chip_request_lines(chip, NULL, line_cfg);

    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    if (!gpio_request)
        return -1;

    return 0;
}

void gpio_write(unsigned int gpio, int value)
{
    if (value)
        gpiod_line_request_set_value(gpio_request, gpio, GPIOD_LINE_VALUE_ACTIVE);
    else
        gpiod_line_request_set_value(gpio_request, gpio, GPIOD_LINE_VALUE_INACTIVE);
}

int main(void)
{
    unsigned int led_gpio = 26;
    char cmd;

    int server_fd;
    int client_fd;
    int recv_len;

    char buffer[100];

    struct sockaddr_in server_addr;

    // gpio init

    if (gpio_init("/dev/gpiochip0", led_gpio) != 0)
    {
        printf("GPIO init failed\n");
        return 1;
    }

    // socket 
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

    while (1)
    {
        printf("Waiting for connection on port 5000...\n");
        client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0)
        {
            printf("accept failed\n");
            return 1;
        }

        printf("Client connected!\n");
        strcpy(FeedBack_Message,"Connected! ON/OFF\n");
        send(client_fd, FeedBack_Message, strlen(FeedBack_Message), 0);

        while (1)
        {
            recv_len = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

            if (recv_len <= 0)
            {
                printf("Client disconnected\n");
                break;
            }

            buffer[recv_len] = '\0';
            buffer[strcspn(buffer, "\r\n")] = '\0';

            if (strcmp(buffer, "ON") == 0)
            {
                printf("LED 1: ON\n");
                strcpy(FeedBack_Message, "STATUS: LED 1 ON\n");
                send(client_fd, FeedBack_Message, strlen(FeedBack_Message), 0);
                gpio_write(26,1);
            }

            else if (strcmp(buffer, "OFF") == 0)
            {
                printf("LED 1: OFF\n");
                strcpy(FeedBack_Message, "STATUS: LED 1 OFF\n");
                send(client_fd, FeedBack_Message, strlen(FeedBack_Message), 0);
                gpio_write(26,0);
            }

            else
            {
                strcpy(FeedBack_Message, "Wrong Command\n");
                send(client_fd, FeedBack_Message, strlen(FeedBack_Message), 0);
            }

            printf("Received: %s\n", buffer);
        }

        close(client_fd);
    }

    return 0;
}