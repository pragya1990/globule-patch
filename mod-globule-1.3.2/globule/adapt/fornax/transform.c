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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "proto.h"

static int labelcount;
static struct blklabel *firstlabel, *lastlabel;
static struct itm *cursignal;

struct blk *
mkblklabel(struct blklabel **labelptr)
{
  struct blk *result;
  CHECK(!(result = malloc(sizeof(struct blk))));
  *result = blktemplate;
  if(!*labelptr) {
    CHECK(!(*labelptr = malloc(sizeof(struct blklabel))));
    (*labelptr)->prev = lastlabel;
    (*labelptr)->next = NULL;
    if(lastlabel) {
      lastlabel->next = (*labelptr);
      lastlabel = (*labelptr);
    } else
      firstlabel = lastlabel = (*labelptr);
  } else {
    assert((*labelptr)->id < 0);
    assert((*labelptr)->entrypoint == NULL);
  }
  (*labelptr)->id         = labelcount++;
  (*labelptr)->entrypoint = result;
  (*labelptr)->signal     = cursignal;
  result->type  = bLABEL;
  result->label = *labelptr;
  return result;
}

struct blk *
mkblkjump(struct blklabel **labelptr)
{
  struct blk *result;
  CHECK(!(result = malloc(sizeof(struct blk))));
  *result = blktemplate;
  if(!*labelptr) {
    CHECK(!(*labelptr = malloc(sizeof(struct blklabel))));
    (*labelptr)->id         = -1;
    (*labelptr)->entrypoint = NULL;
    (*labelptr)->prev       = lastlabel;
    (*labelptr)->next       = NULL;
    if(lastlabel) {
      lastlabel->next = (*labelptr);
      lastlabel = (*labelptr);
    } else
      firstlabel = lastlabel = (*labelptr);
  }
  result->type  = bJUMP;
  result->label = *labelptr;
  return result;
}

void
blkannotate(struct blk *block, struct blklabel **labelptr)
{
  if(!*labelptr) {
    CHECK(!(*labelptr = malloc(sizeof(struct blklabel))));
    (*labelptr)->id         = -1;
    (*labelptr)->entrypoint = NULL;
    (*labelptr)->prev       = lastlabel;
    (*labelptr)->next       = NULL;
    if(lastlabel) {
      lastlabel->next = (*labelptr);
      lastlabel = (*labelptr);
    } else
      firstlabel = lastlabel = (*labelptr);
  }
  block->label = *labelptr;
}

static struct blk *
mkblkif(struct exp *e, struct blk *b1, struct blk *b2)
{
  struct blk *result;
  CHECK(!(result = malloc(sizeof(struct blk))));
  *result = blktemplate;
  result->type        = bIF;
  result->condition   = e;
  result->truebranch  = b1;
  result->falsebranch = b2;
  return result;
}

static struct blk *
mkblkexpr(struct exp *expr)
{
  struct blk *result;
  CHECK(!(result = malloc(sizeof(struct blk))));
  *result = blktemplate;
  result->type        = bPLAIN;
  result->preloop     = expr;
  return result;
}

static struct blk *
mkblklist(struct blk *block,...)
{
  va_list ap;
  struct blk *next, *aux;
#ifndef NDEBUG
int argcount = 0;
#endif
  va_start(ap, block);
  while((next = va_arg(ap, struct blk *)) != NULL) {
    assert(0x100 < (unsigned long)next);
    CHECK(!(aux = malloc(sizeof(struct blk))));
    *aux = blktemplate;
    aux->type        = bPLAIN;
    aux->truebranch  = block;
    aux->falsebranch = next;
    block = aux;
#ifndef NDEBUG
    if(++argcount > 20)
      abort();
#endif
  }
  va_end(ap);
  return block;
}

