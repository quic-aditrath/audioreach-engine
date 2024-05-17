pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ar_osal.pc
EXTRA_DIST = $(pkgconfig_DATA)

AM_CFLAGS = -Wall -Wno-pointer-to-int-cast -Wno-error=unused-function \
            -Wno-error=unused-label
AM_CFLAGS += -D__unused=__attribute__\(\(__unused__\)\) -DLINUX_ENABLED

AM_CFLAGS += -I$(srcdir)/api \
             -I$(top_srcdir)/acdb/ats/transports/diag/linux/inc/

osal_sources = ./api/ar_osal_error.h \
               ./api/ar_osal_file_io.h \
               ./api/ar_osal_heap.h \
               ./api/ar_osal_log.h \
               ./api/ar_osal_mem_op.h \
               ./api/ar_osal_mutex.h \
               ./api/ar_osal_servreg.h \
               ./api/ar_osal_shmem.h \
               ./api/ar_osal_signal.h \
               ./api/ar_osal_sleep.h \
               ./api/ar_osal_string.h \
               ./api/ar_osal_sys_id.h \
               ./api/ar_osal_thread.h \
               ./api/ar_osal_timer.h \
               ./api/ar_osal_types.h

osal_c_sources = ./src/linux/ar_osal_file_io.c \
                 ./src/linux/ar_osal_heap.c \
                 ./src/linux/ar_osal_log.c \
                 ./src/linux/ar_osal_mem_op.c \
                 ./src/linux/ar_osal_mutex.c \
                 ./src/linux/ar_osal_signal.c \
                 ./src/linux/ar_osal_sleep.c \
                 ./src/linux/ar_osal_string.c \
                 ./src/linux/ar_osal_thread.c \
                 ./src/linux/ar_osal_timer.c \
		 ./src/linux/qcom/ar_osal_shmem_db.c \
		  ./src/linux/qcom/ar_osal_servreg.c

if USE_DUMMY_DIAG
osal_c_sources += ./src/linux/ar_osal_stub_log_pkt_op.c
else
osal_c_sources += ./src/linux/qcom/ar_osal_log_pkt_op.c
endif


if ARGS_USE_SYSLOG
AM_CFLAGS += -DAR_OSAL_USE_SYSLOG
endif

libar_osal_la_LIBADD = -lrt -lpthread

if CUTILS_SUPPORTED
libar_osal_la_LIBADD += -lcutils
AM_CFLAGS += -DAR_OSAL_USE_CUTILS
endif

if USE_GLIB
libar_osal_la_LIBADD += -lglib-2.0
AM_CFLAGS += $(GLIB_CFLAGS) -D_GNU_SOURCE -Dstrlcat=g_strlcat -Dstrlcpy=g_strlcpy -include glib.h
endif

lib_includedir = $(includedir)
lib_include_HEADERS = $(osal_sources)
lib_LTLIBRARIES = libar_osal.la
libar_osal_la_CC = @CC@
libar_osal_la_SOURCES = $(osal_c_sources)
libar_osal_la_CFLAGS = $(AM_CFLAGS)
libar_osal_la_LDFLAGS = -shared -avoid-version
