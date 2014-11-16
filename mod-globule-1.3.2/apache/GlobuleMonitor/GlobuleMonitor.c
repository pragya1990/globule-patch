/*
Copyright (c) 2003-2006, Vrije Universiteit
All rights reserved.

Redistribution and use of the GLOBULE system in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

   * Neither the name of Vrije Universiteit nor the names of the software
     authors or contributors may be used to endorse or promote
     products derived from this software without specific prior
     written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL VRIJE UNIVERSITEIT OR ANY AUTHORS OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This product includes software developed by the Apache Software Foundation
<http://www.apache.org/>.
*/
#include "GlobuleMonitor.h"

#include <apr_strings.h>
#include <apr_file_io.h>
#include <apr_uri.h>

#define CRLF   "\r\n"

#define CONFIG_SUFFIX       "\\conf\\httpd.conf"
#define CONFIG_SUFFIX_LEN   16

#define BUF_SIZE            512
#define BIG_BUF_SIZE        8192

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

#define NEW_CONF_SUFFIX         ".gbs"
#define NEW_CONF_SUFFIX_LEN     4

/* prototypes of other functions */
apr_status_t CreateDirs(const char *configFilename, apr_pool_t *pool);
int CheckConfig(const char *configFilename, apr_pool_t *pool);
int parseConfig(const char *configFilename, char *gAdminURL, char *gAutoFetch, 
                char *gbsConfSerial, int *gAutoRestart, apr_pool_t *pool);
int getCurrentSerial(const char *gAdminURL, char *gbsConfSerial, apr_pool_t *pool);
int getNewConfigFile(const char *gAutoFetch, const char *newConfigFilename, apr_pool_t *pool);


/** Creates the directories that are found in the config file.
 * Without the directories, Apache won't start.
 * New directories might be needed when a new config file is downloaded.
 *
 * szImagePath: string containing the patht to the Apache executable
 *              as found in the registry.
 * Example: "C:\Program Files\Apache Group\Apache2\bin\Apache.exe" -k runservice
 */
int GlobuleCreateDirs(const char* configFilename, apr_pool_t *pool) {

  apr_status_t status;
  apr_pool_t *subpool;

  // Make sure the filename was set, if not, skip this.
  if (configFilename == NULL) {
    return 0;
  }
  
  // Create a subpool so that we clean all memory we use
  status = apr_pool_create_ex(&subpool, pool, NULL, NULL);
  if (!APR_STATUS_IS_SUCCESS(status)) {
    return 0;
  }
  apr_pool_tag(subpool, "GlobuleMonitor");

  // Opens the file, parses it and creates the directories needed.  
  if (!APR_STATUS_IS_SUCCESS(CreateDirs(configFilename, subpool))) {
    apr_pool_destroy(subpool);
    return 0;
  }

  // Clean up
  apr_pool_destroy(subpool);
  return 1;
}

/* Retrieve the config file name given the szImagePath
 * szImagePath: string containing the path to the Apache executable
 *              as found in the registry.
 * Example: "C:\Program Files\Apache Group\Apache2\bin\Apache.exe" -k runservice
 *
 * If something goes wrong, the configFilename will remain NULL.
 */
void GlobuleGetConfigFilename(char **configFilename, const char *szImagePath) {
  
  char *temp = NULL;
  int len, start;
  *configFilename = NULL;

  // Where is \bin\ in the path.
  temp = strstr(szImagePath, "\\bin\\");  // temp now points to where \bin\ is.
  if (temp == NULL) {
    // Couldn't find it
    return;
  }
  
  // Determine the length of the new string
  len = temp - szImagePath;
  
  // Start position of the string to copy.
  start = 0;
  
  // We don't want a '"' in our string, move start one position, consequently
  // new string length is one less.
  if (szImagePath[0] == '"') {
    start++;
    len--;
  }
  
  *configFilename = calloc(len + CONFIG_SUFFIX_LEN + 1, sizeof(char));
  strncat(*configFilename, &szImagePath[start], len);
  strncat(&((*configFilename)[len]), CONFIG_SUFFIX, CONFIG_SUFFIX_LEN);
}