struct blk *
mkblkrecvreply(struct blk *source)
{
  struct blk *result;
  CHECK(!(result = malloc(sizeof(struct blk))));
  *result = blktemplate;
  result->type      = bCOMM;
  result->comm.type = cRECVREPLY;
  assert(source->type == bCOMM && source->comm.type == cSEND);
  assert(source->next == NULL);
  result->comm.params.recvreply.lvalue = expdup(source->comm.params.send.lvalue);
  source->comm.params.send.lvalue      = NULL;
  return result;
}

static int
needsbreakup(struct blk *block)
{
  assert(block->next == NULL);
  switch(block->type) {
  case bCOMM:
  case bJUMP:
  case bLABEL:
    return 1;
  default:
    if(block->truebranch  && needsbreakup(block->truebranch))
      return 1;
    if(block->falsebranch && needsbreakup(block->falsebranch))
      return 1;
  }
  return 0;
}

static struct blk *
breakup(char *entityName, char *signalName, struct blk *block,
        struct blklabel **outwards)
{
  struct blklabel *label1 = NULL;
  struct blklabel *label2 = NULL;
  struct blk *b1;
  struct blk *b2;
  struct itm *targetEntity, *targetQueue;

  if(block == NULL)
    return NULL;
  assert(block->next == NULL);
  if(!needsbreakup(block)) {
    return mkblklist(block, mkblkjump(outwards), NULL);
    return block;
  } else {
    switch(block->type) {
    case bPLAIN:
      if(block->falsebranch && needsbreakup(block->falsebranch))
        b2 = breakup(entityName, signalName, block->falsebranch, outwards);
      else
        b2 = block->falsebranch;
      if(block->truebranch && needsbreakup(block->truebranch))
        b1 = breakup(entityName, signalName, block->truebranch, &label1);
      else
        b1 = block->truebranch;
      if(!label1) {
        return mkblklist(b1, b2, mkblkjump(outwards), NULL);
        block->truebranch  = b1;
        block->falsebranch = b2;
        return block;
      } else {
        return mkblklist(b1,mkblklabel(&label1),b2,mkblkjump(outwards),NULL);
      }
    case bIF:
      return mkblkif(block->condition, breakup(entityName, signalName, block->truebranch, outwards),
                                       breakup(entityName, signalName, block->falsebranch, outwards));
    case bELIF:
      return mkblkif(block->condition, breakup(entityName, signalName, block->truebranch, outwards),
                                       breakup(entityName, signalName, block->falsebranch, outwards));
    case bELSE:
      if(block->falsebranch && (block->falsebranch->type != bPLAIN ||
                                block->falsebranch->preloop))
        return breakup(entityName, signalName, block->falsebranch, outwards);
      else
        return NULL;
    case bFOR:
      if(block->condition == NULL)
        return mkblklist(mkblkexpr(block->preloop),
                         mkblkjump(&label1),
                         mkblklabel(&label1),
                         breakup(entityName, signalName, block->truebranch, &label2),
                         mkblklabel(&label2),
                         mkblkexpr(block->postloop),
                         mkblkjump(&label1),
                         NULL);
      else
        return mkblklist(mkblkexpr(block->preloop),
                         mkblkjump(&label1),
                         mkblklabel(&label1),
                         mkblkif(block->condition,
                                 mkblklist(breakup(entityName, signalName, block->truebranch, &label2),
                                           mkblklabel(&label2),
                                           mkblkexpr(block->postloop),
                                           mkblkjump(&label1),
                                           NULL),
                                 NULL),
                         NULL);
    case bDOWHILE:
      return mkblklist(mkblkjump(&label1),
                       mkblklabel(&label1),
                       breakup(entityName, signalName, block->truebranch, &label2),
                       mkblklabel(&label2),
                       mkblkif(block->condition, mkblkjump(&label1), NULL),
                       NULL);
    case bWHILE:
      return mkblklist(mkblkjump(&label1),
                       mkblklabel(&label1),
                       mkblkif(block->condition,
                               breakup(entityName, signalName, block->truebranch, &label1),
                               NULL),
                       NULL);
    case bCOMM:
      if(block->comm.type == cSEND) {
        if(gettargettype(entityName, signalName, block->comm.params.send.entity, block->comm.params.send.queue, &targetEntity, &targetQueue)) {
          if(targetQueue->type == iSYNCSIGNAL || targetQueue->type == iTHRSIGNAL) {
            b1 = mkblklist(block, mkblklabel(&label1), b2 = mkblkrecvreply(block), NULL);
            blkannotate(block, &label1);
            blkannotate(b2, outwards);
            return b1;
          }
        } else
          yyerror("cannot determine send target entity or queue type for context %s.%s looking for signal %s",entityName,signalName,block->comm.params.send);

      }
      blkannotate(block, outwards);
      return block;
    case bLABEL:
      assert(!"label statement not allowed");
      break;
    case bJUMP:
      assert(!"jump statement not allowed");
      break;
    default:
      assert(!"Unknown block type");
    }
    return block;
  }
}

