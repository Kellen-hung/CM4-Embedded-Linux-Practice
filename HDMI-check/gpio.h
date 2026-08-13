#ifndef GPIO_H
#define GPIO_H

int gpio_init(const char *chip_path, unsigned int gpio);
void gpio_write(unsigned int gpio, int value);
void gpio_cleanup(void);

#endif