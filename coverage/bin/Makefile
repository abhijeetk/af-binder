
.PHONY: clean binaries

heredir = .
basedir = ../..

targets = afb-daemon-cov  afb-client hi3.so hello.so salut.so saha.so demat.so bug.so

binaries: $(targets)

clean:
	@rm $(targets) *.gcno *.gcda

#======================================================================================
# creates the targets
#======================================================================================

incdir = $(basedir)/include
srcdir = $(basedir)/src
samdir = $(basedir)/bindings/samples

bindir = $(heredir)/bin

cflags = -I$(incdir) \
	$(shell pkg-config --cflags --libs openssl libmicrohttpd json-c libsystemd uuid) \
	-ldl -lrt -lpthread 

afb_lib_src = $(shell ls $(srcdir)/*.c | egrep -v '/afs-|/main-|-so-v1|-so-vdyn|-fake|-api-dbus|-api-v1' )
afb_clib_src = $(shell ls $(srcdir)/*.c | egrep -v '/afs-|/main-|-so-v1|-so-vdyn|-fake|-api-dbus|-api-v1|afb-supervision' )

afb_daemon_srcs = $(srcdir)/main-afb-daemon.c $(afb_lib_src)
afb_daemon_defs = '-DAFB_VERSION="cov"' -DAGL_DEVEL -DWITH_MONITORING_OPTION '-DBINDING_INSTALL_DIR="fake"'

afb_client_srcs = $(srcdir)/main-afb-client-demo.c $(afb_clib_src)
afb_client_defs = '-DAFB_VERSION="cov"' '-DBINDING_INSTALL_DIR="fake"'

hello3_src = $(samdir)/hello3.c
hi_src = $(samdir)/hi3.c
binding_flags = -shared -fPIC -Wl,--version-script=$(samdir)/export.map

afb-daemon-cov: $(afb_daemon_srcs)
	@echo creation of $@
	@gcc -o $@ $(afb_daemon_srcs) --coverage $(afb_daemon_defs) $(cflags)

afb-client: $(afb_client_srcs)
	@echo creation of $@
	@gcc -o $@ $(afb_client_srcs) $(afb_client_defs) $(cflags)

hi3.so: $(hi3_src)
	@echo creation of $@
	@gcc -o $@ $(hi3_src) $(binding_flags) $(cflags)

hello.so: $(hello3_src)
	@echo creation of $@
	@gcc -o $@ $(hello3_src) '-DAPINAME="hello"' $(binding_flags) $(cflags)

salut.so: $(hello3_src)
	@echo creation of $@
	@gcc -o $@ $(hello3_src) '-DAPINAME="salut"' $(binding_flags) $(cflags)

saha.so: $(hello3_src)
	@echo creation of $@
	@gcc -o $@ $(hello3_src) '-DAPINAME="saha"' $(binding_flags) $(cflags)

demat.so: $(hello3_src)
	@echo creation of $@
	@gcc -o $@ $(hello3_src) '-DAPINAME="demat"' $(binding_flags) $(cflags)

bug.so: bug.c
	@echo creation of $@
	@gcc -o $@ bug.c $(binding_flags) $(cflags)