static struct blk *
joinup(struct blk *block, struct blk **last)
{
  struct blk *b1, *b2, *aux;
  switch(block->type) {
  case bPLAIN:
    assert(!((block->truebranch || block->falsebranch) && block->preloop));
    if(!block->preloop) {
      if(!block->truebranch && block->falsebranch)
        return joinup(block->falsebranch, last);
      if(!block->falsebranch && block->truebranch)
        return joinup(block->truebranch, last);
      if(!block->falsebranch && !block->truebranch)
        return NULL;
      b1 = joinup(block->truebranch, &aux);
      b2 = joinup(block->falsebranch, last);
      if(b1 == NULL) {
        return b2;
      } else if(b2 == NULL) {
        if(last)
          *last = aux;
        return b1;
      } else {
        if(aux->type != bCOMM && aux->type != bJUMP)
          aux->next = b2;
        return b1;
      }
    }
  case bFOR:
  case bDOWHILE:
  case bWHILE:
    assert(!block->falsebranch);
    if(block->truebranch)
      block->truebranch = joinup(block->truebranch, NULL);
    break;
  case bIF:
  case bELIF:
    if(block->truebranch)
      block->truebranch  = joinup(block->truebranch, NULL);
    if(block->falsebranch)
      block->falsebranch = joinup(block->falsebranch, NULL);
    break;
  case bELSE:
    return joinup(block->falsebranch, last);
  case bCOMM:
  case bJUMP:
  case bLABEL:
    break;
  }
  if(last)
    *last = block;
  return block;
}

struct blk *
transform_blk(char *entityName, char *signalName, struct blk *block)
{
  struct blklabel *start = NULL;
  struct blk *entrypoint;
  entrypoint = mkblklabel(&start);
  assert(firstlabel);
  return joinup(mkblklist(entrypoint,
                          breakup(entityName, signalName, block, &firstlabel),
                          NULL),
                NULL);
}

void
transform(struct lst *list)
{
  struct itm *entity;
  struct itm *signal;
  for(entity=list->first; entity; entity=entity->next) {
    if(entity->type == iENTITY) {
      labelcount = 0;
      firstlabel = lastlabel = NULL;
      for(signal=entity->decls.first; signal; signal=signal->next) {
        if(signal->type == iMAINSIGNAL) {
          cursignal = signal;
          signal->code = transform_blk(entity->name,signal->name,signal->code);
        }
      }
      for(signal=entity->decls.first; signal; signal=signal->next) {
        switch(signal->type) {
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
          cursignal = signal;
          signal->code = transform_blk(entity->name,signal->name,signal->code);
          break;
        default:
          ;
        }
      }
#ifdef NOTDEFINED
      { struct blklabel *run;
        for(run=firstlabel; run; run=run->next) {
          fprintf(stdout,"#");
          output_blk(stdout, run->entrypoint, 1);
        }
      }
#endif
      entity->labels = firstlabel;
      labelcount = 0;
      firstlabel = lastlabel = NULL;
    }
  }
}
