#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_WORD 256
#define MAX_SYLLABLES 64
#define MAX_BUF 1024

// Syllabification and tadbalik logic based on the rules of Filipino phonology.
// is_vowel: checks if a character is a vowel (a, e, i, o, u).
static int is_vowel(char c) {
    c = tolower((unsigned char)c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

// compress_ng: replaces "ng" with a single character (0x01) to simplify syllabification.
static int compress_ng(const char *word, char *out) {
    int j   = 0;
    int len = (int)strlen(word);

    // Replace "ng" with 0x01 to treat it as a single consonant during syllabification.
    for (int i = 0; i < len; ) {
        if (i + 1 < len
                && tolower((unsigned char)word[i])     == 'n'
                && tolower((unsigned char)word[i + 1]) == 'g') {
            // Found "ng", replace with 0x01 and skip the next character.
            out[j++] = '\x01';
            i += 2;
        } else {
            // Copy the character as is.
            out[j++] = word[i++];
        }
    }
    // Null-terminate the output string.
    out[j] = '\0';
    return j;
}

// expand_ng: restores "ng" from the compressed form after syllabification.
static void expand_ng(const char *in, char *out) {
    int j = 0;
    for (int i = 0; in[i]; i++) {
        if (in[i] == '\x01') {
            // Found the compressed "ng" character, expand it back to "ng".
            out[j++] = 'n';
            out[j++] = 'g';
        } else {
            // Copy the character as is.
            out[j++] = in[i];
        }
    }
    // Null-terminate the output string.
    out[j] = '\0';
}

// syllabify: splits a word into syllables based on Filipino phonological rules.
static int syllabify(const char *word, char syls[MAX_SYLLABLES][MAX_WORD]) {
    char compressed[MAX_WORD];
    int  clen = compress_ng(word, compressed);

    int n = 0;
    int i = 0;
    
    while (i < clen) {
        char syl[MAX_WORD];
        int  slen = 0;

        // Onset: all consonants before the first vowel (if any)
        while (i < clen && !is_vowel(compressed[i]))
            // Add the consonant to the syllable and move to the next character.
            syl[slen++] = compressed[i++];

        // vowels
        if (i < clen && is_vowel(compressed[i]))
            // Add the vowel to the syllable and move to the next character.
            syl[slen++] = compressed[i++];

        // Coda: a single consonant after the vowel, if it's not followed by another vowel
        if (i < clen && !is_vowel(compressed[i])
                     && (i + 1 >= clen || !is_vowel(compressed[i + 1])))
            // Add the coda consonant to the syllable and move to the next character.
            syl[slen++] = compressed[i++];

        // Null-terminate the syllable string.
        syl[slen] = '\0';

        // Restore "ng" inside the syllable before storing
        char expanded[MAX_WORD];
        expand_ng(syl, expanded);
        strcpy(syls[n++], expanded);
    }

    return n;
}

// Words with fewer than 2 syllables, or these explicit exceptions, are left unchanged. */
static int should_skip(const char *word) {
    char lower[MAX_WORD];
    int  len = (int)strlen(word);
    for (int i = 0; i <= len; i++)
        // Convert the word to lowercase for case-insensitive comparison.
        lower[i] = tolower((unsigned char)word[i]);

    if (!strcmp(lower, "ng")   || !strcmp(lower, "ang") ||
        !strcmp(lower, "hays") || !strcmp(lower, "mga"))
        return 1;

    // Syllabify the word and check if it has fewer than 2 syllables.
    char syls[MAX_SYLLABLES][MAX_WORD];
    return syllabify(word, syls) < 2;
}

// tadbalik_word: converts a Filipino word to its tadbalik form by rearranging syllables.
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

// socket helper functions for sending and receiving data reliably over TCP.
// sock_send: sends the entire message, handling partial sends.
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

// sock_recv_line: receives data until a newline character is encountered, handling partial receives.
static int sock_recv_line(int fd, char *buf, int size) {
    int total = 0;
    while (total < size - 1) {
        // Receive one character at a time until a newline is found, ignoring carriage returns.
        char c;
        if (recv(fd, &c, 1, 0) <= 0) 
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            buf[total++] = c;
    }
    // Null-terminate the buffer after receiving the line.
    buf[total] = '\0';
    return total;
}

// handle_client: manages the interaction with a connected client, including receiving words, sending translations, and handling continuation.
static void handle_client(int conn_fd, struct sockaddr_in *cli_addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr->sin_addr, ip, sizeof(ip));
    printf("[SERVER] Client %s connected.\n", ip);

    sock_send(conn_fd, "Welcome to the Filipino Tadbalik Server!\n");

    char buf[MAX_BUF];

    while (1) {
        // Receive a word from the client
        int n = sock_recv_line(conn_fd, buf, sizeof(buf));
        if (n < 0)
            break;
        if (n == 0)
            continue;

        printf("[SERVER] Received: %s\n", buf);

        // Translate the word to tadbalik form and send the response
        char result[MAX_BUF * 2];
        tadbalik_word(buf, result);

        printf("[SERVER] Baliktad: %s\n", result);

        char response[MAX_BUF * 2 + 16];
        snprintf(response, sizeof(response), "Baliktad: %s\n", result);
        sock_send(conn_fd, response);

        // Ask whether to continue
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

int main(int argc, char *argv[]) {
    int port;

    if (argc > 1) {
        port = atoi(argv[1]);
    } else {
        printf("[SERVER] Enter port to start server: ");
        scanf("%d", &port);
    }

    // Create a TCP socket for the server to listen for incoming connections.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { 
        perror("socket"); 
        return 1; 
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");   
        close(server_fd); 
        return 1;
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen"); 
        close(server_fd); 
        return 1;
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