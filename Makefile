objects = cserver.o md4c.o md4c-html.o entity.o mustach.o

cserver: $(objects)
	cc -o cserver $(objects)

# md4c redefines OFF_MAX as (sizeof(OFF) == 8 ? UINT64_MAX : UINT32_MAX); macOS SDK defeintion is LLONG_MAX
md4c.o: md4c/src/md4c.c
	cc -c md4c/src/md4c.c -Wno-macro-redefined

md4c-html.o: md4c/src/md4c-html.c
	cc -c md4c/src/md4c-html.c

entity.o: md4c/src/entity.c
	cc -c md4c/src/entity.c

mustach.o: mustach/mustach.c
	cc -c mustach/mustach.c

.PHONY : clean
clean :
	rm $(objects)