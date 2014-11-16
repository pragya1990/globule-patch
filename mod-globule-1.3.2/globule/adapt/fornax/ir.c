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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include "proto.h"
#include "parser.h"

char *
strndup(char *source, int len)
{
  char *dest;
  CHECK(!(dest = (char *)malloc(len + 1)));
  strncpy(dest, source, len);
  dest[len] = '\0';
  return dest;
}

struct exp *
expdup(struct exp *org)
{
  struct exp *cpy;
  if(org == NULL || (org->children == NULL &&
                     org->contents == NULL &&
                     org->next == NULL))
    return NULL;
  CHECK(!(cpy = malloc(sizeof(struct exp))));
  memcpy(cpy, org, sizeof(struct exp));
  return cpy;
}

struct itm *
itmdup(struct itm *org)
{
  struct itm *cpy;
  CHECK(!(cpy = malloc(sizeof(struct itm))));
  memcpy(cpy, org, sizeof(struct itm));
  return cpy;
}

struct blk *
blkdup(struct blk *org)
{
  struct blk *cpy;
  CHECK(!(cpy = malloc(sizeof(struct blk))));
  memcpy(cpy, org, sizeof(struct blk));
  return cpy;
}

/****************************************************************************/

struct typ
mktype1(enum itmtype specifier)
{
  struct typ result;
  result.type   = specifier;
  result.name   = NULL;
  result.length = NULL;
  return result;
}

struct typ
mktype2(struct typ t1, struct typ t2)
{
  struct typ result;
  assert(!(t1.name && t2.name));
  assert(!(t1.length && t2.length));
  result.name   = (t1.name   ? t1.name   : t2.name);
  result.length = (t1.length ? t1.length : t2.length);
  if(result.name)
    result.name = strdup(result.name);
  if(result.length)
    result.length = expdup(result.length);

  if((t1.type & 0xf) && (t2.type & 0xf))
    if(t1.type == tINTEGER)
      result.type = t2.type;
    else if(t2.type == tINTEGER)
      result.type = t1.type;
    else
      assert(!((t1.type & 0xf) && (t2.type & 0xf)));
  else if(t1.type & 0xf)
    result.type = t1.type;
  else if(t2.type & 0xf)
    result.type = t2.type;
  else
    result.type = 0;
  result.type |= (t1.type & 0xf0);
  result.type |= (t2.type & 0xf0);
  return result;
}

struct typ
mktype3(struct typ t1, struct typ t2, struct typ t3)
{
  return mktype2(mktype2(t1,t2),t3);
}

struct typ
mktypei(enum itmtype specifier, struct nam name)
{
  struct typ result;
  result.type   = specifier;
  result.name   = strdup(name.name);
  result.length = NULL;
  return result;
}

struct typ
mktypex1(struct typ t1)
{
  struct typ result;
  result.type   = (t1.type | tARRAY);
  result.name   = (t1.name ? strdup(t1.name) : NULL);
  result.length = NULL;
  return result;
}

struct typ
mktypex2(struct typ t1, struct exp nod)
{
  struct typ result;
  result.type   = (t1.type | tARRAY);
  result.name   = (t1.name ? strdup(t1.name) : NULL);
  result.length = expdup(&nod);
  return result;
}

/****************************************************************************/

