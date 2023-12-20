# cserver

The idea is to build a web server in C that:

- [ ] Serves static files from the `static` folder.
- [ ] Serves Markdown files, converting them to HTML files on the fly.
- [ ] Runs Lua scripts.

Header files are located in `` `xcrun --show-sdk-path`/usr/include `` in macOS.

### Links

- [HTTP response status codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)

### Make

```
make cserver
```

### Run

```
./cserver
```
