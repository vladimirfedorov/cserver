# cserver

The idea is to build a web server in C that:

- [x] Serves static files from the `static` folder.
- [ ] Serves Markdown files, converting them to HTML files on the fly.
- [ ] Runs Lua scripts.

Ideally, the server should be able to serve static files from the `static` folder, transform Markdown files to html pages using templates in the `templates` folder, and create RSS feed(s) for categories in markdow files' header metadata. Each rendered page should be cached and kept cached until the server restarts or one of website files changes.

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
