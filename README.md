# Exercise 06

# Tadbalik Server-Client

A TCP client-server application written in C that translates Filipino words and sentences into *Baligtád* — an informal Filipino speech disguise also known as *tadbalik*.

---

## What is Tadbalik?

*Baligtád* (literally "inside-out" or "reversed") is a type of informal speech disguise commonly heard in the Philippines. This program implements the *tadbalik* variant, which partially reverses the syllabic order of a word:

```
Syllables:   1  2  3  →  3  1  2

maganda   →  damagan
malupet   →  petmalu
meyor     →  yorme
pasasalamat  →  matpasasala
```

Words with fewer than two syllables are left unchanged. The following words are also always skipped:

| Word  | Reason                              |
|-------|-------------------------------------|
| `ng`  | One-syllable function word          |
| `ang` | One-syllable function word          |
| `hays`| One-syllable word                   |
| `mga` | Special two-syllable exception      |

---

## Files

```
.
├── server.c    — Tadbalik translation server
└── client.c    — Client that sends text and receives translations
```

---

## Requirements

- GCC (or any C99-compatible compiler)
- POSIX-compatible OS (Linux or macOS)

---

## Compiling

```bash
gcc -o server server.c
gcc -o client client.c
```

---

## Usage

### 1. Start the server

```bash
./server [port]
```

If no port is provided, the server will prompt for one.

```
[SERVER] Listening on port 9999 ...
```

### 2. Start the client (in a separate terminal)

```bash
./client [server_ip] [port]
```

If no arguments are provided, the client will prompt for the server IP. Port defaults to `9999`.

```
Enter server IP address [127.0.0.1]:
[CLIENT] Connecting to 127.0.0.1:9999 ...
[CLIENT] Welcome to the Filipino Tadbalik Server!
```

### 3. Translate words

Once connected, type any Filipino word or sentence when prompted:

```
Enter Filipino word: maganda
[CLIENT] Baliktad: damagan
[CLIENT] CONTINUE? (y/n): y

Enter Filipino word: ako ay masaya
[CLIENT] Baliktad: koа yа yasama
[CLIENT] CONTINUE? (y/n): n
[CLIENT] GOODBYE
[CLIENT] Disconnected.
```

---

## Program Flow

```
Client                          Server
  |                               |
  |── Connect ──────────────────> |
  |<── "Welcome..." ─────────────|
  |                               |
  |<── "Enter Filipino word: " ──|
  |── word/sentence ────────────>|
  |<── "Baliktad: <result>" ─────|
  |<── "CONTINUE? (y/n): " ──────|
  |── y / n ───────────────────> |
  |      (if n) <── "GOODBYE" ───|
  |── Disconnect ───────────────>|
```

---

## How Syllabification Works

The syllabifier follows standard Filipino CV / CVC rules:

1. Collect any leading consonants.
2. Take the vowel nucleus (consecutive vowels stay together as a diphthong).
3. Close with one trailing consonant only when the next character is also a consonant or end-of-word (CVC rule).

The digraph **ng** is compressed to a single placeholder byte before syllabification and expanded back afterward, so it is always treated as one consonant unit.

---

## Notes

- The server handles one client at a time. To support multiple simultaneous clients, add `fork()` or threads inside the `accept` loop in `server.c`.
- Both programs use plain TCP with line-based messaging — no external libraries are required.