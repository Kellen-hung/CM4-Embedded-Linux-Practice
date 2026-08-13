#include <stdio.h>
#include <string.h>
#include "gpio.h"
#include "hdmi.h"

void update_hdmi_status(void)
{
    FILE *fp;
    char status[32];

    fp = fopen("/sys/class/drm/card1-HDMI-A-1/status", "r");

    if (!fp)
    {
        perror("fopen HDMI status");
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