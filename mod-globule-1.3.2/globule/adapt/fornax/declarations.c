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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "proto.h"

/****************************************************************************/

/* The next two functions SUCK! */

struct itm *
gettype(char *entityName, char *signalName, struct exp *expr)
{
  struct itm *item;
  assert(!expr->children || !expr->contents);
  if(expr->children) {
    item = gettype(entityName, signalName, expr->children);
    if(item)
      return item;
  } else {
    if(expr->type == eIDENT) {
      item = decl_getitem(entityName,signalName,expr->contents);
      if(item)
        return item;
    }
  }
  if(expr->next)
    return gettype(entityName, signalName, expr->next);
  return NULL;
}

int
gettargettype(char *entityName, char *signalName, struct exp *targetExpr,
              char *queueName,
              struct itm **targetEntity, struct itm **targetQueue)
{
  struct itm *item;
  item = gettype(entityName, signalName, targetExpr);
  if(item && item->type == iDECL && (item->rttype.type&0xf) == tENTITY)
    item = decl_getitem(NULL, NULL, item->rttype.name);
  if(item && item->type == iENTITY) {
    if(targetEntity)
      *targetEntity = item;
    item = decl_getitem(item->name, NULL, queueName);
    if(item && (item->type == iASYNCSIGNAL ||
                item->type == iSYNCSIGNAL  ||
                item->type == iTHRSIGNAL)) {
      if(targetQueue)
        *targetQueue = item;
      return 1;
    }
  }
  return 0;
}

/****************************************************************************/

struct decl {
  enum decltype { dENTITY, dSIGNAL, dARGUMENT, dGLOBAL, dLOCAL } type;
  struct itm *source;
  char *name;
  struct decl *prev;
  struct decl *next;
};
struct decl *firstdecl = NULL;
struct decl *lastdecl  = NULL;

static void
newdecl(char *name, enum decltype type, struct itm *source)
{
  struct decl *newdecl;
  CHECK(!(newdecl = malloc(sizeof(struct decl))));
  newdecl->type   = type;
  newdecl->name   = strdup(name);
  newdecl->source = source;
  newdecl->prev   = NULL;
  newdecl->next   = NULL;
  if(firstdecl) {
    lastdecl->next = newdecl;
    newdecl->prev = lastdecl;
    lastdecl = newdecl;
  } else
    firstdecl = lastdecl = newdecl;
}

void
decl_dump()
{
  struct decl *run;
  char *type = "INVALID";
  for(run=firstdecl; run; run=run->next) {
    switch(run->type) {
    case dENTITY:    type = "entity  ";  break;
    case dSIGNAL:    type = "signal  ";  break;
    case dARGUMENT:  type = "argument";  break;
    case dGLOBAL:    type = "global  ";  break;
    case dLOCAL:     type = "local   ";  break;
    }
    fprintf(stderr, "%s %s\t%p\n",type,run->name,run->source);
  }
}

void
decl_collect(struct lst *list)
{
  char s[1024];
  struct itm *entity;
  struct itm *item;
  struct itm *decl;
  char *signalName;
  for(entity=list->first; entity; entity=entity->next) {
    if(entity->type == iENTITY) {
      snprintf(s, sizeof(s), "%s", entity->name);
      newdecl(s, dENTITY, entity);
      for(item=entity->decls.first; item; item=item->next) {
        switch(item->type) {
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
        case iMAINSIGNAL:
          if(item->type == iMAINSIGNAL)
            signalName = "main";
          else
            signalName = item->name;
          snprintf(s, sizeof(s), "%s.%s", entity->name, signalName);
          newdecl(s, dSIGNAL, item);
          for(decl=item->args.first; decl; decl=decl->next) {
            snprintf(s, sizeof(s), "%s.%s.%s", entity->name, signalName,
                     decl->name);
            newdecl(s, dARGUMENT, decl);
          }
          for(decl=item->decls.first; decl; decl=decl->next) {
            snprintf(s, sizeof(s), "%s.%s.%s", entity->name, signalName,
                     decl->name);
            newdecl(s, dLOCAL, decl);
          }
          break;
        case iDECL:
          snprintf(s, sizeof(s), "%s.%s", entity->name, item->name);
          newdecl(s, dGLOBAL, item);
          break;
        default:
          ;
        }
      }
    }
  }
}

static struct decl *
lookup(char *entityName, char *signalName, char *varName)
{
  char s[1024];
  struct decl *run;
  if(signalName && *signalName) {
    snprintf(s, sizeof(s), "%s.%s.%s", entityName, signalName, varName);
    for(run=firstdecl; run; run=run->next)
      if(!strcmp(s, run->name))
        return run;
  } else if(entityName && *entityName) {
    snprintf(s, sizeof(s), "%s.main.%s", entityName, varName);
    for(run=firstdecl; run; run=run->next)
      if(!strcmp(s, run->name))
        return run;
  }
  if(entityName && *entityName) {
    snprintf(s, sizeof(s), "%s.%s", entityName, varName);
    for(run=firstdecl; run; run=run->next)
      if(!strcmp(s, run->name))
        return run;
  }
  snprintf(s, sizeof(s), "%s", varName);
  for(run=firstdecl; run; run=run->next)
    if(!strcmp(s, run->name))
      return run;
  return NULL;
}

struct itm *
decl_getitem(char *entityName, char *signalName, char *varName)
{
  struct decl *decl;
  if(!strcmp(varName,"TIME"))
    return NULL;
  decl = lookup(entityName, signalName, varName);
  if(decl)
    return decl->source;
  else
    return NULL;
}

char *
decl_getident(char *entityName, char *signalName, char *varName)
{
  static char store[1024];
  struct decl *decl;
  if(!strcmp(varName,"TIME")) {
    return "time()";
  }
  decl = lookup(entityName, signalName, varName);
  if(decl) {
    switch(decl->type) {
    case dARGUMENT:
    case dLOCAL:
      if(signalName && *signalName)
        snprintf(store,sizeof(store),"(state->vars.%s_%s)",signalName,varName);
      else
        snprintf(store,sizeof(store),"(state->vars.main_%s)",varName);
      break;
    case dGLOBAL:
      snprintf(store,sizeof(store),"(*(state->vars.%s))",varName);
      break;
    default:
      assert(!"Illegal declaration at this point");
    }
  } else
    snprintf(store,sizeof(store),"%s",varName);
  store[sizeof(store)-1] = '\0';
  return store;
}

/****************************************************************************/
