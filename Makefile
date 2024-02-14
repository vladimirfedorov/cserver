objects = cserver.o md4c.o md4c-html.o entity.o

cserver: $(objects)
	cc -o cserver $(objects)

md4c.o: md4c/src/md4c.c
	cc -c md4c/src/md4c.c

md4c-html.o: md4c/src/md4c-html.c
	cc -c md4c/src/md4c-html.c

entity.o: md4c/src/entity.c
	cc -c md4c/src/entity.c

.PHONY : clean
clean :
	rm $(objects)