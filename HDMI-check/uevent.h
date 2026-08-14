#ifndef UEVENT_H
#define UEVENT_H

int uevent_init(void);
int uevent_receive(int sock_fd, char *buffer, int size);
int is_hdmi_event(char *buffer, int len);

#endif