/* Checks the config file for the current Apache.
 * configFilename -> file to parse and fetch a (new) config file for.
 * returns : 0 config file is the same
 *           1 config file is new(er)
 */
int GlobuleCheckConfig(const char *configFilename, apr_pool_t *pool) {
  apr_status_t status;
  apr_pool_t *subpool;
  int ret = 0;

  // Make sure the filename was set, if not, skip this.
  if (configFilename == NULL) {
    return 0;
  }
  
  // Create a subpool so that we clean all memory we use
  status = apr_pool_create_ex(&subpool, pool, NULL, NULL);
  if (!APR_STATUS_IS_SUCCESS(status)) {
    return 0;
  }
  apr_pool_tag(subpool, "GlobuleMonitor");

  // Opens the file, parses it and fetches the new one, compares etc.
  // returns 1 if new config needs to be loaded
  ret = CheckConfig(configFilename, subpool);

  // Clean up
  apr_pool_destroy(subpool);
  return ret;
}

// Parses the config file and creates the directories.
apr_status_t CreateDirs(const char *configFilename, apr_pool_t *pool) {

  char buf[BUF_SIZE];
  char *crlf = NULL;
  int len = 0;
  apr_status_t status;
  apr_file_t *cfgFile;
  
  // Open the config file.
  status = apr_file_open(&cfgFile, configFilename, APR_READ,
                         APR_OS_DEFAULT, pool);
  if (!APR_STATUS_IS_SUCCESS(status)) {
    return status;
  }
  
  // Parse it and create the directory
  while (APR_STATUS_IS_SUCCESS(apr_file_eof(cfgFile))) {
    memset(buf, 0, BUF_SIZE);
    status = apr_file_gets(buf, BUF_SIZE, cfgFile);
    if (!APR_STATUS_IS_SUCCESS(status)) {
      apr_file_close(cfgFile);
      return status;
    }
    
    if (strstr(buf, GLOBULE_CREATE_DIR) == NULL) {
      continue;
    }
    
    // strip off the CRLF, otherwise recursive creation does not work
    if ((crlf = strchr(buf, '\r')) != NULL ||
        (crlf = strchr(buf, '\n')) != NULL) {
      *crlf = '\0';
    }
    
    // We got a string like:
    // # GlobuleCreateDir D:/Apache2/test.globeworld.net/htdocs

    // Make directory recursively, skip the GlobuleCreateDir part
    apr_dir_make_recursive(&buf[GLOBULE_CREATE_DIR_LEN], APR_OS_DEFAULT, pool);
  }
  
  apr_file_close(cfgFile);
  return APR_SUCCESS;
}


