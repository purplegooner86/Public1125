// sudo apt install libssh-4 libssh-dev pkg-config

/* ssh_echo_client.c
 *
 * Minimal SSH client using libssh that connects and sends data, reads echo.
 *
 * Build:
 *   gcc -o ssh_echo_client ssh_echo_client.c $(pkg-config --cflags --libs libssh)
 *
 * Usage:
 *   ./ssh_echo_client <host> <port> <username> <password> "message to echo"
 *
 * Example:
 *   ./ssh_echo_client 127.0.0.1 2222 testuser secret "hello world\n"
*/

#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s host port username password message\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *user = argv[3];
    const char *pass = argv[4];
    const char *msg = argv[5];

    ssh_session session = ssh_new();
    if (!session) return 1;

    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, user);

    if (ssh_connect(session) != SSH_OK) {
        fprintf(stderr, "Error connecting: %s\n", ssh_get_error(session));
        ssh_free(session);
        return 1;
    }

    /* For demo: accept any server key (INSECURE). In production verify host key! */
    if (ssh_is_server_known(session) != SSH_SERVER_KNOWN_OK) {
        /* In demo accept; in real code check fingerprint and persist */
        /* WARNING: this disables MITM detection */
    }

    if (ssh_userauth_password(session, NULL, pass) != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Authentication failed: %s\n", ssh_get_error(session));
        ssh_disconnect(session);
        ssh_free(session);
        return 1;
    }

    printf("Creating channel...\n");
    ssh_channel channel = ssh_channel_new(session);
    if (!channel) {
        fprintf(stderr, "Failed to create channel\n");
        ssh_disconnect(session);
        ssh_free(session);
        return 1;
    }

    printf("Opening session channel...\n");
    if (ssh_channel_open_session(channel) != SSH_OK) {
        fprintf(stderr, "Failed to open session channel: %s\n", ssh_get_error(session));
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        return 1;
    }

    /* send message */
    printf("Sending: '%s'\n", msg);
    if (ssh_channel_write(channel, msg, strlen(msg)) < 0) {
        fprintf(stderr, "Failed to write to channel\n");
    }

    /* read echo */
    char buf[4096];
    printf("Reading from channel...\n");
    int n = ssh_channel_read(channel, buf, sizeof(buf)-1, 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("Echoed: '%s'\n", buf);
    } else {
        printf("No data echoed\n");
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    return 0;
}