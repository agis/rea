# rea
rea is an experimental HTTP/1.1 server written in C. It has an event-driven architecture and uses `select(2)`.

It's nowhere near functional yet and just parses requests and writes responses back. It can handle many concurrent
clients efficiently since it uses non-blocking I/O.

Roadmap:
- Switch from `select(2)` to `epoll(7)`
- Link with [mruby](https://github.com/mruby/mruby) and define request handlers in it
- IPv6 support
- Unix domain sockets support


