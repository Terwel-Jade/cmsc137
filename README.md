# Exercise 06 Server-Client Application in C

- Jade Brian Terwel
- CMSC 137 AB-2L
- 2nd Semester 2025-2026

# Tadbalik

A TCP server-client application written in C that translates Filipino words into *Baligtád* — an informal Filipino speech disguise also known as *tadbalik*.

---

## Files

```
.
├── server.c    — Tadbalik translation server
└── client.c    — Client that sends text and receives translations
```

---

## Compiling and Running
Run in two different Linux/Unix terminals
```bash
gcc -o server server.c && ./server [port]
```

If no port is given, the server program will prompt for it.

```bash
gcc -o client client.c && ./client [server_ip] [port]
```

If no server_ip is given, the client program will prompt for it.
If no port is given, the client program will use the default port `9999`

---