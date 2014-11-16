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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <map>
#include <stack>
#include <sstream>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <string>
#include <apr.h>
#include <apr_lib.h>
#include <apr_strings.h>
#include "adapt/mapping.h"
#include "adapt/reportreader.h"
#include <stdlib.h>
#include <cstdlib>
using namespace std;

const char* argv0;
int verbosity = 0;
string prefix("/");
enum { COMMON, COMBINED } format = COMMON;
#define linesize 8196

/****************************************************************************/

static int
getinteger(const char* arg, int *valptr)
{
  char *endp;
  *valptr = strtol(arg, &endp, 0);
  if(arg == endp)
    return -1;
  while(*endp == ' ' || *endp == '\t')
    ++endp;
  if(*endp!='\0')
    return -1;
  return 0;
}

static std::string
urldecode(const char* s)
{
  char ch1, ch2;
  std::ostringstream os;
  for(; *s; ++s) {
    switch(*s) {
    case '+':
      os << ' ';
      break;
    case '%':
      if((ch1 = *++s) && (ch2 = *++s)) {
	if(ch1 >= '0' && ch1 <= '9')
	  ch1 -= '0';
	else if(ch1 >= 'a' && ch1 <= 'f')
	  ch1 -= 'a'-10;
	else if(ch1 >= 'A' && ch1 <= 'F')
	  ch1 -= 'A'-10;
	else {
	  os << '?';
	  break;
	}
	if(ch2 >= '0' && ch2 <= '9')
	  ch2 -= '0';
	else if(ch2 >= 'a' && ch2 <= 'f')
	  ch2 -= 'a'-10;
	else if(ch2 >= 'A' && ch2 <= 'F')
	  ch2 -= 'A'-10;
	else {
	  os << '?';
	  break;
	}
	ch2 |= ch1<<4;
	os << ch2;
      }
      break;
    default:
      os << *s;
    }
  }
  return os.str();
}
/****************************************************************************/

struct field {
  apr_uint64_t i;
  string       s;
};

char *
processapacheline(apr_time_t& timestamp, char *line, size_t size,
		  map<string,struct field>* content)
{
#define PARSEFAIL(MATCH) do { if(verbosity > 2) fprintf(stderr,"parse failure on %s\n",MATCH); return NULL; } while(0)
  char *s;
  apr_time_exp_t timespec = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int i, timezone;
  apr_status_t status;

  if(*(s=line) == '#')
    return NULL;
  while(*s && !apr_isspace(*s)) ++s;
  while(*s &&  apr_isspace(*s)) ++s;
  while(*s && !apr_isspace(*s)) ++s;
  while(*s &&  apr_isspace(*s)) ++s;
  while(*s && !apr_isspace(*s)) ++s;
  while(*s &&  apr_isspace(*s)) ++s;
  if(*s != '[') {
    PARSEFAIL("match [");
  } else
    ++s;

  while(apr_isdigit(*s))
    timespec.tm_mday = timespec.tm_mday * 10 + *(s++) - '0';
  if(timespec.tm_mday <= 0 || timespec.tm_mday > 31)
    PARSEFAIL("day");

  if(*s != '/') {
    PARSEFAIL("/ after day");
  } else
    ++s;

  for(i=0; i<12; i++)
    if(!strncmp(s,apr_month_snames[i],strlen(apr_month_snames[i]))) {
      s += strlen(apr_month_snames[i]);
      break;
    }
  if(i==12) {
    PARSEFAIL("recognize month");
  } else
    timespec.tm_mon = i;

  if(*s != '/') {
    PARSEFAIL("/ after month");
  } else
    ++s;

  while(apr_isdigit(*s))
    timespec.tm_year = timespec.tm_year * 10 + *(s++) - '0';
  if(timespec.tm_year < 1900)
    PARSEFAIL("year");
  timespec.tm_year -= 1900;

  if(*s != ':') {
    PARSEFAIL(": after year");
  } else
    ++s;

  if(!apr_isdigit(*s))
    PARSEFAIL("locating hour");
  while(apr_isdigit(*s))
    timespec.tm_hour = timespec.tm_hour * 10 + *(s++) - '0';
  if(timespec.tm_hour > 24)
    PARSEFAIL("hour");

  if(*s != ':') {
    PARSEFAIL(": after hour");
  } else
    ++s;

  if(!apr_isdigit(*s))
    PARSEFAIL("locating minut");
  while(apr_isdigit(*s))
    timespec.tm_min = timespec.tm_min * 10 + *(s++) - '0';
  if(timespec.tm_min > 60)
    PARSEFAIL("minut");

  if(*s != ':') {
    PARSEFAIL(": after minut");
  } else
    ++s;

  if(!apr_isdigit(*s))
    PARSEFAIL("locating seconds");
  while(apr_isdigit(*s))
    timespec.tm_sec = timespec.tm_sec * 10 + *(s++) - '0';
  if(timespec.tm_sec > 60)
    PARSEFAIL("seconds");

  if(*s != ' ') {
    PARSEFAIL("space before timezone");
  } else
    ++s;

  if(*s != '+' && *s != '-')
    PARSEFAIL("sign on timezone");
  if(!apr_isdigit(s[1]) || !apr_isdigit(s[2]) ||
     !apr_isdigit(s[3]) || !apr_isdigit(s[4]))
    PARSEFAIL("timezone");
  timezone = (s[1]-'0')*1000 + (s[2]-'0')*100 + (s[3]-'0')*10 + (s[4]-'0');
  if(*s == '-')
    timezone *= -1;
  timezone = timezone * 36;
  s += 5;

  if(*s != ']')
    PARSEFAIL("match ]");

#ifdef NOTDEFINED
  fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d.%03d (%d,%d,%d,%d)\n",
          timespec.tm_year+1900, timespec.tm_mon+1, timespec.tm_mday,
	  timespec.tm_hour, timespec.tm_min, timespec.tm_sec,
	  timespec.tm_usec,
	  timespec.tm_wday, timespec.tm_yday,
	  timespec.tm_isdst, timespec.tm_gmtoff);
