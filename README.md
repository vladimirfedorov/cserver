# cserver

[![Unit Tests](https://github.com/vladimirfedorov/cserver/actions/workflows/run-tests.yml/badge.svg)](https://github.com/vladimirfedorov/cserver/actions/workflows/run-tests.yml)

The idea is to build a web server in C that:

- [x] Serves static files from the `static` folder.
- [x] Serves Markdown files, converting them to HTML files on the fly.
- [ ] Categories based on pages metadata
- [ ] Atom feeds
- [ ] Runs Lua scripts.

Ideally, the server should be able to serve static files in the `static` folder, transform Markdown files to HTML pages using templates in the `templates` folder, and create RSS feed(s) for categories in markdow files' header metadata. Each rendered page should be cached and kept cached until the server restarts or one of the website files changes.

### Make

```sh
./scripts/make.sh
```

### Test

```sh
./scripts/test.sh
```


### Run

```sh
./cserver run /path/to/files
```

--OR--

```sh
# start as a service
./cserver start /path/to/files

# list running instances
./cserver list

# stop an instance
./cserver stop id
```

The server listens on port 3000 by default. You can change this value in `config.json` (see the `example` folder).

