
CDK_LIBS=$(builddir)/cdk/libcdk.a
CDK_INCLUDES=$(srcdir)/cdk/include

bin_PROGRAMS = shc

tool_flags = -I$(CDK_INCLUDES)
if STATIC_LIBSHARE
tool_flags += -I$(LIBSHARE)/include
endif
if DEBUG
tool_flags += -g
else
tool_flags += -O
endif

if STATIC_LIBSHARE
ld_flags = $(LIBSHARE)/src/share-lib/libshare.a
else
ld_flags = -lshare
endif
if USE_LIBCURSES
ld_flags += $(CDK_LIBS)
ld_flags += -lncurses
endif

# main source code for shioncoin console client utility program.
console_sources = \
	shcon.c \
	shcon.h \
	shcon_init.c \
	shcon_init.h \
	shcon_term.c \
	shcon_term.h \
	shcon_opt.c \
	shcon_opt.h \
	shcon_log.c \
	shcon_log.h \
	shcon_stream.c \
	shcon_stream.h \
	shcon_gui.c \
	shcon_gui.h \
	shcon_mine.c \
	shcon_mine.h \
	net/net.c \
	net/net.h \
	net/net_conn.c \
	net/net_read.c \
	net/net_write.c \
	key/key.c \
	key/key.h \
	key/key_dat.c \
	key/key_auth.c \
	command/command.c \
	command/command.h \
	command/command_print.c


shc_SOURCES = $(console_sources)
shc_CFLAGS = $(tool_flags) -DSHC_COIN_CLIENT
shc_LDADD = $(ld_flags)

if USDE_SERVICE
bin_PROGRAMS += usde
usde_SOURCES = $(console_sources)
usde_CFLAGS = $(tool_flags) -DUSDE_COIN_CLIENT
usde_LDADD = $(ld_flags)
endif

if EMC2_SERVICE
bin_PROGRAMS += emc2
emc2_SOURCES = $(console_sources)
emc2_CFLAGS = $(tool_flags) -DEMC2_COIN_CLIENT
emc2_LDADD = $(ld_flags)
endif

if TESTNET_SERVICE
bin_PROGRAMS += testnet
testnet_SOURCES = $(console_sources)
testnet_CFLAGS = $(tool_flags) -DTESTNET_COIN_CLIENT
testnet_LDADD = $(ld_flags)
endif

