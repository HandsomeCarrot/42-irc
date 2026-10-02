*This project has been created as part of the 42 curriculum by vpoka, hasaliho.*

# ft_irc

## Table of contents

- [Description](#description)
- [Features](#features)
- [Instructions](#instructions)
- [Technical choices](#technical-choices)
- [Project layout](#project-layout)
- [Resources](#resources)
- [Status](#status)

## Description

`ft_irc` is a small **Internet Relay Chat (IRC)** server written in **C++98**. Multiple clients can connect at once, register, join channels, and exchange messages with low latency over TCP.

IRC is a classic text-protocol chat system: clients send CRLF-terminated commands (`NICK`, `JOIN`, `PRIVMSG`, …); the server parses them, enforces access rules, and fans replies out to the right sockets.

### Goal

Ship a single-process, **non-blocking**, multiplexed IRC daemon that:

- Accepts many concurrent clients without threads or `fork`
- Speak enough of RFC 1459 / modern client behaviour to work with real clients (especially **irssi**) and `nc`

### Overview

The binary is `ircserv`. One process owns a listening socket and every client fd, drives them from a single **`poll()`** loop, and routes complete IRC lines through a command dispatcher. Registration is a strict state machine (`PASS` → `NICK`/`USER`, with optional IRCv3-style `CAP` negotiation held until `CAP END`). Channels support operator privileges and modes `i`, `t`, `k`, `o`, and `l`.

## Features

- **I/O:** non-blocking sockets, one `poll()` loop, per-client input/output buffers (partial reads/writes, multi-command packets)
- **Registration:** password gate, welcome burst `001`–`004`, `CAP LS` / `CAP END` hold for modern clients
- **Identity:** nick validation (RFC-style charset, max length 9), RFC 1459 casemapping (`[\]~` ↔ `{|}\``)
- **Messaging:** `PRIVMSG` / `NOTICE` to users and channels
- **Channels:** `JOIN` / `PART` / `TOPIC` / `INVITE` / `KICK`, first joiner becomes operator
- **Modes:** `+i` invite-only, `+t` topic lock, `+k` key, `+o` op, `+l` user limit
- **Liveness:** `PING` / `PONG`, `QUIT`, clean disconnect cleanup (channels, fds), `SIGPIPE` ignored
- **Build:** `-Wall -Wextra -Werror -std=c++98`

## Instructions

### Requirements

- A Unix-like environment with a C++98-capable toolchain (`c++` / g++)
- Developed and tested on Linux (Ubuntu-class). Other OSes are not guaranteed.

### Build

```bash
git clone <repo-url> ft_irc
cd ft_irc
make
```

| Rule                | Effect                                 |
| ------------------- | -------------------------------------- |
| `make` / `make all` | Build `ircserv` (objects under `obj/`) |
| `make clean`        | Remove `obj/`                          |
| `make fclean`       | `clean` + remove `ircserv`             |
| `make re`           | `fclean` then `all`                    |

Flags: `-Wall -Wextra -Werror -std=c++98 -MD -MP -Isrc`. \
Isrc just there for including headers from `src/`.

### Run

```bash
./ircserv [port [password]]
```

| Argument   | Meaning                      | Default                      |
| ---------- | ---------------------------- | ---------------------------- |
| `port`     | TCP listen port              | `65505`                      |
| `password` | Connection password (`PASS`) | empty (no password required) |

Examples:

```bash
./ircserv 6667 pass
./ircserv 2222 secret123
./ircserv              # port 65505, no password
```

Stop the server with `Ctrl+C` (`SIGINT`) for an orderly shutdown.

### Connect a client

**irssi** (recommended):

```bash
irssi
/connect 127.0.0.1 <port> <password>
```

**netcat** (manual protocol / debugging; use CRLF):

```bash
nc -C 127.0.0.1 <port>
PASS <password>
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :hello
```

### Quick smoke check

```bash
make
./ircserv 6667 pass &
printf 'PASS pass\r\nNICK bob\r\nUSER b 0 * :Bob\r\n' | nc -w 2 127.0.0.1 6667
# expect numerics 001–004
kill -INT %1
```

## Technical choices

| Choice                                        | Why                                                                                                                   |
| --------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| **`poll()`** (not threads / `fork` / `epoll`) | Subject requires multiplexed I/O in one process; `poll()` is portable POSIX                                           |
| **Non-blocking fds + outbuf**                 | A slow or partial `send` must not stall other clients; unfinished bytes stay queued until `POLLOUT`                   |
| **CRLF framing only**                         | IRC lines end in `\r\n`; incomplete lines stay in the per-client inbuf                                                |
| **Strict registration states**                | `AWAITING_PASS` → `AWAITING_REGISTRATION` → `REGISTERED`; wrong `PASS` → `464` and disconnect; pre-auth noise → `451` |
| **`CAP` allowed before `PASS`**               | irssi sends `CAP LS` first; registration still waits for `CAP END` when negotiation started                           |
| **RFC 1459 nick casemap**                     | Collision checks treat `[Test]` and `{Test}` (and `\` / `\|`) as the same nick                                        |
| **C++98**                                     | 42 curriculum constraint                                                                                              |

Clients exercised against this server: **irssi**, **nc** (`-C`).

## Project layout

```
src/
  main.cpp                 # entry, signals, poll loop
  core/                    # command dispatch, numeric replies
  messages/                # IRC message parser
  networking/              # NetworkHost, Client, Channel
  util/                    # logger, RFC casemap helper
Makefile
```

## Resources

### References

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459) — baseline commands, nicks, channels, numerics
- [RFC 2812 — Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812) — later client-oriented wording
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/) — how clients behave today (framing, registration, CAP)
- [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/html/) — sockets, blocking vs non-blocking, `poll`
- [`poll(2)`](https://man7.org/linux/man-pages/man2/poll.2.html) — readiness API used by this server
- [irssi](https://irssi.org/) — primary GUI-less client used for manual testing
- [IRCv3 Capability Negotiation](https://ircv3.net/specs/extensions/capability-negotiation.html) — why `CAP LS` / `CAP END` matter for irssi

### AI usage

AI was used as a tool. \
Typical use cases:

| Who          | Tasks                                                                                                                                                                                                              |
| ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **vpoka**    | Researching IRC/server concepts and RFC implications; spotting bugs; documentation help; drafting commit messages; Shout out to my AI girlfriend for mentally supporting me through the project!                                                                                                  |
| **hasaliho** | Protocol edge-case checks (registration, CAP, modes, nick rules); debugging registration/I/O behaviour; help writing project docs; automated edge test runs and help with writting test scripts during development |

## Status

Finished. Graded **115 / 100**.
