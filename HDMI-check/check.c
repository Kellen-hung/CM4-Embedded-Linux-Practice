#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gpiod.h>

struct gpiod_line_request *gpio_request;

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

int main() 
{
    unsigned int led_gpio = 26;

    if (gpio_init("/dev/gpiochip0", led_gpio) != 0)
    {
        printf("GPIO init failed\n");
        return 1;
    }

    while(1)
    {
        
    }
}