int CheckConfig(const char *configFilename, apr_pool_t *pool) {
  
  int restart = 0;
  int newer = 0;
  char newConfigFilename[BUF_SIZE];
  
  char cur_gAdminURL[BUF_SIZE];     // cur config file stuff
  char cur_gAutoFetch[BUF_SIZE];
  char cur_gbsConfSerial[BUF_SIZE];
  int  cur_gAutoRestart = 0;
  
  char run_gbsConfSerial[BUF_SIZE]; // What Apache is actually running
  int  run = 0;
  
  char new_gAdminURL[BUF_SIZE];     // what does the new config file say ?
  char new_gAutoFetch[BUF_SIZE];
  char new_gbsConfSerial[BUF_SIZE];
  int  new_gAutoRestart = 0;
  int  new = 0;
  
  // Clear strings
  memset(cur_gAdminURL, 0, BUF_SIZE);
  memset(cur_gAutoFetch, 0, BUF_SIZE);
  memset(cur_gbsConfSerial, 0, BUF_SIZE);

  memset(run_gbsConfSerial, 0, BUF_SIZE);

  memset(new_gAdminURL, 0, BUF_SIZE);
  memset(new_gAutoFetch, 0, BUF_SIZE);
  memset(new_gbsConfSerial, 0, BUF_SIZE);

  // Find out what the adminURL, auto fetch URL and the current serial are
  if (!parseConfig(configFilename, cur_gAdminURL, cur_gAutoFetch, cur_gbsConfSerial,
                   &cur_gAutoRestart, pool)) {
    // could not get this file, so I can't get current running and new config either
    return restart;
  }
  
  if (!cur_gAutoRestart) {
    // The user does not want us to fetch new config and restart, pull out.
    return restart;
  }
  
  if (strlen(cur_gAdminURL) == 0 || strlen(cur_gAutoFetch) == 0 || strlen(cur_gbsConfSerial) == 0) {
    // Could not gather all the information I needed from the config file.
    return restart;
  }
  
  // See if we can find out what the current apache is running
  if (!getCurrentSerial(cur_gAdminURL, run_gbsConfSerial, pool)) {
    // Could not get new current running config, but just continue
  }
  
  // Retrieve the new config file
  strcpy(newConfigFilename, configFilename);
  strncat(newConfigFilename, NEW_CONF_SUFFIX, NEW_CONF_SUFFIX_LEN);
  
  if (!getNewConfigFile(cur_gAutoFetch, newConfigFilename, pool)) {
    // Could not get new config file, but just continue
  }
  
  // Find out what the adminURL, and current serial are in the new config
  parseConfig(newConfigFilename, new_gAdminURL, new_gAutoFetch, new_gbsConfSerial,
              &new_gAutoRestart, pool);
  
  // Start comparing the stuff.
  run = strlen(run_gbsConfSerial);
  new = strlen(new_gbsConfSerial);
  
  // - Apache was not running/not able to retrieve globule gbs serial number
  // - new config file was downloaded
  // --> if new config is newer than old, replace old
  if (!run && new) {
    // newer depends on if old and new are different
    if (strcmp(cur_gbsConfSerial, new_gbsConfSerial) != 0) {
      newer = 1;
    }
  }
  
  if (run) {
    // Apache is running
    // Check if current config serial is newer than running
    if (strcmp(cur_gbsConfSerial, run_gbsConfSerial) != 0) {
      restart = 1;
    }
    // Check if the newer config file serial is newer than running
    if (new) {
      // We were able to retrieve a new config file, huuray !
      if (strcmp(new_gbsConfSerial, run_gbsConfSerial) != 0) {
        // new config file has newer serial than running
        restart = 1;
        newer = 1;
      } else if (strcmp(cur_gbsConfSerial, new_gbsConfSerial) != 0) {
        // new config was not newer than running, but newer than old one (?)
        // Maybe someone put a backup back, replace it with the newer.
        newer = 1;
      }
    }
  }
  
  if (newer) {
    // overwrite the current httpd.conf with httpd.conf.new
    apr_file_copy(newConfigFilename, configFilename, APR_FILE_SOURCE_PERMS, pool);
  }

  return restart;
}

int parseConfig(const char* configFilename, char *gAdminURL, char *gAutoFetch,
                char *gbsConfSerial, int *gAutoRestart, apr_pool_t *pool) {
                
  char buf[BUF_SIZE];
  char *crlf = NULL;
  apr_status_t status;
  apr_file_t *cfgFile;                
                
  // Open the config file
  status = apr_file_open(&cfgFile, configFilename, APR_READ,
                         APR_OS_DEFAULT, pool);
  if (!APR_STATUS_IS_SUCCESS(status)) {
    return 0;
  }                
                
  // Find out the following things from the config file
  // 1. GlobuleAdminURL
  // 2. GlobuleAutoFetch
  // 3. GlobuleBrokerConfigurationSerial
  while (APR_STATUS_IS_SUCCESS(apr_file_eof(cfgFile))) {
    memset(buf, 0, BUF_SIZE);
    status = apr_file_gets(buf, BUF_SIZE, cfgFile);
    if (!APR_STATUS_IS_SUCCESS(status)) {
      // Did hit EOF ?
      break;
    }

    // strip off the CRLF, otherwise recursive creation does not work
    if ((crlf = strchr(buf, '\r')) != NULL ||
        (crlf = strchr(buf, '\n')) != NULL) {
      *crlf = '\0';
    }
 
    // Is it an AdminURL ?   
    if (strstr(buf, GLOBULE_ADMIN_URL) != NULL) {
      // Only store the first occurence
      if (strlen(gAdminURL) == 0) {
        strcpy(gAdminURL, &buf[GLOBULE_ADMIN_URL_LEN]);
      }
      continue;
    }
    
    // Is it an AutoFetch ?
    if (strstr(buf, GLOBULE_AUTO_FETCH) != NULL) {
      if (strlen(gAutoFetch) == 0) {
        strcpy(gAutoFetch, &buf[GLOBULE_AUTO_FETCH_LEN]);
      }
      continue;
    }
    
    // Is it an AutoRestart ?
    if (strstr(buf, GLOBULE_AUTO_RESTART) != NULL) {
      if (buf[GLOBULE_AUTO_RESTART_LEN] == 'y'
          || buf[GLOBULE_AUTO_RESTART_LEN] == 'Y') {
        *gAutoRestart = 1;
       }
       continue;
    }
    
    // Is it an GlobuleBrokerSerialConfig ?
    if (strstr(buf, GBS_CONF_SERIAL) != NULL) {
      if (strlen(gbsConfSerial) == 0) {
        strcpy(gbsConfSerial, &buf[GBS_CONF_SERIAL_LEN]);
      }
      continue;
    }
  }
  apr_file_close(cfgFile);
  return 1;
}


