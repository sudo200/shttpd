include config.mk
include lib/libs.mk
all: out/shttpd

out/shttpd: obj/main.c.o out
	if [ -n '$(wildcard obj/*.cpp.o)' ]; then $(CXX) $(LDFLAGS) -o'out/shttpd' $(wildcard obj/*.o) $(wildcard lib/bin/*.a); else $(CC) $(LDFLAGS) -o'out/shttpd' $(wildcard obj/*.o) $(wildcard lib/bin/*.a); fi
	$(OBJCOPY) --only-keep-debug 'out/shttpd' 'out/shttpd.dbg'
	chmod -x 'out/shttpd.dbg'
	$(OBJCOPY) --strip-unneeded 'out/shttpd'
	$(OBJCOPY) --add-gnu-debuglink='out/shttpd.dbg' 'out/shttpd'

obj/%.cpp.o: obj src/%.cpp
	$(CXX) -c -o'$@' 'src/$(patsubst obj/%.cpp.o,%,$@).cpp' $(CXXFLAGS)

obj/%.c.o: obj src/%.c
	$(CC) -c -o'$@' 'src/$(patsubst obj/%.c.o,%,$@).c' $(CFLAGS)

clean:
	$(RM) -r out
	$(RM) -r obj

out:
	$(MKDIR) out

obj:
	$(MKDIR) obj

.PHONY: clean all
