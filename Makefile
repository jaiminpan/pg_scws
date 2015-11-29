# contrib/pg_scws/Makefile

MODULE_big = pg_scws

SCWS_OBJS = charset.o crc32.o pool.o scws.o xdict.o darray.o rule.o lock.o xdb.o xtree.o

OBJS = pg_scws.o \
		$(addprefix libscws/,$(SCWS_OBJS))

EXTENSION = pg_scws
DATA = pg_scws--1.0.sql pg_scws--unpackaged--1.0.sql
DATA_TSEARCH = dict/dict.utf8.xdb dict/rules.utf8.ini

REGRESS = pg_scws

PG_CPPFLAGS = -DHAVE_STRUCT_FLOCK -DHAVE_FLOCK

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_scws
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
