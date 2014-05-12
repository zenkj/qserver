OBJS = src/qs_lvmpool.o src/qserver.o
LUADIR = lib/lua-5.2.3/src
LUATAR = lua-5.2.3.tar.gz

all: qserver

test: qserver
	./qserver -h
	./qserver 5 10
	./qserver 10 100

$(OBJS): %.o: %.c src/qs_common.h src/qs_lvmpool.h $(LUADIR)/lua.h
	gcc -c -g -o $@ $< -I$(LUADIR)

qserver: $(OBJS) $(LUADIR)/liblua.a
	gcc -o $@ $^ -lpthread -ldl -lm

$(LUADIR)/lua.h: lib/$(LUATAR)
	cd lib && tar zxf $(LUATAR)

$(LUADIR)/liblua.a: $(LUADIR)/lua.h
	cd $(LUADIR) && make a SYSCFLAGS="-DLUA_USE_LINUX" SYSLIBS="-Wl,-E -ldl -lreadline"

clean:
	rm -rf src/*.o qserver lib/lua-5.2.3

