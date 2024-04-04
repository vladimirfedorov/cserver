objects = cserver.o md4c.o md4c-html.o entity.o cjson.o mustach.o mustach-wrap.o mustach-cjson.o

cserver: $(objects)
	cc -o cserver $(objects)

cserver.o: cserver.c
	clang -I. --analyze cserver.c
	cc -Wall -I. -c cserver.c

# md4c redefines OFF_MAX as (sizeof(OFF) == 8 ? UINT64_MAX : UINT32_MAX); macOS SDK defeintion is LLONG_MAX
md4c.o: md4c/src/md4c.c
	cc -c md4c/src/md4c.c -Wno-macro-redefined

md4c-html.o: md4c/src/md4c-html.c
	cc -c md4c/src/md4c-html.c

entity.o: md4c/src/entity.c
	cc -c md4c/src/entity.c

cjson.o: cjson/cJSON.c
	cc -c cjson/cJSON.c

mustach.o: mustach/mustach.c
	cc -c mustach/mustach.c

mustach-cjson.o: mustach/mustach-cjson.c
	cc -I. -c mustach/mustach-cjson.c

mustach-wrap.o: mustach/mustach-wrap.c
	cc -I. -c mustach/mustach-wrap.c

.PHONY : clean
clean :
	rm $(objects)