struct blk blktemplate = { bPLAIN, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

struct blk
mkblock0()
{
  struct blk result = blktemplate;
  return result;
}
struct blk
mkblock1(struct exp e)
{
  struct blk result = blktemplate;
  result.preloop = expdup(&e);
  return result;
}
struct blk
mkblock2(struct blk b1, struct blk b2)
{
  struct blk result = blktemplate;
  result.truebranch  = blkdup(&b1);
  result.falsebranch = blkdup(&b2);
  return result;
}
struct blk
mkblockif(struct exp e, struct blk b1, struct blk b2)
{
  struct blk result = blktemplate;
  result.type        = bIF;
  result.condition   = expdup(&e);
  result.truebranch  = blkdup(&b1);
  result.falsebranch = blkdup(&b2);
  return result;
}
struct blk
mkblockelif(struct exp e, struct blk b1, struct blk b2)
{
  struct blk result = blktemplate;
  result.type        = bELIF;
  result.condition   = expdup(&e);
  result.truebranch  = blkdup(&b1);
  result.falsebranch = blkdup(&b2);
  return result;
}
struct blk
mkblockelse(struct blk b)
{
  struct blk result = blktemplate;
  result.type        = bELSE;
  result.falsebranch = blkdup(&b);
  return result;
}
struct blk
mkblockfor(struct exp e1, struct exp e2, struct exp e3, struct blk b)
{
  struct blk result = blktemplate;
  result.type        = bFOR;
  result.preloop     = expdup(&e1);
  result.condition   = expdup(&e2);
  result.postloop    = expdup(&e3);
  result.truebranch  = blkdup(&b);
  return result;
}
struct blk
mkblockdowhile(struct blk b, struct exp e)
{
  struct blk result = blktemplate;
  result.type        = bDOWHILE;
  result.condition   = expdup(&e);
  result.truebranch  = blkdup(&b);
  return result;
}
struct blk
mkblockwhile(struct exp e, struct blk b)
{
  struct blk result = blktemplate;
  result.type        = bWHILE;
  result.condition   = expdup(&e);
  result.truebranch  = blkdup(&b);
  return result;
}
struct blk
mkblockwait(struct exp e)
{
  struct blk result = blktemplate;
  result.type = bCOMM;
  result.comm.type = cWAIT;
  result.comm.params.time = expdup(&e);
  return result;
}
struct blk
mkblockreply(struct exp e)
{
  struct blk result = blktemplate;
  result.type = bCOMM;
  result.comm.type = cREPLY;
  result.comm.params.rtvalue = expdup(&e);
  return result;
}
struct blk
mkblockrecv(struct lst l)
{
  struct blk result = blktemplate;
  result.type = bCOMM;
  result.comm.type = cRECV;
  result.comm.params.queues = l;
  return result;
}
struct blk
mkblocksend(struct exp entity, struct nam name, struct exp args,
            struct exp lvalue)
{
  struct blk result = blktemplate;
  result.type = bCOMM;
  result.comm.type = cSEND;
  result.comm.params.send.queue  = strdup(name.name);
  result.comm.params.send.entity = expdup(&entity);
  result.comm.params.send.args   = expdup(&args);
  result.comm.params.send.lvalue = expdup(&lvalue);
  return result;
}

/****************************************************************************/

struct lst
mklist0()
{
  struct lst lst;
  lst.first = lst.last = NULL;
  return lst;
}
struct lst
mklist1(struct itm i)
{
  struct lst result;
  result.first = result.last = itmdup(&i);
  return result;
}
struct lst
mklist2(struct itm i, struct lst l)
{
  struct lst result;
  if(l.first == NULL)
    return mklist1(i);
  result.first = itmdup(&i);
  result.last  = l.last;
  result.first->next = l.first;
  return result;
}
struct lst
mklist3(struct lst l1, struct lst l2, struct itm i)
{
  struct lst result;
  if(l1.first == NULL && l2.first == NULL)
    return mklist1(i);
  if(l1.first == NULL) {
    result.first  = l2.first;
    result.last   = itmdup(&i);
    l2.last->next = result.last;
  } else if(l2.first == NULL) {
    result.first  = l1.first;
    result.last   = itmdup(&i);
    l1.last->next = result.last;
  } else {
    result.first  = l1.first;
    result.last   = itmdup(&i);
    l1.last->next = l2.first;
    l2.last->next = result.last;
  }
  return result;
}
struct lst
mklistjoin(struct lst l1, struct lst l2)
{
  struct lst result;
  if(l1.first == NULL)
    result = l2;
  else if(l2.first == NULL)
    result = l1;
  else {
    result.first  = l1.first;
    result.last   = l2.last;
    l1.last->next = l2.first;
  }
  return result;
}

struct nam
mkname0()
{
  struct nam result;
  result.name = strdup("");
  return result;
}
struct nam
mkname1(struct tok t)
{
  struct nam result;
  result.name = strdup(t.contents);
  return result;
}
struct nam
mkname2(struct nam n, struct tok t)
{
  struct nam result;
  int length;
  length = strlen(n.name) + 2 + strlen(t.contents) + 1;
  CHECK(!(result.name = malloc(length)));
  snprintf(result.name, length, "%s::%s", n.name, t.contents);
  result.name[length-1] = '\0';
  return result;
}

/****************************************************************************/

static struct exp
mkexpr(int nexprs, ...)
{
  va_list ap;
  int i;
  struct exp result;
  struct exp e;
  struct exp *cpy, **lastptr;
  result.type     = ePLAIN;
  result.next     = NULL;
  result.children = NULL;
  result.contents = NULL;
  lastptr = &result.children;
  va_start(ap, nexprs);
  for(i=0; i<nexprs; i++) {
    e = va_arg(ap, struct exp);
    cpy = expdup(&e);
    if(cpy) {
      *lastptr = cpy;
      lastptr = &(*lastptr)->next;
    }
  }
  va_end(ap);
  return result;
}

struct exp
mkexprl(struct lst l)
{
  struct exp result;
  struct itm *run;
  struct exp seperator;
  struct exp **lastptr;
  result.type     = ePLAIN;
  result.next     = NULL;
  result.children = NULL;
  result.contents = NULL;
  lastptr = &result.children;
  for(run=l.first; run; run=run->next) {
    assert(run->type == iEXPR);
    *lastptr = run->code->preloop;
    lastptr = &(*lastptr)->next;
    if(run->next) {
      seperator = mkexpr0();
      seperator.contents = strdup(",");
      *lastptr = expdup(&seperator);
      lastptr = &(*lastptr)->next;
    }
  }
  return result;
}

struct exp
mkident(struct nam n)
{
  struct exp result;
  result.type          = eIDENT;
  result.next          = NULL;
  result.children      = NULL;
  result.contents      = strdup(n.name);
  result.leadin        = NULL;
  result.location.file = NULL;
  result.location.line = -1;
  return result;
}
struct exp
mkstring(struct tok t)
{
  struct exp result;
  result.type          = eSTRING;
  result.next          = NULL;
  result.children      = NULL;
  result.contents      = strdup(t.contents);
  result.leadin        = (t.leadin ? strdup(t.leadin) : NULL);
  result.location.file = strdup(t.location.file);
  result.location.line = t.location.line;
  return result;
}
struct exp
mkliteral(struct tok t)
{
  struct exp result;
  result.type          = eLITERAL;
  result.next          = NULL;
  result.children      = NULL;
  result.contents      = strdup(t.contents);
  result.leadin        = (t.leadin ? strdup(t.leadin) : NULL);
  result.location.file = strdup(t.location.file);
  result.location.line = t.location.line;
  return result;
}
struct exp
mkexpr0()
{
  struct exp result;
  result.type          = ePLAIN;
  result.next          = NULL;
  result.children      = NULL;
  result.contents      = NULL;
  result.leadin        = NULL;
  result.location.file = NULL;
  result.location.line = -1;
  return result;
}
struct exp
mkexprt(struct tok t)
{
  struct exp result;
  result.type          = ePLAIN;
  result.next          = NULL;
  result.children      = NULL;
  result.contents      = strdup(t.contents);
  result.leadin        = (t.leadin ? strdup(t.leadin) : NULL);
  result.location.file = strdup(t.location.file);
  result.location.line = t.location.line;
  return result;
}
struct exp
mkexprtet(struct tok t1, struct exp e, struct tok t2)
{
  return mkexpr(3, mkexprt(t1), e, mkexprt(t2));
}
struct exp
mkexprte(struct tok t, struct exp e)
{
  return mkexpr(2, mkexprt(t), e);
}
struct exp
mkexpretet(struct exp e1, struct tok t1, struct exp e2, struct tok t2)
{
  return mkexpr(4, e1, mkexprt(t1), e2, mkexprt(t2));
}
struct exp
mkexpretlt(struct exp e, struct tok t1, struct lst l, struct tok t2)
{
  return mkexpr(4, e, mkexprt(t1), mkexprl(l), mkexprt(t2));
}
struct exp
mkexpretete(struct exp e1, struct tok t1, struct exp e2, struct tok t2,
            struct exp e3)
{
  return mkexpr(5, e1, mkexprt(t1), e2, mkexprt(t2), e3);
}
struct exp
mkexprete(struct exp e1, struct tok t, struct exp e2)
{
  return mkexpr(3, e1, mkexprt(t), e2);
}
struct exp
mkexpret(struct exp e, struct tok t)
{
  return mkexpr(2, e, mkexprt(t));
}

/****************************************************************************/

struct itm
mkitem(enum itmtype specifier, ...)
{
  struct itm result;
  struct itm fullbody;
  char *name;
  struct exp expr;
  struct blk code;
  struct tok token;
  va_list ap;
  va_start(ap, specifier);
  result.type          = specifier;
  result.next          = NULL;
  result.location.file = NULL;
  result.location.line = -1;
  switch(specifier) {
  case iLITERAL:
    token                = va_arg(ap, struct tok);
    result.name          = strdup(token.contents);
    result.location.file = strdup(token.location.file);
    result.location.line = token.location.line;
    break;
  case iENTITY:
    token                = va_arg(ap, struct tok);
    result.location.file = strdup(token.location.file);
    result.location.line = token.location.line;
    result.name          = strdup(va_arg(ap, char *));
    result.decls         = va_arg(ap, struct lst);
    result.labels        = NULL;
    break;
  case iTHRSIGNAL:
  case iASYNCSIGNAL:
  case iSYNCSIGNAL:
  case iMAINSIGNAL:
    token                = va_arg(ap, struct tok);
    result.location.file = strdup(token.location.file);
    result.location.line = token.location.line;
    result.name          = strdup(va_arg(ap, char *));
    result.rttype        = va_arg(ap, struct typ);
    result.args          = va_arg(ap, struct lst);
    fullbody             = va_arg(ap, struct itm);
    assert(fullbody.type == iBODY);
    result.decls         = fullbody.decls;
    result.code          = fullbody.code;
    break;
  case iBODY:
    result.decls = va_arg(ap, struct lst);
    code = va_arg(ap, struct blk);
    result.code  = blkdup(&code);
    break;
  case iDECL:
    name = va_arg(ap, char *);
    result.name   = strdup(name);
    result.rttype = va_arg(ap, struct typ);
    break;
  case iNAME:
    name = va_arg(ap, char *);
    result.name   = strdup(name);
    break;
  case iEXPR:
    expr = va_arg(ap, struct exp);
    code = mkblock1(expr);
    result.code = blkdup(&code);
    break;
  }
  va_end(ap);
  return result;
}

/****************************************************************************/
