#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

static struct gpiod_line_request *request_output_line(const char *chip_path, unsigned int offset)
{
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_line_request *request;

    chip = gpiod_chip_open(chip_path);
    if (!chip)
        return NULL;

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();

    if (!settings || !line_cfg)
        return NULL;

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings))
        return NULL;

    request = gpiod_chip_request_lines(chip, NULL, line_cfg);

    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    return request;
}

int main(void)
{
    const char *chip_path = "/dev/gpiochip0";
    unsigned int gpio = 26;

    struct gpiod_line_request *request;

    request = request_output_line(chip_path, gpio);

    if (!request)
    {
        printf("Failed to request GPIO26\n");
        return 1;
    }

    while (1)
    {
        printf("LED ON\n");
        gpiod_line_request_set_value(request, gpio, GPIOD_LINE_VALUE_ACTIVE);
        sleep(1);

        printf("LED OFF\n");
        gpiod_line_request_set_value(request, gpio, GPIOD_LINE_VALUE_INACTIVE);
        sleep(1);
    }

    gpiod_line_request_release(request);

    return 0;
}