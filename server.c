// server.c
// Tadbalik Server — translates Filipino words to Baligtad (tadbalik) speech.
// Compile: gcc -o server server.c
// Usage:   ./server [port]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_WORD      256
#define MAX_SYLLABLES  64
#define MAX_BUF       1024

/* ── Helpers ────────────────────────────────────────────────────────────── */

static int is_vowel(char c) {
    c = tolower((unsigned char)c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

/* Replace every "ng" digraph with a single placeholder byte (\x01)
   so it is treated as one consonant during syllabification. */
static int compress_ng(const char *word, char *out) {
    int j   = 0;
    int len = (int)strlen(word);

    for (int i = 0; i < len; ) {
        if (i + 1 < len
                && tolower((unsigned char)word[i])     == 'n'
                && tolower((unsigned char)word[i + 1]) == 'g') {
            out[j++] = '\x01';
            i += 2;
        } else {
            out[j++] = word[i++];
        }
    }
    out[j] = '\0';
    return j;
}

/* Restore every \x01 placeholder back to "ng". */
static void expand_ng(const char *in, char *out) {
    int j = 0;
    for (int i = 0; in[i]; i++) {
        if (in[i] == '\x01') {
            out[j++] = 'n';
            out[j++] = 'g';
        } else {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

/* ── Syllabification ────────────────────────────────────────────────────── */

/* Split `word` into syllables using CV / CVC rules.
   Fills syls[0..n-1] and returns n. */
static int syllabify(const char *word, char syls[MAX_SYLLABLES][MAX_WORD]) {
    char compressed[MAX_WORD];
    int  clen = compress_ng(word, compressed);

    int n = 0;
    int i = 0;

    while (i < clen) {
        char syl[MAX_WORD];
        int  slen = 0;

        /* Leading consonants */
        while (i < clen && !is_vowel(compressed[i]))
            syl[slen++] = compressed[i++];

        /* Vowel nucleus */
        if (i < clen && is_vowel(compressed[i]))
            syl[slen++] = compressed[i++];

        /* Closing consonant (only when not followed by another vowel) */
        if (i < clen && !is_vowel(compressed[i])
                     && (i + 1 >= clen || !is_vowel(compressed[i + 1])))
            syl[slen++] = compressed[i++];

        syl[slen] = '\0';

        /* Restore "ng" inside the syllable before storing */
        char expanded[MAX_WORD];
        expand_ng(syl, expanded);
        strcpy(syls[n++], expanded);
    }

    return n;
}

/* ── Tadbalik transformation (1-2-3 → 3-1-2) ───────────────────────────── */

/* Words with fewer than 2 syllables, or these explicit exceptions,
   are left unchanged. */
static int should_skip(const char *word) {
    char lower[MAX_WORD];
    int  len = (int)strlen(word);
    for (int i = 0; i <= len; i++)
        lower[i] = tolower((unsigned char)word[i]);

    if (!strcmp(lower, "ng")   || !strcmp(lower, "ang") ||
        !strcmp(lower, "hays") || !strcmp(lower, "mga"))
        return 1;

    char syls[MAX_SYLLABLES][MAX_WORD];
    return syllabify(word, syls) < 2;
}

/* Rotate the last syllable to the front: [1,2,3] → [3,1,2]. */
static void tadbalik_word(const char *word, char *out) {
    if (should_skip(word)) {
        strcpy(out, word);
        return;
    }

    char syls[MAX_SYLLABLES][MAX_WORD];
    int  n = syllabify(word, syls);

    out[0] = '\0';
    strcat(out, syls[n - 1]);
    for (int i = 0; i < n - 1; i++)
        strcat(out, syls[i]);
}

/* ── Socket helpers ─────────────────────────────────────────────────────── */

static int sock_send(int fd, const char *msg) {
    int len  = (int)strlen(msg);
    int sent = 0;
    while (sent < len) {
        int n = (int)send(fd, msg + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

/* Read one line from the socket (strips '\r', stops at '\n'). */
static int sock_recv_line(int fd, char *buf, int size) {
    int total = 0;
    while (total < size - 1) {
        char c;
        if (recv(fd, &c, 1, 0) <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[total++] = c;
    }
    buf[total] = '\0';
    return total;
}

/* ── Client handler ─────────────────────────────────────────────────────── */

static void handle_client(int conn_fd, struct sockaddr_in *cli_addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr->sin_addr, ip, sizeof(ip));
    printf("[SERVER] Client %s connected.\n", ip);

    sock_send(conn_fd, "Welcome to the Filipino Tadbalik Server!\n");

    char buf[MAX_BUF];

    while (1) {
        /* Receive from client */
        int n = sock_recv_line(conn_fd, buf, sizeof(buf));
        if (n < 0)
            break;
        if (n == 0)
            continue;

        printf("[SERVER] Received: %s\n", buf);

        /* Translate and send result */
        char result[MAX_BUF * 2];
        tadbalik_word(buf, result);

        printf("[SERVER] Baliktad: %s\n", result);

        char response[MAX_BUF * 2 + 16];
        snprintf(response, sizeof(response), "Baliktad: %s\n", result);
        sock_send(conn_fd, response);

        /* Ask whether to continue */
        sock_send(conn_fd, "CONTINUE? (y/n): ");
        if (sock_recv_line(conn_fd, buf, sizeof(buf)) < 0)
            break;

        if (tolower((unsigned char)buf[0]) != 'y') {
            sock_send(conn_fd, "GOODBYE\n");
            break;
        }
    }

    close(conn_fd);
    printf("[SERVER] Client %s disconnected.\n", ip);
}

/* ── Entry point ────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    int port;

    if (argc > 1) {
        port = atoi(argv[1]);
    } else {
        printf("[SERVER] Enter port to start server: ");
        scanf("%d", &port);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");   close(server_fd); return 1;
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    printf("[SERVER] Listening on port %d ...\n", port);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int conn_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (conn_fd < 0) { perror("accept"); continue; }

        handle_client(conn_fd, &cli_addr);
    }

    close(server_fd);
    return 0;
}