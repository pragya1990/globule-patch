ApacheMonitor is extended to automatically fetch new config files
and restart Apache. It also creates directories that are required
when Apache is (re)started.


To get it to work:
1. Put the source files in the Apache support/win32/ directory.

2. Add the files to MS Visual Studio.

3. Right click on ApacheMonitor and select properties.
   - in the Linker->General section add the directory where
     Apache has his libs (we require libapr etc.) in the section
     'Additional Library Directories'.
   - in the Linker->Input section add the following libraries in 
     the section 'Additional Dependencies':
     comctl32.lib libapr.lib libapriconv.lib libaprutil.lib Ws2_32.lib

Now you should be able to compile it.

In GlobuleMonitor.h you can find the #define for the period that
ApacheMonitor checks the config file at the GBS.

In GlobuleMonitor.c you can find the strings that are searched for in the
config files. I will list them here. If they are changed in the GBS don't
forget to change them here too !

#define GLOBULE_CREATE_DIR      "# GlobuleCreateDir "
#define GLOBULE_CREATE_DIR_LEN  19

#define GLOBULE_ADMIN_URL       "GlobuleAdminUrl "
#define GLOBULE_ADMIN_URL_LEN   16

#define GLOBULE_AUTO_FETCH      "# GlobuleAutoFetch "
#define GLOBULE_AUTO_FETCH_LEN  19

#define GLOBULE_AUTO_RESTART     "# GlobuleAutoRestart "
#define GLOBULE_AUTO_RESTART_LEN 21

#define GBS_CONF_SERIAL         "GlobuleBrokerConfigurationSerial "
#define GBS_CONF_SERIAL_LEN     33

W.