// Given the url, fetch the result and try to parse the current serial number.
// returns 1 if success, 0 if fail and gbsConfSerial will have 0 size
int getCurrentSerial(const char *gAdminURL, char *gbsConfSerial, apr_pool_t *pool) {
  apr_socket_t *sock;
  apr_status_t err, rv;
  apr_size_t nbytes, len;
  char buffer[BIG_BUF_SIZE];
  char *start = NULL;
  char *end = NULL;
  int done = 0, toFile=0;
  apr_sockaddr_t *connect_addr;
  apr_uri_t uri;
  apr_port_t port;

  // Retrieve the data from the URL pointed to by gAdminRUL
  if (!APR_STATUS_IS_SUCCESS(apr_uri_parse(pool, gAdminURL, &uri))) {
    return 0;
  }
  
  // Determine the port
  port = apr_uri_port_of_scheme(uri.scheme);

  // do a DNS lookup for the destination host
  err = apr_sockaddr_info_get(&connect_addr, uri.hostname, APR_UNSPEC, port, 0, pool);
  
  if (!APR_STATUS_IS_SUCCESS(err)) {
    // DNS failed
    return 0;
  }
  
  // Connect
  rv = apr_socket_create(&sock, connect_addr->family, SOCK_STREAM, pool);
  if (!APR_STATUS_IS_SUCCESS(rv)) {
    // Could not create socket...
    return 0;
  }
  
  // make the connection
  rv = apr_socket_connect(sock, connect_addr);
  if (!APR_STATUS_IS_SUCCESS(rv)) {
    apr_socket_close(sock);
    return 0;
  }
  
  // Sent the request
  // GET /globulectl/gbs HTTP/1.0
  // Host: wilfred.cs.vu.nl
  nbytes = apr_snprintf(buffer, sizeof(buffer),
                        "GET %sgbs HTTP/1.0%sHost: %s:%u%s%s",               // use 1.0 so we have no chunked encoding
                        uri.path, CRLF, uri.hostname, port, CRLF, CRLF);
  apr_send(sock, buffer, &nbytes);

  // Cross you fingers we get all data at once (8k)
  while (!done) {
    nbytes = sizeof(buffer);
    memset(buffer, 0, nbytes);
    rv = apr_socket_recv(sock, buffer, &nbytes);
    
    if (!APR_STATUS_IS_SUCCESS(rv)) {
      // We hit EOF, but there maybe data in the buffer.
      done = 1;
    }

    // Find the place where the actual data starts, CRLFCRLF<gbsserial>CRLF
    if ((start = strstr(buffer, "\r\n\r\n")) != NULL) {
      if ((end = strstr(&start[4], "\n")) != NULL) { // skip the CRLFCRLF of start, hmms, end in just \n no \r
        // Got it ! move the start position 4 places
        start += 4;
        len = end - start;
        
        // Copy the string over (there are already \0 in gbsConfSerial
        // because of the memset. The strings must be appended and prepende with "
        gbsConfSerial[0] = '"';
        strncpy(&gbsConfSerial[1], start, len);
        gbsConfSerial[len+1] = '"';
        
        apr_socket_close(sock);
        return 1;
      }
    } else {
      // Whoopty doo, you have a problem on your hands !
    }
  }
 
  // Did not quite get it..
  apr_socket_close(sock);
  return 0;
}


