#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_BUF 1024
#define DEFAULT_PORT 9999

// strip_newline: removes trailing newline characters from a string.
static void strip_newline(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

// sock_recv_until: receives data until a specified sentinel string is encountered, handling partial receives.
static int sock_recv_until(int fd, char *buf, int size, const char *sentinel) {
    int total = 0;
    int slen  = (int)strlen(sentinel);

    while (total < size - 1) {
        // Receive one character at a time until the sentinel is found, ignoring carriage returns.
        char c;
        if (recv(fd, &c, 1, 0) <= 0) 
            return -1;
        if (c != '\r') 
            buf[total++] = c;
        if (c == '\n') 
            break;
        // Check if the end of the buffer matches the sentinel string.
        if (total >= slen &&
                strncmp(buf + total - slen, sentinel, slen) == 0)
            break;
    }
    // Null-terminate the buffer after receiving the data.
    buf[total] = '\0';
    return total;
}

int main(int argc, char *argv[]) {
    char server_ip[64] = "127.0.0.1";
    int  port = DEFAULT_PORT;

    // Get server IP and port from command line or user input
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
    } else {
        printf("Enter server IP address [127.0.0.1]: ");
        fflush(stdout);
        if (fgets(server_ip, sizeof(server_ip), stdin)) {
            strip_newline(server_ip);
            if (strlen(server_ip) == 0)
                strcpy(server_ip, "127.0.0.1");
        }
    }

    // Get port number from command line or user input
    if (argc > 2) 
        port = atoi(argv[2]);

    printf("[CLIENT] Connecting to %s:%d ...\n", server_ip, port);

    // Create socket and connect to server
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "[CLIENT] Invalid IP address: %s\n", server_ip);
        close(sock_fd);
        return 1;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[CLIENT] connect");
        close(sock_fd);
        return 1;
    }

    // Receive welcome message from server
    char buf[MAX_BUF];
    sock_recv_until(sock_fd, buf, sizeof(buf), "\n");
    strip_newline(buf);
    printf("[CLIENT] %s\n", buf);

    while (1) {
        // Get user input
        printf("\n[CLIENT] Enter Filipino word: ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        strip_newline(buf);
        if (strlen(buf) == 0) continue;

        // Send the word to the server
        char to_send[MAX_BUF + 2];
        snprintf(to_send, sizeof(to_send), "%s\n", buf);
        if (send(sock_fd, to_send, strlen(to_send), 0) < 0) {
            perror("[CLIENT] send"); break;
        }

        // Receive and print translation
        sock_recv_until(sock_fd, buf, sizeof(buf), "\n");
        strip_newline(buf);
        printf("[CLIENT] %s\n", buf);

        // Receive and display the "CONTINUE?" prompt
        sock_recv_until(sock_fd, buf, sizeof(buf), ": ");
        strip_newline(buf);
        printf("[CLIENT] %s ", buf);
        fflush(stdout);

        // Read user's answer
        char answer[64];
        if (!fgets(answer, sizeof(answer), stdin)) break;
        strip_newline(answer);

        // Send answer to server
        char ans_send[70];
        snprintf(ans_send, sizeof(ans_send), "%s\n", answer);
        if (send(sock_fd, ans_send, strlen(ans_send), 0) < 0) {
            perror("[CLIENT] send"); break;
        }

        // If not continuing, receive GOODBYE message and exit
        if (strcasecmp(answer, "yes") != 0 && strcasecmp(answer, "y") != 0) {
            sock_recv_until(sock_fd, buf, sizeof(buf), "\n");
            strip_newline(buf);
            printf("[CLIENT] %s\n", buf);
            break;
        }
    }

    close(sock_fd);
    printf("[CLIENT] Disconnected.\n");
    return 0;
}