#endif

  timestamp = 0;
  status = apr_time_exp_get(&timestamp, &timespec);
  if(status != APR_SUCCESS)
    PARSEFAIL("interpreting time");
  //timestamp += apr_time_from_sec(timezone);
  return line;
#undef PARSEFAIL
}

char *
processreportline(apr_time_t& timestamp, char *line, size_t size,
		  map<string,struct field>* content)
{
    apr_status_t status;
    char ch;
    char *s, *k, *v, *aux;
    bool numberic;

    for(s=k=line,v=NULL,ch='\0'; apr_isspace(*s); s++)
      ;
    if(*s && *s != '#') {
      for(map<string,struct field>::iterator iter=content->begin();
          iter != content->end();
          ++iter)
        iter->second.s = "";
      numberic = false;
      for(s=k=line,v=NULL,ch='\0'; *s; s++)
        if(ch=='\0' || *s==ch || (apr_isspace(*s) && ch==' '))
          switch(*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
            *s = '\0';
            if(*k)
              if(k[1] != '\0' || v) {
                map<string,struct field>::iterator iter = content->find(k);
                if(iter != content->end()) {
                  iter->second.s = string(v?v:"");
                  iter->second.i = apr_strtoi64(v, &aux, 0);
                }
              } else {
                (*content)[""].s = k;
                (*content)[""].i = 0LL;
              }
            k = &s[1];
            v = NULL;
            ch = '\0';
            break;
          case ':':
          case ';':
          case '=':
            numberic = (*s == ':');
            *s = '\0';
            v = &s[1];
            ch = (*s == '=' ? 0xff : ' ');
            break;
          }
      if(*k)
        if(k[1] != '\0' || v) {
          map<string,struct field>::iterator iter = content->find(k);
          if(iter != content->end()) {
            iter->second.s = v;
            iter->second.i = apr_strtoi64(v, &aux, 0);
          }
        } else {
          (*content)[""].s = k;
          (*content)[""].i = 0LL;
        }

      if((*content)[""].s != "R")
        return NULL;

      // use the line
      char timestr[30] = "-";
      apr_time_exp_t xt;
      if((*content)["t"].i != 0) {
        status = apr_time_exp_gmt(&xt, (*content)["t"].i);
        if(status == APR_SUCCESS) {
	  timestamp = (*content)["t"].i;
          apr_snprintf(timestr, sizeof(timestr)-1,
                       "[%02d/%s/%d:%02d:%02d:%02d +0000]",
                       xt.tm_mday, apr_month_snames[xt.tm_mon],
                       xt.tm_year+1900, xt.tm_hour, xt.tm_min, xt.tm_sec);
	} else
          return NULL;
      }
      string url = prefix + (*content)["path"].s.c_str();
      if(url == "")
        url = "-";
      switch(format) {
      case COMMON:
	snprintf(line, size, "%s - - %s \"GET %s HTTP/1.1\" 200 %s\n",
		 ((*content)["client"].s=="" ? "-" : (*content)["client"].s.c_str()),
		 timestr, url.c_str(),
		 ((*content)["sndsize"].s=="" ? "-" : (*content)["sndsize"].s.c_str()));
	break;
      case COMBINED:
	snprintf(line, size, "%s - - %s \"GET %s HTTP/1.1\" 200 %s \"%s\" \"%s\"\n",
		 ((*content)["client"].s=="" ? "-" : (*content)["client"].s.c_str()),
		 timestr, url.c_str(),
		 ((*content)["sndsize"].s=="" ? "-" : (*content)["sndsize"].s.c_str()),
		 ((*content)["referer"].s=="" ? "-" : urldecode((*content)["referer"].s.c_str()).c_str()),
		 ((*content)["browser"].s=="" ? "-" : urldecode((*content)["browser"].s.c_str()).c_str()));
	break;
      }
    }
    return line;
}

