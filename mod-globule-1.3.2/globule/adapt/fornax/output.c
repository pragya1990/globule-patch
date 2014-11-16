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
#include <ctype.h>
#include "proto.h"

char *
indentspacing(int indentlevel)
{
  static char *maxindentspacing = "                                        ";
  int pos;
  if(indentlevel <= 0)
    return "";
  if((pos = strlen(maxindentspacing)-indentlevel*2) < 0)
    pos = 0;
  return &maxindentspacing[pos];
}

void
output_typ(FILE *outfp, struct typ *type)
{
  if((type->type & tUNSIGNED))
    fprintf(outfp,"unsigned ");
  switch(type->type & 0xf) {
  case tVOID:       fprintf(outfp,"void");                    break;
  case tBOOLEAN:    fprintf(outfp,"boolean");                 break;
  case tBYTE:       fprintf(outfp,"byte");                    break;
  case tCHARACTER:  fprintf(outfp,"character");               break;
  case tSHORT:      fprintf(outfp,"short");                   break;
  case tINTEGER:    fprintf(outfp,"integer");                 break;
  case tLONG:       fprintf(outfp,"long");                    break;
  case tLONGLONG:   fprintf(outfp,"long long");               break;
  case tFLOAT:      fprintf(outfp,"float");                   break;
  case tDOUBLE:     fprintf(outfp,"double");                  break;
  case tSTRING:     fprintf(outfp,"string");                  break;
  case tTYPENAME:   fprintf(outfp,"typename %s",type->name);  break;
  case tENTITY:     fprintf(outfp,"entity %s",type->name);    break;
  }
  if((type->type & tARRAY)) {
    fprintf(outfp,"[");
    if(type->length)
      output_exp(outfp, type->length);
    fprintf(outfp,"]");
  }
}

void
output_exp(FILE *outfp, struct exp *expr)
{
  char *s;
  if(!expr)
    return;
  assert(!expr->children || !expr->contents);
  if(expr->children) {
    output_exp(outfp, expr->children);
  } else {
    switch(expr->type) {
    case ePLAIN:
      if(expr->contents)
        fprintf(outfp, "%s", expr->contents);
      break;
    case eIDENT:
      fprintf(outfp, "%s", expr->contents);
      break;
    case eSTRING:
      fputc('"',outfp);
      for(s=expr->contents; *s; s++) {
        if(isprint(*s))
          fputc(*s,outfp);
        else
          switch(*s) {
          case '\n':  fprintf(outfp,"\\n");  break;
          case '\t':  fprintf(outfp,"\\t");  break;
          case '\r':  fprintf(outfp,"\\r");  break;
          case '\b':  fprintf(outfp,"\\b");  break;
          case '\f':  fprintf(outfp,"\\f");  break;
          default:    fprintf(outfp,"\\%3.3o",*s);
          }
      }
      fputc('"',outfp);
      break;
    case eLITERAL:
      fprintf(outfp, "`%s`", expr->contents);
      break;
    }
  }
  if(expr->next) {
    fprintf(outfp," ");
    output_exp(outfp, expr->next);
  }
}

