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
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <string>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

using namespace std;

char *argv0;

class histogram
{
public:
  int nbuckets;
  int *buckets;
  int start, size;
  int below, above;
  histogram(int lbnd, int ubnd, int num)
    : nbuckets(num), below(0), above(0)
  {
    start = lbnd;
    size  = (ubnd-lbnd) / num;
    buckets = new int[num];
    for(int i=0; i<num; i++)
      buckets[i] = 0;
  }
  int add(int value) {
    int bucketnum = (value-start)/size;
    if(bucketnum < 0)
      below++;
    else if(bucketnum >= nbuckets)
      above++;
    else {
      buckets[bucketnum]++;
    }
    return value;
  }
};

ostream& operator<< (ostream &ost, const histogram &h) {
  int count = 0;
  float ratio;
  ost << "# below threshold " << h.below << endl;
  for(int i=0; i<h.nbuckets; i++) {
    count += h.buckets[i];
    ost << i*h.size+h.size/2+h.start << "\t" << h.buckets[i] << endl;
  }
  ost << "# above threshold " << h.above << endl;
  ratio = count/(double)(count+h.above+h.below);
  ost << "# ratio matched " << ratio << endl;
  return ost;
}

class generator
{
protected:
  gsl_rng *r;
  long _seed;
  long nextinterval;
  long (generator::*fn)(void);
  int mean, stddev, lbnd, ubnd;
  long runtime, curtime;
  void init(long seed)
  {
    r = gsl_rng_alloc(gsl_rng_rand48);
    gsl_rng_set(r, _seed = seed);
    do {
      nextinterval = (this->*fn)();
    } while(nextinterval < 0);
  }
  void init()
  {
    init(time(NULL));
  }
public:
  generator(long runtime, int lrange, int urange)
    : runtime(runtime), curtime(0)
  {
    stddev = (urange-lrange)/2;
    mean   = lrange + stddev;
    fn     = &generator::getNextGauss;
    init();
  }
  generator(long runtime, int meanvalue)
    : runtime(runtime), curtime(0)
  {
    mean = meanvalue;
    fn   = &generator::getNextGamma;
    init();
  }
  generator(int lowerbnd, int upperbnd)
    : runtime(-1), curtime(0)
  {
    lbnd = lowerbnd;
    ubnd = upperbnd;
    fn   = &generator::getNextUniform;
    init();
  }
  ~generator() {
    gsl_rng_free(r);
  }
  bool more() {
    if(runtime > 0)
      return (curtime + nextinterval < runtime);
    else
      return true;
  }
  int next() {
    long rtninterval = nextinterval;
    do {
      nextinterval = (this->*fn)();
    } while(nextinterval < 0);
    curtime += rtninterval;
    return rtninterval;
  }
  int peek() {
    return nextinterval;
  }
  long seed() {
    return _seed;
  }
  void seed(long seedValue) {
    init(seedValue);
  }
protected:
  long getNextGauss(void) {
    double x = gsl_ran_gaussian(r, stddev/3);
    return (int)floor(x + mean + 0.5);
  }
  long getNextGamma(void) {
    return (int)floor(gsl_ran_gamma(r, 1.0, 2.0) * mean);
  }
  long getNextUniform(void) {
    return (int)floor(gsl_ran_flat(r, lbnd, ubnd+1));
  }
};

struct modifier {
  char *unit;
  int multiplier;
};

int
getopt_number(long *value, struct modifier *modifiers)
{
  int i;
  double v;
  char *s, *t;
  v = strtod(optarg, &s);
  if(s == optarg)
    return -1;
  while(isspace(*s))
    ++s;
  if(*s && !(*s == ',' || *s == ':')) {
    if(!modifiers)
      return -2;
    for(i=0; modifiers[i].unit; i++)
      if(!strncasecmp(s, modifiers[i].unit, strlen(modifiers[i].unit))) {
        t = &s[strlen(modifiers[i].unit)];
        while(isspace(*t))
          ++t;
        if(*t == '\0' || (*t == ',' || *t == ':')) {
          v *= modifiers[i].multiplier;
          s = t;
          break;
        }
      }
    if(modifiers[i].unit == NULL)
      return -3;
  }
  if(*s == ',' || *s == ':')
    ++s;
  optarg = s;
  *value = (long)(v + 0.5);
  return 0;
}

int
getopt_number(int *value, struct modifier *modifiers)
{
  long v;
  int rtncode;
  if(!(rtncode = getopt_number(&v, modifiers))) {
    if(v > INT_MAX)
      return -4;
    *value = v;
  }
  return rtncode;
}