class buffer
{
public:
  int inputno;
  int lineno;
  string line;
  inline buffer(int num, int l, const char* content) : inputno(num), lineno(l), line(content) { };
};
struct input {
  const char *fname;
  apr_file_t *fdes;
  char *(*procfunc)(apr_time_t&,char*,size_t,map<string,struct field>*);
  int flineno;        /* line number in file */
  apr_time_t last;    /* last written timestamp */
  int lastlineno;
  apr_time_t current;
  char line[linesize];
};

void
process(apr_pool_t* p, apr_time_t window, int ninputs, char **fnames)
{
  apr_status_t status;
  apr_time_t now;
  int earliest;
  unsigned int lookaheadmaxsize = 0;
  apr_file_t* fstdout;
  struct input *inputs;
  map<string,struct field> content;
  multimap<const apr_time_t,buffer> lookaheads;
  struct field empty = { 0LL, "" };
  content[""]        = empty;
  content["t"]       = empty;
  content["client"]  = empty;
  content["sndsize"] = empty;
  content["path"]    = empty;

  apr_file_open_stdout(&fstdout, p);

  inputs = (struct input *) apr_palloc(p, sizeof(struct input) * ninputs);
  for(int i=0; i<ninputs; i++) {
    inputs[i].fname = fnames[i];
    if(!strcmp(inputs[i].fname,"-"))
      status = apr_file_open_stdin(&inputs[i].fdes, p);
    else
      status = apr_file_open(&inputs[i].fdes,inputs[i].fname, APR_READ|APR_BUFFERED,APR_OS_DEFAULT, p);
    if(status != APR_SUCCESS) {
      fprintf(stderr,"%s: Cannot open file %s\n",argv0,inputs[i].fname);
      inputs[i].fdes = NULL;
    }
    inputs[i].procfunc      = NULL;
    inputs[i].flineno       = 0;
    inputs[i].last          = 0;
    inputs[i].lastlineno    = 0;
  }

  for(int i=0; i<ninputs; i++) {
    status = apr_file_gets(inputs[i].line, linesize, inputs[i].fdes);
    if(status == APR_SUCCESS) {
      inputs[i].flineno++;
      if(processapacheline(inputs[i].current, inputs[i].line, linesize, &content)) {
#ifdef NOTDEFINED
        { apr_time_exp_t timespec = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        apr_time_exp_gmt(&timespec,inputs[i].current);
        printf("%04d-%02d-%02d %02d:%02d:%02d.%03d\n",
               timespec.tm_year+1900, timespec.tm_mon+1, timespec.tm_mday,
               timespec.tm_hour, timespec.tm_min, timespec.tm_sec,
               timespec.tm_usec); }
#endif
          inputs[i].procfunc = processapacheline;
          if(verbosity > 1)
	    fprintf(stderr,"Input %s is in Apache common or combined log format\n",inputs[i].fname);
      } else {
        if(processreportline(inputs[i].current, inputs[i].line, linesize, &content)) {
          if(verbosity > 1)
	    fprintf(stderr,"Input %s is in Globule log format\n",inputs[i].fname);
        } else {
          if(verbosity > 1)
	    fprintf(stderr,"Input %s is assumed to be in Globule log format\n",inputs[i].fname);
          do {
            status = apr_file_gets(inputs[i].line, linesize, inputs[i].fdes);
            if(status == APR_SUCCESS) {
              inputs[i].flineno++;
              if(processreportline(inputs[i].current, inputs[i].line, linesize, &content))
                break;
            } else {
              if(verbosity > 0)
                fprintf(stderr,"%s: warning skipped over entire content of %s\n",argv0,inputs[i].fname);
              if(strcmp(inputs[i].fname,"-"))
                apr_file_close(inputs[i].fdes);
            }
          } while(status == APR_SUCCESS);
	}
        inputs[i].procfunc = processreportline;
      }
    } else {
      if(strcmp(inputs[i].fname,"-"))
        apr_file_close(inputs[i].fdes);
      inputs[i].fdes = NULL;
    }
  }

  now = 0;
  for(int i=0; i<ninputs; i++)
    if(inputs[i].fdes && (now == 0 || now > inputs[i].current))
      now = inputs[i].current;

  for(;;) {
    earliest = -1;
    for(int i=0; i<ninputs; i++)
      if(inputs[i].fdes && (earliest < 0 || inputs[i].current < inputs[earliest].current))
        earliest = i;
    if(earliest < 0)
      break;
    if(now + window < inputs[earliest].current && lookaheadmaxsize < lookaheads.size()) {
      if(verbosity > 2 && lookaheads.size() - lookaheadmaxsize > 100)
        fprintf(stderr,"Lookahead buffer now contains %d lines\n",lookaheads.size());
      lookaheadmaxsize = lookaheads.size();
    }
    while(now + window < inputs[earliest].current && lookaheads.size() > 0) {
      multimap<const apr_time_t,buffer>::iterator iter = lookaheads.begin();
      apr_file_puts(iter->second.line.c_str(), fstdout);
      inputs[iter->second.inputno].last = iter->first;
      inputs[iter->second.inputno].lastlineno = iter->second.lineno;
      now = iter->first;
      lookaheads.erase(iter);
    }

    lookaheads.insert(make_pair(inputs[earliest].current, buffer(earliest,inputs[earliest].flineno,inputs[earliest].line)));
    while(inputs[earliest].fdes) {
      status = apr_file_gets(inputs[earliest].line, linesize, inputs[earliest].fdes);
      if(status == APR_SUCCESS) {
        inputs[earliest].flineno++;
        if(inputs[earliest].procfunc(inputs[earliest].current, inputs[earliest].line, linesize, &content)) {
#ifdef NOTDEFINED
          { apr_time_exp_t timespec = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
          apr_time_exp_gmt(&timespec,inputs[earliest].current);
          printf("%04d-%02d-%02d %02d:%02d:%02d.%03d   %s",
                 timespec.tm_year+1900, timespec.tm_mon+1, timespec.tm_mday,
                 timespec.tm_hour, timespec.tm_min, timespec.tm_sec,
                 timespec.tm_usec,inputs[earliest].line); }
#endif
            if(inputs[earliest].current < inputs[earliest].last) {
              fprintf(stderr,"%s: Error: lookahead of %d seconds too small for input file %s on line %d (require more than %d seconds)\n", argv0, (int)apr_time_sec(window), inputs[earliest].fname, inputs[earliest].flineno, (int)apr_time_sec(inputs[earliest].last - inputs[earliest].current - 1));
              if(verbosity > 0) {
                fprintf(stderr,"read timestamp of ");
                { apr_time_exp_t timespec = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                apr_time_exp_gmt(&timespec,inputs[earliest].current);
                fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                        timespec.tm_year+1900, timespec.tm_mon+1, timespec.tm_mday,
                        timespec.tm_hour, timespec.tm_min, timespec.tm_sec,
                        timespec.tm_usec); }
                fprintf(stderr," on line %d while last output was from line %d ",
                        inputs[earliest].flineno, inputs[earliest].lastlineno);
                { apr_time_exp_t timespec = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
                apr_time_exp_gmt(&timespec,inputs[earliest].last);
                fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d.%03d\n",
                        timespec.tm_year+1900, timespec.tm_mon+1, timespec.tm_mday,
                        timespec.tm_hour, timespec.tm_min, timespec.tm_sec,
                        timespec.tm_usec); }
                fprintf(stderr,"\n");
              }
	      continue; //return;
            }
            break;
        }
      } else {
        if(strcmp(inputs[earliest].fname,"-"))
          apr_file_close(inputs[earliest].fdes);
        inputs[earliest].fdes = NULL;
      }
    }
  }

  while(lookaheads.size() > 0) {
    multimap<const apr_time_t,buffer>::iterator iter = lookaheads.begin();
    apr_file_puts(iter->second.line.c_str(), fstdout);
    lookaheads.erase(iter);
  }
  if(verbosity > 1)
    fprintf(stderr,"Lookahead buffer maximum size was %d lines\n",lookaheadmaxsize);
}