void
output_blk(FILE *outfp, struct blk *block, int indent)
{
  struct itm *run;
  switch(block->type) {
  case bPLAIN:
    if(block->preloop) {
      fprintf(outfp,"%s",indentspacing(indent));
      output_exp(outfp, block->preloop);
      fprintf(outfp,";\n");
    }
    if(block->truebranch)
      output_blk(outfp, block->truebranch, indent);
    if(block->falsebranch)
      output_blk(outfp, block->falsebranch, indent);
    break;
  case bIF:
    fprintf(outfp,"%sif(",indentspacing(indent));
    output_exp(outfp, block->condition);
    fprintf(outfp,") {\n");
    output_blk(outfp, block->truebranch,indent+1);
    if(block->falsebranch) {
      if(block->falsebranch->type == bELIF ||
         block->falsebranch->type == bELSE) {
        fprintf(outfp,"%s} ",indentspacing(indent));
        output_blk(outfp, block->falsebranch, indent+1);
      } else {
        fprintf(outfp,"%s} else {\n",indentspacing(indent));
        output_blk(outfp, block->falsebranch, indent+1);
        fprintf(outfp,"%s}\n",indentspacing(indent));
      }
    } else
      fprintf(outfp,"%s}\n",indentspacing(indent));
    break;
  case bELIF:
    fprintf(outfp,"else if(");
    output_exp(outfp, block->condition);
    fprintf(outfp,") {\n");
    output_blk(outfp, block->truebranch,indent+1);
    if(block->falsebranch) {
      if(block->falsebranch->type == bELIF ||
         block->falsebranch->type == bELSE) {
        fprintf(outfp,"%s} ",indentspacing(indent));
        output_blk(outfp, block->falsebranch, indent+1);
      } else {
        fprintf(outfp,"%s} else {",indentspacing(indent));
        output_blk(outfp, block->falsebranch, indent+1);
        fprintf(outfp,"%s}\n",indentspacing(indent));
      }
    } else
      fprintf(outfp,"%s}\n",indentspacing(indent));
    break;
  case bELSE:
    fprintf(outfp,"else {\n");
    output_blk(outfp, block->falsebranch, indent+1);
    fprintf(outfp,"%s}\n",indentspacing(indent));
    break;
  case bFOR:
    fprintf(outfp,"%sfor(",indentspacing(indent));
    output_exp(outfp, block->preloop);
    fprintf(outfp,"; ");
    output_exp(outfp, block->condition);
    fprintf(outfp,"; ");
    output_exp(outfp, block->postloop);
    fprintf(outfp,") {\n");
    output_blk(outfp, block->truebranch, indent+1);
    fprintf(outfp,"%s}\n",indentspacing(indent));
    break;
  case bDOWHILE:
    fprintf(outfp,"%sdo {",indentspacing(indent));
    output_blk(outfp, block->truebranch, indent+1);
    fprintf(outfp,"%s} while(\n",indentspacing(indent));
    output_exp(outfp, block->condition);
    fprintf(outfp,");\n");
    break;
  case bWHILE:
    fprintf(outfp,"%swhile(",indentspacing(indent));
    output_exp(outfp, block->condition);
    fprintf(outfp,") {\n");
    output_blk(outfp, block->truebranch, indent+1);
    fprintf(outfp,"%s}\n",indentspacing(indent));
    break;
  case bCOMM:
    switch(block->comm.type) {
    case cWAIT:
      fprintf(outfp,"%sWAIT ",indentspacing(indent));
      output_exp(outfp, block->comm.params.time);
      fprintf(outfp,";\n");
      break;
    case cREPLY:
      fprintf(outfp,"%sREPLY ",indentspacing(indent));
      output_exp(outfp, block->comm.params.rtvalue);
      fprintf(outfp,";\n");
      break;
    case cRECV:
      fprintf(outfp,"%sREPLY ",indentspacing(indent));
      for(run=block->comm.params.queues.first; run; run=run->next) {
        output_itm(outfp, run, -1);
        if(run->next)
          fprintf(outfp,", ");
      }
      fprintf(outfp,";\n");
      break;
    case cSEND:
      fprintf(outfp,"%sSEND ",indentspacing(indent));
      output_exp(outfp, block->comm.params.send.entity);
      fprintf(outfp," -> %s (",block->comm.params.send.queue);
      output_exp(outfp, block->comm.params.send.args);
      fprintf(outfp,");\n");
      break;
    default:
      assert(!"Unknown comm block type");
    }
    if(block->label)
      fprintf(outfp,"%sJUMP %d; // implicitly\n", indentspacing(indent),
              block->label->id);
    break;
  case bLABEL:
    fprintf(outfp,"%sLABEL %d:\n",indentspacing(indent-1),block->label->id);
    break;
  case bJUMP:
    fprintf(outfp,"%sJUMP %d;\n",indentspacing(indent),block->label->id);
    break;
  default:
    assert(!"Unknown block type");
  }
  if(block->next)
    output_blk(outfp, block->next, indent);
}

