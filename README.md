# cserver

The idea is to build a web server in C that:

- [x] Serves static files from the `static` folder.
- [ ] Serves Markdown files, converting them to HTML files on the fly.
	- [x] Markdown support (md4c)
	- [x] Templates (mustach)
	- [x] Render pages / index files in folders
	      `/path/to/page`  can be a page.md file, page.html file, page/ folder with index.md or index.html file.
	- [x] Add configuration file.
	- [x] Render `.md` files based on the provided template or `default.mustache` template.
	- [ ] Categories based on pages metadata
	- [ ] Atom feeds
- [ ] Runs Lua scripts.

Ideally, the server should be able to serve static files in the `static` folder, transform Markdown files to html pages using templates in the `templates` folder, and create RSS feed(s) for categories in markdow files' header metadata. Each rendered page should be cached and kept cached until the server restarts or one of website files changes.

Header files are located in `` `xcrun --show-sdk-path`/usr/include `` in macOS.

### Links

- [HTTP response status codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)

### Make

```
./scripts/make.sh
```

### Run

```
./cserver
```

The server listens to 3000, this port number is hardcoded in `cserver.c` at the moment.