int
main(int argc, char *argv[])
{
  apr_pool_t* pool;
  apr_allocator_t *allocer;
  int ch;
  int lookahead = 0;

#ifdef HAVE_GETOPT_LONG
  static struct option longopts[] = {
    { "verbose",          2, 0, 'v' },
    { "help",             0, 0, 'h' },
    { "prefix",           1, 0, 'p' },
    { "format",           1, 0, 'f' },
    { "lookahead-window", 1, 0, 'w' },
    { 0, 0, 0, 0 }
  };
#define GNUGETOPT(X1,X2,X3,X4,X5) getopt_long(X1,X2,X3,X4,X5)
#else
#define GNUGETOPT(X1,X2,X3,X4,X5) getopt(X1,X2,X5)
#endif

  /* Get the name of the program */
  if((argv0 = strrchr(argv[0],'/')) == NULL)
    argv0 = argv[0];
  else
    ++argv0;

  /* give error on depricated usage */
  if(argc>1 && !strcmp(argv[1],"report2clf")) {
    fprintf(stderr,"%s: depricated use of command line parameters, use -h for "
	    "help.\n",argv0);
    exit(1);
  }

  /* Parse options */
  opterr = 0;
  while((ch = GNUGETOPT(argc, argv,"hv::p:f:w:",longopts,NULL)) >= 0) {
    switch(ch) {
    case 'v':
      if(optarg) {
        if(getinteger(optarg,&verbosity))
          goto badnumber;
      } else
	++verbosity;
      break;
    case 'h':
             /*34567890123456789012345678901234567890123456789012345678901234567890123456*/
      printf("\nUsage: %s [-hv] -p<prefix> -f<format> <filename>...\n\n",argv[0]);
      //printf("%*.*s \n\n",strlen(argv[0]),strlen(argv[0]),"");
      printf("  -h  --help\n");
      printf("      For this friendly reminder of options.\n\n");
      printf("  -v  --verbose[=<num>]\n");
      printf("      Increase the verbosity of the program.\n\n");
      printf("  -p  --prefix=<prefix>\n");
      printf("      Prepend each URL with the given prefix.\n\n");
      printf("  -f  --format=(common|combined)\n");
      printf("      Produce an access log in either common or combined log format\n\n");
      printf("  -w  --lookahead-window=seconds\n");
      printf("      Use an overall lookahead buffer for maximum of n seconds\n\n");
      exit(0);
    case 'p':
      prefix = optarg;
      if(prefix[0] != '/')
	prefix = "/" + prefix;
      if(prefix[prefix.length()-1] != '/')
	prefix = prefix + "/";
      break;
    case 'f':
      if(!strcasecmp(optarg,"common") || !strcasecmp(optarg,"clf"))
	format = COMMON;
      else if(!strcasecmp(optarg,"combined"))
	format = COMBINED;
      break;
    case 'w':
      if(getinteger(optarg,&lookahead))
        goto badnumber;
      break;
    case ':':
      fprintf(stderr,"%s: missing option argument to -%c\n",argv0,optopt);
      exit(1);
    case '?':
      fprintf(stderr,"%s: unrecognized option -%c\n",argv0,optopt);
      exit(1);
    }
  }

  if(apr_allocator_create(&allocer) ||
     apr_pool_create_ex(&pool, NULL, NULL, allocer)) {
    fprintf(stderr,"%s: Cannot initialize apr library\n",argv0);
    return 1;
  }

  process(pool, apr_time_from_sec(lookahead), argc - optind, &argv[optind]);

  apr_pool_destroy(pool);
  apr_allocator_destroy(allocer);
  exit(0);

badnumber:
  fprintf(stderr,"%s: bad argument to option -%c\n",argv0,optopt);
  exit(1);
}