void
output_itm(FILE *outfp, struct itm *item, int indent)
{
  struct itm *subitem;
  switch(item->type) {

  case iLITERAL:
    fprintf(outfp, "%s%s", indentspacing(indent), item->name);
    break;

  case iENTITY:
    fprintf(outfp,"%sentity %s {\n",indentspacing(indent),item->name);
    for(subitem=item->decls.first; subitem; subitem=subitem->next)
      switch(subitem->type) {
      case iDECL:
        output_itm(outfp, subitem, indent+1);
        break;
      default:
        ;
      }
    for(subitem=item->decls.first; subitem; subitem=subitem->next)
      switch(subitem->type) {
      case iASYNCSIGNAL:
      case iSYNCSIGNAL:
      case iTHRSIGNAL:
        output_itm(outfp, subitem, indent+1);
        break;
      default:
        ;
      }
    for(subitem=item->decls.first; subitem; subitem=subitem->next)
      switch(subitem->type) {
      case iMAINSIGNAL:
        output_itm(outfp, subitem, indent+1);
        break;
      default:
        ;
      }
    for(subitem=item->decls.first; subitem; subitem=subitem->next)
      switch(subitem->type) {
      case iDECL:
      case iASYNCSIGNAL:
      case iSYNCSIGNAL:
      case iTHRSIGNAL:
      case iMAINSIGNAL:
        break;
      default:
        assert(!"Bad item in list");
      }
    fprintf(outfp,"%s}\n",indentspacing(indent));
    break;

  case iASYNCSIGNAL:
  case iSYNCSIGNAL:
  case iTHRSIGNAL:
  case iMAINSIGNAL:
    fprintf(outfp,"%s",indentspacing(indent));
    switch(item->type) {
    case iASYNCSIGNAL:
      fprintf(outfp,"signal %s",item->name);
      break;
    case iSYNCSIGNAL:
      fprintf(outfp,"signal ");
      output_typ(outfp, &item->rttype);
      fprintf(outfp," %s",item->name);
      break;
    case iTHRSIGNAL:
      fprintf(outfp,"threaded ");
      output_typ(outfp, &item->rttype);
      fprintf(outfp," %s",item->name);
      break;
    case iMAINSIGNAL:
      fprintf(outfp,"main");
      break;
    default:
      assert(!"Bad signal");
    }
    fprintf(outfp,"(");
    for(subitem=item->args.first; subitem; subitem=subitem->next) {
      switch(subitem->type) {
      case iDECL:
        output_itm(outfp, subitem,-1);
        break;
      default:
        assert(!"Bad declaration");
      }
      if(subitem->next)
        fprintf(outfp,", ");
    }
    fprintf(outfp,") {\n");
    for(subitem=item->decls.first; subitem; subitem=subitem->next)
      switch(subitem->type) {
      case iDECL:
        output_itm(outfp, subitem,indent+1);
        break;
      default:
        assert(!"Bad declaration");
      }
    output_blk(outfp, item->code,indent+1);
    fprintf(outfp,"%s}\n",indentspacing(indent));
    break;

  case iDECL:
    if(indent >= 0)
      fprintf(outfp,"%s",indentspacing(indent));
    output_typ(outfp, &item->rttype);
    fprintf(outfp," %s",item->name);
    if(indent >= 0)
      fprintf(outfp,";\n");
    break;

  case iNAME:
    if(indent >= 0)
      fprintf(outfp,"%s",indentspacing(indent));
    fprintf(outfp,"%s",item->name);
    if(indent >= 0)
      fprintf(outfp,";\n");
    break;

  case iBODY:
    assert(!"Illegal item");
    break;
  default:
    assert(!"Unknown item");
  }
}

void
output(FILE *outfp, struct lst *l)
{
  struct itm *item;
  for(item=l->first; item; item=item->next) {
    output_itm(outfp, item, 0);
    if(item->next)
      fprintf(outfp,"\n");
  }
}
