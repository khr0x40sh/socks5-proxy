## Compile for ARMv5

### Example cross compile:

```bash
arm-linux-gnueabi-gcc -static -O2 socks.c -o socks
```
or for musl:

```bash
musl-gcc -static -O2 socks.c -o socks
```
Optional size reduction:
```bash
strip socks
```
Flags that help on old devices:

```
-O2
-static
-s
```
### Why This Works Well on Broken Embedded Networking

Avoids:
```
accept4
epoll
pthread
glibc extensions
complex DNS APIs
```
Uses only:
```
socket
bind
listen
accept
connect
select
read/write
fork
```
These exist on almost every Linux kernel since 2.4.
