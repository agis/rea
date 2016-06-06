# rea
rea is a toy HTTP/1.1 server written in C. It has an event-driven architecture and uses non-blocking I/O (`select(2)` for now) and can handle many concurrent clients efficiently; within a single thread.

It's nowhere near functional yet and just parses requests and writes dummy responses back.

Roadmap:
- Switch from `select(2)` to `epoll(7)`
- Link with [mruby](https://github.com/mruby/mruby) and define request handlers in it
- IPv6 support
- Unix domain sockets support

## LICENSE

See [MIT-LICENSE](MIT-LICENSE).
