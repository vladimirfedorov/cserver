# cserver

[![Unit Tests](https://github.com/vladimirfedorov/cserver/actions/workflows/run-tests.yml/badge.svg)](https://github.com/vladimirfedorov/cserver/actions/workflows/run-tests.yml)

The idea is to build a web server in C with the following features:

- [x] Serving static files from the `static` folder.
- [x] Serving Markdown files and converting them to HTML on the fly.
- [ ] Categorizing pages based on their metadata.
- [ ] Generating Atom feeds.
- [ ] Running Lua scripts.

Ideally, the server should be able to serve static files in the `static` folder, transform Markdown files to HTML pages using templates in the `templates` folder, and create RSS feeds for categories based on Markdown files' header metadata. Each rendered page should be cached and kept cached until the server restarts or one of the website files changes.

### Make

```sh
./scripts/make.sh
```

### Test

```sh
./scripts/test.sh
```


### Run

You can run the server in CLI or debug mode. In this mode, the server will stay active in the terminal and will print all the output to the terminal.

```sh
./cserver run /path/to/files
```

Alternatively, you can run the server in service mode. In this mode, the server will keep running in the background.

```sh
# start as a service
./cserver start /path/to/files

# list running instances
./cserver list

# restart an instance
./cserver start /path/to/files

# stop an instance
./cserver stop id
```

The server listens on port 3000 by default. You can change this value in `config.json` (see the `example` folder).