// Given the url, fetch the new config and store it in newConfigFilename
int getNewConfigFile(const char *gAutoFetch, const char *newConfigFilename,
                     apr_pool_t *pool) {

  apr_socket_t *sock;
  apr_status_t err, rv;
  apr_size_t nbytes, len;
  char buffer[BIG_BUF_SIZE];
  char *temp = NULL;
  int done = 0, toFile=0;
  apr_sockaddr_t *connect_addr;
  apr_uri_t uri;
  apr_port_t port;

  apr_file_t *cfgFile;                
                
  // Open the new config file, overwrite old one.
  rv = apr_file_open(&cfgFile, newConfigFilename, APR_WRITE | APR_CREATE | APR_TRUNCATE,
                         APR_OS_DEFAULT, pool);
  if (!APR_STATUS_IS_SUCCESS(rv)) {
    return 0;
  }                

  // Retrieve the data from the URL pointed to by gAutoFetch and save it
  // as newConfigFilename. If it fails, return 0.
  if (!APR_STATUS_IS_SUCCESS(apr_uri_parse(pool, gAutoFetch, &uri))) {
    apr_file_close(cfgFile);
    return 0;
  }
  
  // Determine the port
  port = apr_uri_port_of_scheme(uri.scheme);

  // do a DNS lookup for the destination host
  err = apr_sockaddr_info_get(&connect_addr, uri.hostname, APR_UNSPEC, port, 0, pool);
  
  if (!APR_STATUS_IS_SUCCESS(err)) {
    // DNS FAILED...
    apr_file_close(cfgFile);
    return 0;
  }
  
  // Connect
  rv = apr_socket_create(&sock, connect_addr->family, SOCK_STREAM, pool);
  if (!APR_STATUS_IS_SUCCESS(rv)) {
    // Could not create socket...
    apr_file_close(cfgFile);
    return 0;
  }
  
  // make the connection
  rv = apr_socket_connect(sock, connect_addr);
  if (!APR_STATUS_IS_SUCCESS(rv)) {
    apr_socket_close(sock);
    apr_file_close(cfgFile);
    return 0;
  }
  
  // Sent the request,ex:
  // GET /config.php?serverID=4&userID=4 HTTP/1.0
  // Host: www.globeworld.net:80
  nbytes = apr_snprintf(buffer, sizeof(buffer),
                        "GET %s?%s HTTP/1.0%sHost: %s:%u%s%s",               // use 1.0 so we have no chunked encoding
                        uri.path, uri.query, CRLF, uri.hostname, port, CRLF, CRLF);
  apr_send(sock, buffer, &nbytes);

  // Read in the response line by line till we get a line of the config file.
  while (!done) {
    nbytes = sizeof(buffer);
    memset(buffer, 0, nbytes);
    rv = apr_socket_recv(sock, buffer, &nbytes);
    
    if (!APR_STATUS_IS_SUCCESS(rv)) {
      // We hit EOF, but there maybe data in the buffer.
      done = 1;
    }
    
    if (!toFile) {
      // See if we are getting to the part yet where the config file starts.
      // Assume the first time we see '#' as the start of the httpd.conf
      if ((temp = strstr(buffer, "#")) != NULL) {
        len = nbytes - (temp - buffer);
        apr_file_write(cfgFile, temp, &len);
        // Now keep on writing everything to file
        toFile = 1;
      }
    } else {
      // Write to the file
      apr_file_write(cfgFile, buffer, &nbytes);
    }
  }
 
  apr_socket_close(sock); 
  apr_file_flush(cfgFile);
  apr_file_close(cfgFile);
  return 1;
}
