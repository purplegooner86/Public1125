// sudo apt install libssh-4 libssh-dev pkg-config

/* ssh_echo_server.c
 *
 * Minimal SSH echo server using libssh.
 * - listens on specified port (default 2222)
 * - uses a host key file you generate with ssh-keygen
 * - accepts password auth and simply accepts any password for demo purposes
 * - opens a session channel and echoes received bytes back to the client
 *
 * Build (Linux):
 *   gcc -o ssh_echo_server ssh_echo_server.c $(pkg-config --cflags --libs libssh)
 *
 * Run:
 *   ./ssh_echo_server 0.0.0.0 2222 /path/to/host_rsa_key
 *
*/

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUF_SIZE 4096

int handle_auth(ssh_session session) {
    /* Very simple auth loop: accept password or none for demo */
    for (;;) {
        ssh_message msg = ssh_message_get(session);
        if (!msg) return SSH_AUTH_DENIED;
        if (ssh_message_type(msg) == SSH_REQUEST_AUTH &&
            ssh_message_subtype(msg) == SSH_AUTH_METHOD_PASSWORD) {
            /* Extract username and password (demo: accept anything) */
            const char *user = ssh_message_auth_user(msg);
            const char *pw = ssh_message_auth_password(msg);
            (void)user; (void)pw;
            ssh_message_auth_reply_success(msg, 0); /* accept */
            ssh_message_free(msg);
            return SSH_AUTH_SUCCESS;
        }
        /* reject other auth methods */
        ssh_message_reply_default(msg);
        ssh_message_free(msg);
    }
    return SSH_AUTH_DENIED;
}

int main(int argc, char **argv) {
    const char *bindaddr = "0.0.0.0";
    const char *portstr = "2222";
    const char *hostkey = NULL;

    if (argc >= 2) bindaddr = argv[1];
    if (argc >= 3) portstr = argv[2];
    if (argc >= 4) hostkey = argv[3];
    if (!hostkey) {
        fprintf(stderr, "Usage: %s [bindaddr] [port] /path/to/host_rsa_key\n", argv[0]);
        return 1;
    }

    ssh_bind sshbind = ssh_bind_new();
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, bindaddr);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, portstr);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, hostkey); /* path to private key file */

    if (ssh_bind_listen(sshbind) < 0) {
        fprintf(stderr, "Error listening: %s\n", ssh_get_error(sshbind));
        ssh_bind_free(sshbind);
        return 1;
    }

    printf("SSH echo server listening on %s:%s\n", bindaddr, portstr);

    for (;;) {
        ssh_session session = ssh_new();
        if (!session) break;

        if (ssh_bind_accept(sshbind, session) != SSH_OK) {
            fprintf(stderr, "ssh_bind_accept failed: %s\n", ssh_get_error(sshbind));
            ssh_free(session);
            continue;
        }

        printf("Doing key exchange...\n");
        /* handle key exchange */
        if (ssh_handle_key_exchange(session) != SSH_OK) {
            fprintf(stderr, "Key exchange failed: %s\n", ssh_get_error(session));
            ssh_disconnect(session);
            ssh_free(session);
            continue;
        }

        printf("Authenticating client...\n");
        /* simple auth handler */
        if (handle_auth(session) != SSH_AUTH_SUCCESS) {
            fprintf(stderr, "Authentication failed\n");
            ssh_disconnect(session);
            ssh_free(session);
            continue;
        }

        printf("Client authenticated, Waiting for channel to open...\n");

    ssh_channel channel = NULL;

    for (;;) {
        ssh_message msg = ssh_message_get(session);
        if (!msg)
            break;

        if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL_OPEN &&
            ssh_message_subtype(msg) == SSH_CHANNEL_SESSION) {

            channel = ssh_message_channel_request_open_reply_accept(msg);
            ssh_message_free(msg);
            break;  // session channel opened
        }

        ssh_message_reply_default(msg);
        ssh_message_free(msg);
    }

    if (!channel) {
        fprintf(stderr, "No channel opened.\n");
        ssh_disconnect(session);
        ssh_free(session);
        return;
    }

        /* optional: you could request a shell or exec; for echo we just read/write */
        char buf[BUF_SIZE];
        int n;
        while ((n = ssh_channel_read(channel, buf, sizeof(buf), 0)) > 0) {
            /* echo back */
            int written = 0;
            while (written < n) {
                int w = ssh_channel_write(channel, buf + written, n - written);
                if (w < 0) break;
                written += w;
            }
        }

        /* cleanup */
        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        printf("Client disconnected, waiting for next connection...\n");
    }

    ssh_bind_free(sshbind);
    return 0;
}
 