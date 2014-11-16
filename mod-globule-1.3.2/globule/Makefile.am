SUBDIRS		= adapt
lib_LTLIBRARIES	= mod_globule.la
LIBTOOL:=@LIBTOOL@ --silent

bin_PROGRAMS = globuleutil globuleprobe
EXTRA_PROGRAMS = globuledebug

globuleutil_SOURCES = globuleutil.cpp

globuleprobe_CFLAGS = -DSTANDALONE_APR
globuleprobe_CPPFLAGS = -DSTANDALONE_APR
globuleprobe_SOURCES = globuleprobe.cpp \
  Storage.cpp netw/HttpRequest.cpp netw/Url.cpp utilities.cpp exceptions.cpp

globuledebug_CFLAGS = -DSTANDALONE_APR
globuledebug_CPPFLAGS = -DSTANDALONE_APR
globuledebug_SOURCES = globuledebug.cpp \
  resource/BaseHandler.cpp resource/Peer.cpp \
  Storage.cpp netw/HttpRequest.cpp netw/Url.cpp utilities.cpp exceptions.cpp

mod_globule_la_LIBADD = adapt/libadapt.la $(LTLIBOBJS)
mod_globule_la_LDFLAGS = -module
if DEBUGGCC
mod_globule_la_CFLAGS=-finstrument-functions
mod_globule_la_CXXFLAGS=-finstrument-functions
mod_globule_la_LDFLAGS+=-finstrument-functions
mod_globule_la_LDFLAGS+=-Wl,"--wrap=__cyg_profile_func_enter"
mod_globule_la_LDFLAGS+=-Wl,"--wrap=__cyg_profile_func_exit"
endif
mod_globule_la_SOURCES = \
	psodium.h \
	utilities.cpp \
	exceptions.cpp \
	utilities.h \
	monitoring.cpp \
	monitoring.h \
	redirect/dns_config.h           \
	redirect/dns_comm.h             \
	redirect/dns_comm.cpp           \
	redirect/mod_netairt.h          \
	redirect/mod_netairt.cpp        \
	redirect/dns_protocol.h         \
	redirect/dns_protocol.cpp       \
	redirect/dns_policy.hpp         \
	redirect/dns_policy.cpp         \
	redirect/dns_policy_rr.hpp      \
	redirect/dns_policy_rr.cpp      \
	redirect/dns_policy_wrr.hpp     \
	redirect/dns_policy_wrr.cpp     \
	redirect/dns_policy_wrand.hpp   \
	redirect/dns_policy_wrand.cpp   \
	redirect/dns_policy_as.hpp      \
	redirect/dns_policy_as.cpp      \
	redirect/dns_policy_balanced.hpp \
	redirect/dns_policy_balanced.cpp \
	Constants.hpp \
	Storage.cpp \
	Storage.hpp \
	FilterConfiguration.cpp \
	FilterConfiguration.hpp \
	alloc/Allocator.hpp \
	alloc/Allocator.cpp \
	locking.hpp \
	locking.cpp \
	heartbeat.hpp \
	heartbeat.cpp \
	filemonitor.hpp \
	filemonitor.cpp \
	policy/ReplPolicy.hpp \
	policy/ReplPolicy.cpp \
	policy/SpecialPolicy.hpp \
	policy/SpecialPolicy.cpp \
	policy/InvalidatePolicy.hpp \
	policy/InvalidatePolicy.cpp \
	policy/PureProxyPolicy.hpp \
	policy/PureProxyPolicy.cpp \
	policy/MirrorNoConsPolicy.hpp \
	policy/MirrorNoConsPolicy.cpp \
	policy/AlexPolicy.hpp \
	policy/AlexPolicy.cpp \
	policy/TtlPolicy.hpp \
	policy/TtlPolicy.cpp \
	policy/GlobeCBPolicy.hpp \
	policy/GlobeCBPolicy.cpp \
	resource/Handler.hpp \
	resource/Handler.cpp \
	resource/Peer.hpp \
	resource/Peer.cpp \
	resource/BaseHandler.hpp \
	resource/BaseHandler.cpp \
	resource/ConfigHandler.hpp \
	resource/ConfigHandler.cpp \
	resource/BrokeredHandler.hpp \
	resource/BrokeredHandler.cpp \
	resource/ImportHandler.hpp \
	resource/ImportHandler.cpp \
	resource/KeeperHandler.hpp \
	resource/KeeperHandler.cpp \
	resource/NameBindingHandler.hpp \
	resource/NameBindingHandler.cpp \
	resource/RedirectorHandler.hpp \
	resource/RedirectorHandler.cpp \
	resource/SourceHandler.hpp \
	resource/SourceHandler.cpp \
	resource/OriginHandler.hpp \
	resource/OriginHandler.cpp \
	resource/DatabaseHandler.hpp \
	resource/DatabaseHandler.cpp \
	resource/ProxyHandler.hpp \
	resource/ProxyHandler.cpp \
	event/EventMgr.cpp \
	event/EventMgr.hpp \
	event/GlobuleEvent.hpp \
	event/GlobuleEvent.cpp \
	event/HttpMetaEvent.hpp \
	event/HttpReqEvent.hpp \
	event/LoadEvent.hpp \
	event/HttpLogEvent.hpp \
	event/SwitchEvent.hpp \
	event/PostRequestEvent.hpp \
	event/RedirectEvent.hpp \
	event/RedirectEvent.cpp \
	event/ReportEvent.hpp \
	event/ReportEvent.cpp \
	event/RegisterEvent.hpp \
	event/RegisterEvent.cpp \
	event/HeartBeatEvent.hpp \
	event/FileMonitorEvent.hpp \
	event/EvictionEvent.hpp \
	netw/HttpRequest.cpp \
	netw/HttpRequest.hpp \
	netw/Url.cpp \
	netw/Url.hpp \
	documents.hpp \
	documents.cpp \
	resources.hpp \
	resources.cpp \
        configuration.hpp \
	mod_globule.cpp
if REPLACEMENTRMM
mod_globule_la_SOURCES += ./myapr_rmm.h ./myapr_rmm.c
endif
