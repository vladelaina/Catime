/**
 * @file single.h
 * @brief Single-instance enforcement with CLI forwarding over a Unix socket.
 */
#ifndef CATIME_LINUX_SINGLE_H
#define CATIME_LINUX_SINGLE_H

typedef void (*SingleMessageCb)(char **tokens, int n);

/**
 * Try to become the single instance.
 * @return 0 if we are now the server (proceed to run);
 *         1 if another instance is running and we forwarded @p argv and must exit;
 *         -1 on unrecoverable error.
 */
int single_instance_init(SingleMessageCb cb, int argc, char **argv);

void single_instance_shutdown(void);

#endif /* CATIME_LINUX_SINGLE_H */