int
main(int argc, char *argv[])
{
  int verbosity = 0;
  long runtime = 7*86400;
  long nservers = 4;
  long seedValue;
  bool seedExplicitly = false;
  const int docid = 0;
  int A = 5*60;
  int B = 1*86400;
  int C = 5*60;
  int D = 1*1048576;
  int E = 100*1048576;
  int R = 0;

  FILE *fp;
  int ch;
#ifdef HAVE_GETOPT_LONG
  static struct option options[] = {
    { "verbose",      no_argument,       NULL, 'v' },
    { "seed",         optional_argument, NULL, 's' },
    { "nservers",     required_argument, NULL, 'n' },
    { "runtime",      required_argument, NULL, 'r' },
    { "updating",     required_argument, NULL, 'U' },
    { "retrieval",    required_argument, NULL, 'R' },
    { "documentsize", required_argument, NULL, 'D' },
    { NULL, 0, 0, 0 }
  };
#endif
  struct modifier sizemodifiers[] = {
    { "b",                1 },
    { "byte",             1 },
    { "k",             1024 },
    { "kb",            1024 },
    { "kbyte",         1024 },
    { "kilobyte",      1024 },
    { "m",          1048576 },
    { "mb",         1048576 },
    { "mbyte",      1048576 },
    { "megabyte",   1048576 },
    { NULL, 0 }
  };
  struct modifier timemodifiers[] = {
    { "s",          1 },
    { "sec",        1 },
    { "secs",       1 },
    { "second",     1 },
    { "seconds",    1 },
    { "seconde",    1 },
    { "seconden",   1 },
    { "m",         60 },
    { "min",       60 },
    { "mins",      60 },
    { "minut",     60 },
    { "minuut",    60 },
    { "minute",    60 },
    { "minute",    60 },
    { "minutes",   60 },
    { "minuten",   60 },
    { "minutes",   60 },
    { "h",       3600 },
    { "hr",      3600 },
    { "hrs",     3600 },
    { "hour",    3600 },
    { "hours",   3600 },
    { "u",       3600 },
    { "uur",     3600 },
    { "uren",    3600 },
    { "d",      86400 },
    { "dy",     86400 },
    { "dys",    86400 },
    { "day",    86400 },
    { "days",   86400 },
    { "dag",    86400 },
    { "dagen",  86400 },
    { NULL, 0 }
  };

  /* Get the name of the program without path */
  if((argv0 = strrchr(argv[0],'/')) == NULL)
    argv0 = argv[0];
  else
    ++argv0;

  opterr = 0;
#ifdef HAVE_GETOPT_LONG
  while((ch = getopt_long(argc, argv, "vs::n:r:U:R:D:", options, NULL)) >= 0) {
#else
  while((ch = getopt(argc, argv, "vs::n:r:U:R:D:")) >= 0) {
#endif
    switch(ch) {
    case 'v':
      ++verbosity;
      break;
    case 's':
      if(optarg && *optarg) {
        if(getopt_number(&seedValue, NULL)) {
          fprintf(stderr,"%s: illegal seed value\n",argv0);
          exit(1);
        }
      } else
        seedValue = time(NULL);
      seedExplicitly = true;
      break;
    case 'n':
      if(getopt_number(&nservers, NULL)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      break;
    case 'r':
      if(getopt_number(&runtime, timemodifiers)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      break;
    case 'D':
      if(getopt_number(&D, sizemodifiers)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      if(getopt_number(&E, sizemodifiers)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      break;
    case 'U':
      if(getopt_number(&A, timemodifiers)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      if(getopt_number(&B, timemodifiers)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      break;
    case 'R':
      if(getopt_number(&C, timemodifiers)) {
        fprintf(stderr,"%s: required argument for parameter not a valid number\n",argv0);
        exit(1);
      }
      if(getopt_number(&R, timemodifiers))
        R = 0;
      break;
    case '?':
      fprintf(stderr,"%s: invalid option %c\n",argv0,optopt);
      exit(1);
    }
  }
  if(optind < argc) {
    fprintf(stderr,"%s: too many arguments for command\n",argv0);
    exit(1);
  }

  if(!(fp = popen("sort -k 2n","w"))) {
    fprintf(stderr,"%s: cannot open pipe to output command.",argv0);
    exit(1);
  }


  { // Document Update Generator
    int count = 0;
    int delta, time = 0;
    generator interval(runtime, A, B);
    generator docsize(D, E);
    histogram hist(0, 86400+86400/2, 36);
    if(seedExplicitly) {
      interval.seed(seedValue);
      docsize.seed(seedValue);
    }
    fprintf(fp, "U\tt:%d\tdocsize:%d\tdocid:%d\n", time, docsize.next(), docid+1);
    while(interval.more()) {
      ++count;
      delta = interval.next();
      time += delta;
      hist.add(delta);
      fprintf(fp, "U\tt:%d\tdocsize:%d\tdocid:%d\n", time, docsize.next(), docid+1);
    }
    if(verbosity > 1) {
      cerr << "# Histogram of document update interval" << endl;
      cerr << hist;
    }
    if(verbosity > 0) {
      clog << "# update interval seed    = " << interval.seed() << endl;
      clog << "# update docsize  seed    = " << docsize.seed()  << endl;
    }
  }

  { // Document Retrieval Generator
    int count = 0;
    int delta, time = 0;
    int i, serverid;
    generator interval(runtime, C);
    generator accesspoint(0, nservers-1);
    histogram hist(0, 1*3600, 36);
    if(seedExplicitly) {
      interval.seed(seedValue);
      accesspoint.seed(seedValue);
    }
    while(interval.more()) {
      ++count;
      do {
        delta = interval.next();
      } while(time + delta == 0);
      time += delta;
      hist.add(delta);
      serverid = accesspoint.next();
      fprintf(fp, "R\tt:%d\tserver:%d\tdocid:%d\n", time, serverid+1, docid+1);
      for(i=1; i<R; i++)
        fprintf(fp, "R\tt:%d\tserver:%d\tdocid:%d\n", time+i*60, serverid+1, docid+1);
    }
    if(verbosity > 1) {
      cerr << "# Histogram of document retrieval interval" << endl;
      cerr << hist;
    }
    if(verbosity > 0) {
      clog << "# retrieval interval seed = " << interval.seed()    << endl;
      clog << "# retrieval accesspt seed = " << accesspoint.seed() << endl;
    }
  }

  pclose(fp);
  exit(0);
}
