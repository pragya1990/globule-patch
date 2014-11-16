lib_LTLIBRARIES	= mod_psodium.la

mod_psodium_la_LDFLAGS = -module
mod_psodium_la_SOURCES = \
	utillib/AprError.cpp \
	utillib/AprError.hpp \
	utillib/Debug.cpp \
	utillib/DigestsUtil.cpp \
	utillib/SlaveDatabaseUtil.cpp \
	utillib/BadSlavesUtil.cpp \
	utillib/LyingClientsUtil.cpp \
	utillib/PledgeUtil.cpp \
	utillib/util.c \
        module/client/HandleMasterRedirect.cpp \
        module/client/HandleMasterGetSlaveInfo.cpp \
        module/client/HandleSlaveGet.cpp \
        module/client/HandleMasterDigests.cpp \
        module/client/HandleMasterBadPledge.cpp \
        module/client/HandleAuditorPutPledge.cpp \
        module/client/ClientRequestHandling.cpp \
        module/client/modified_proxy_http.c \
        module/client/modified_proxy_util.c \
        module/MasterRequestHandling.cpp \
        module/output_filter.c \
        module/PsodiumConfig.cpp \
        module/pso_error.c \
        module/temp_storage.c \
        module/mod_psodium.c

bin_SCRIPTS = auditorctl
bin_PROGRAMS = auditor
auditor_SOURCES = \
	utilities.cpp \
	../globule/alloc/Allocator.cpp \
	../globule/locking.cpp \
	../globule/netw/Url.cpp \
	utillib/AprError.cpp \
	utillib/AprError.hpp \
	utillib/Debug.cpp \
	utillib/DigestsUtil.cpp \
	utillib/SlaveDatabaseUtil.cpp \
	utillib/BadSlavesUtil.cpp \
	utillib/PledgeUtil.cpp \
	utillib/util.c \
	auditord/AuditorThread.cpp \
        auditord/AuditorMain.cpp 

auditor_CFLAGS = -DSTANDALONE_APR
auditor_CXXFLAGS = -DSTANDALONE_APR
auditor_LDFLAGS = -laprutil-0 -lcrypt -lgdbm -ldb -lpthread -lexpat \
		  $(libpsodium_la_LDFLAGS)
