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
#include <ctype.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#endif
#include "proto.h"

/****************************************************************************/

#if !defined(__GNUC__) || __GNUC__ < 2 || \
    (__GNUC__ == 2 && __GNUC_MINOR__ < 7) ||\
    defined(NEXT)
#ifndef __attribute__
#define __attribute__(__x)
#endif
#endif

void generate_out(FILE *outfp, const char *fmt, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));

static char buffer[10485760]; // no check against buffer is made
static int lineno = 0;
static int atnewline = 1;
 
void
generate_out(FILE *outfp, const char *fmt, ...)
{
  va_list ap;
  char *s;
  va_start(ap, fmt);
  vsprintf(buffer, fmt, ap);
  for(s=buffer; *s; s++)
    if(*s == '\n')
      ++lineno;
  if(s != buffer) {
    if(*(--s) == '\n')
      atnewline = 1;
    else
      atnewline = 0;
  }
  fputs(buffer, outfp);
  va_end(ap);
}

void
generate_location(FILE *outfp, struct location *location)
{
  if(option_linedirectives && location && location->file && location->line != lineno) {
    if(!atnewline)
      fprintf(outfp, "\n");
    fprintf(outfp, "#line %d \"%s\"\n", location->line+1, location->file);
    lineno = location->line;
    atnewline = 1;
  }
}

void
generate_leadin(FILE *outfp, struct exp *expr)
{
  if(expr->leadin)
    generate_out(outfp, expr->leadin);
  generate_location(outfp, &expr->location);
}

/****************************************************************************/

void
generate_typ(FILE *outfp, char *entityName, char *signalName,
             struct typ *type, char *varname)
{
  if((type->type & tUNSIGNED))
    generate_out(outfp,"unsigned ");
  switch(type->type & 0xf) {
  case tVOID:
    generate_out(outfp,"void");
    break;
  case tBOOLEAN:
    generate_out(outfp,"bool");
    break;
  case tBYTE:
    if(!(type->type & tUNSIGNED))
      generate_out(outfp,"unsigned ");
    generate_out(outfp,"char");
    break;
  case tCHARACTER:
    generate_out(outfp,"char");
    break;
  case tSHORT:
    generate_out(outfp,"short");
    break;
  case tINTEGER:
    generate_out(outfp,"int");
    break;
  case tFLOAT:
    generate_out(outfp,"float");
    break;
  case tDOUBLE:
    generate_out(outfp,"double");
    break;
  case tLONG:
    generate_out(outfp,"long");
    break;
  case tLONGLONG:
    generate_out(outfp,"long long");
    break;
  case tSTRING:
    generate_out(outfp,"std::string");
    break;
  case tTYPENAME:
    generate_out(outfp,"%s",type->name);
    break;
  case tENTITY:
    generate_out(outfp,"%s *",type->name);
    break;
  }
  if((type->type & 0xf) != tENTITY)
    generate_out(outfp," ");
  if((type->type & tARRAY))
    if(type->length) {
      assert(varname && *varname);
      generate_out(outfp,"%s[", varname);
      generate_exp(outfp, entityName, signalName, type->length);
      generate_out(outfp,"]");
    } else {
      generate_out(outfp,"*");
      if(varname && *varname)
        generate_out(outfp,"%s", varname);
    }
  else
    if(varname && *varname)
      generate_out(outfp,"%s", varname);
}

void
generate_exp(FILE *outfp, char *entityName, char *signalName, struct exp *expr)
{
  char *s;
  if(!expr)
    return;
  assert(!expr->children || !expr->contents);
  if(expr->children) {
    generate_exp(outfp, entityName, signalName, expr->children);
  } else {
    switch(expr->type) {
    case ePLAIN:
      generate_leadin(outfp, expr);
      if(expr->contents)
        generate_out(outfp,"%s",expr->contents);
      break;
    case eIDENT:
      generate_leadin(outfp, expr);
      generate_out(outfp, "%s",
                      decl_getident(entityName,signalName,expr->contents));
      break;
    case eSTRING:
      generate_leadin(outfp, expr);
      generate_out(outfp, "\"");
      for(s=expr->contents; *s; s++) {
        if(isprint(*s))
          generate_out(outfp, "%c", *s);
        else
          switch(*s) {
          case '\n':  generate_out(outfp,"\\n");  break;
          case '\t':  generate_out(outfp,"\\t");  break;
          case '\r':  generate_out(outfp,"\\r");  break;
          case '\b':  generate_out(outfp,"\\b");  break;
          case '\f':  generate_out(outfp,"\\f");  break;
          default:    generate_out(outfp,"\\%3.3o",*s);
          }
      }
      generate_out(outfp, "\"");
      break;
    case eLITERAL:
      generate_leadin(outfp, expr);
      generate_out(outfp, "%s", expr->contents);
      break;
    }
  }
  if(expr->next) {
    generate_out(outfp, " ");
    generate_exp(outfp, entityName, signalName, expr->next);
  }
}

void
generate_blk(FILE *outfp, char *entityName, char *signalName,
             struct blk *block, int indent, int pass, int uniqnumber)
{
  int i;
  struct itm *run;
  struct itm *targetEntity, *targetQueue;
  switch(block->type) {
  case bPLAIN:
    assert(!block->truebranch && !block->falsebranch);
    assert(block->preloop);
    switch(pass) {
    case 3:
      generate_out(outfp,"%s",indentspacing(indent));
      generate_exp(outfp, entityName, signalName, block->preloop);
      generate_out(outfp,";\n");
      break;
    }
    break;
  case bIF:
    switch(pass) {
    case 3:
      generate_out(outfp,"%sif(",indentspacing(indent));
      generate_exp(outfp, entityName, signalName, block->condition);
      generate_out(outfp,") {\n");
      break;
    }
    generate_blk(outfp, entityName, signalName, block->truebranch, indent+1, pass, uniqnumber);
    if(block->falsebranch) {
      if(block->falsebranch->type == bELIF ||
         block->falsebranch->type == bELSE) {
        if(pass == 3)
          generate_out(outfp,"%s} ",indentspacing(indent));
        generate_blk(outfp, entityName, signalName, block->falsebranch, indent+1, pass, uniqnumber);
      } else {
        if(pass == 3)
          generate_out(outfp,"%s} else {\n",indentspacing(indent));
        generate_blk(outfp, entityName, signalName, block->falsebranch, indent+1, pass, uniqnumber);
        if(pass == 3)
          generate_out(outfp,"%s}\n",indentspacing(indent));
      }
    } else
      if(pass == 3)
        generate_out(outfp,"%s}\n",indentspacing(indent));
    break;
  case bELIF:
    switch(pass) {
    case 3:
      generate_out(outfp,"else if(");
      generate_exp(outfp, entityName, signalName, block->condition);
      generate_out(outfp,") {\n");
      break;
    }
    generate_blk(outfp, entityName, signalName, block->truebranch, indent+1, pass, uniqnumber);
    if(block->falsebranch) {
      if(block->falsebranch->type == bELIF ||
         block->falsebranch->type == bELSE) {
        if(pass == 3)
          generate_out(outfp,"%s} ",indentspacing(indent));
        generate_blk(outfp, entityName, signalName, block->falsebranch, indent+1, pass, uniqnumber);
      } else {
        if(pass == 3)
          generate_out(outfp,"%s} else {",indentspacing(indent));
        generate_blk(outfp, entityName, signalName, block->falsebranch, indent+1, pass, uniqnumber);
        if(pass == 3)
          generate_out(outfp,"%s}\n",indentspacing(indent));
      }
    } else
      if(pass == 3)
        generate_out(outfp,"%s}\n",indentspacing(indent));
    break;
  case bELSE:
    switch(pass) {
    case 3:
      generate_out(outfp,"else {\n");
      break;
    }
    generate_blk(outfp, entityName, signalName, block->falsebranch, indent+1, pass, uniqnumber);
    if(pass == 3)
      generate_out(outfp,"%s}\n",indentspacing(indent));
    break;
  case bFOR:
    switch(pass) {
    case 3:
      generate_out(outfp,"%sfor(",indentspacing(indent));
      generate_exp(outfp, entityName, signalName, block->preloop);
      generate_out(outfp,"; ");
      generate_exp(outfp, entityName, signalName, block->condition);
      generate_out(outfp,"; ");
      generate_exp(outfp, entityName, signalName, block->postloop);
      generate_out(outfp,") {\n");
      break;
    }
    generate_blk(outfp, entityName, signalName, block->truebranch, indent+1, pass, uniqnumber);
    if(pass == 3)
      generate_out(outfp,"%s}\n",indentspacing(indent));
    break;
  case bDOWHILE:
    if(pass == 3)
      generate_out(outfp,"%sdo {",indentspacing(indent));
    generate_blk(outfp, entityName, signalName, block->truebranch, indent+1, pass, uniqnumber);
    switch(pass) {
    case 3:
      generate_out(outfp,"%s} while(\n",indentspacing(indent));
      generate_exp(outfp, entityName, signalName, block->condition);
      generate_out(outfp,");\n");
      break;
    }
    break;
  case bWHILE:
    switch(pass) {
    case 3:
      generate_out(outfp,"%swhile(",indentspacing(indent));
      generate_exp(outfp, entityName, signalName, block->condition);
      generate_out(outfp,") {\n");
      break;
    }
    generate_blk(outfp, entityName, signalName, block->truebranch, indent+1, pass, uniqnumber);
    if(pass == 3)
      generate_out(outfp,"%s}\n",indentspacing(indent));
    break;
  case bCOMM:
    assert(block->label && block->label->id >= 0);
    switch(block->comm.type) {
    case cWAIT:
      switch(pass) {
      case 3:
        generate_out(outfp, "%sstate->wait(", indentspacing(indent));
        generate_exp(outfp, entityName, signalName, block->comm.params.time);
        generate_out(outfp,");\n");
        generate_out(outfp, "%sreturn %d;", indentspacing(indent), block->label->id);
        break;
      }
      break;
    case cREPLY:
      switch(pass) {
      case 3:
        if(block->comm.params.rtvalue) {
          generate_out(outfp, "%sstate->vars._%s->args()._ = ", indentspacing(indent), signalName);
          generate_exp(outfp, entityName, signalName, block->comm.params.rtvalue);
          generate_out(outfp, ";\n");
        }
        generate_out(outfp, "%sstate->vars._%s->reply();\n", indentspacing(indent), signalName);
        generate_out(outfp, "%sreturn %d;\n", indentspacing(indent), block->label->id);
        break;
      }
      break;
    case cRECV:
      run = block->comm.params.queues.first;
      assert(run && run->type == iNAME);
      if(run->next == NULL) {
        switch(pass) {
        case 3:
          generate_out(outfp, "%srecv(state, queue_%s);\n", indentspacing(indent), run->name);
          generate_out(outfp, "%sreturn %d;\n", indentspacing(indent), block->label->id);
          break;
        }
      } else {
        int numqueues=0;
        for(run=block->comm.params.queues.first; run; run=run->next)
          ++numqueues;
        switch(pass) {
        case 1:
          generate_out(outfp, "  fornax::QueueBase *queues_%d[%d];\n", uniqnumber, numqueues);
          break;
        case 2:
          for(i=0, run=block->comm.params.queues.first; run; run=run->next, i++) {
            assert(run->type == iNAME);
            generate_out(outfp, "    queues_%d[%d] = &queue_%s;\n", uniqnumber, i, run->name);
          }
          break;
        case 3:
          generate_out(outfp, "%srecv(state, %d, queues_%d);\n", indentspacing(indent), numqueues, uniqnumber);
          generate_out(outfp, "%sreturn %d;\n", indentspacing(indent), block->label->id);
          break;
        }
      }
      break;
    case cRECVREPLY:
      switch(pass) {
      case 3:
        if(block->comm.params.recvreply.lvalue) {
          generate_out(outfp, "%s", indentspacing(indent));
          generate_exp(outfp, entityName, signalName, block->comm.params.recvreply.lvalue);
          generate_out(outfp, " = state->vars._%d->args()._;\n", uniqnumber);
        }
        generate_out(outfp, "%sdelete state->vars._%d;\n", indentspacing(indent), uniqnumber);
        generate_out(outfp, "%sreturn %d;\n", indentspacing(indent), block->label->id);
        break;
      }
      break;
    case cSEND:
      if(gettargettype(entityName, signalName, block->comm.params.send.entity, block->comm.params.send.queue, &targetEntity, &targetQueue)) {
        switch(pass) {
        case 3:
          generate_out(outfp, "%s{ struct %s_%s evtdata = { ", indentspacing(indent), targetEntity->name, targetQueue->name);
          generate_exp(outfp, entityName, signalName, block->comm.params.send.args);
          generate_out(outfp, " };\n");
          break;
        }
        switch(targetQueue->type) {
        case iASYNCSIGNAL:
          switch(pass) {
          case 3:
            generate_out(outfp, "%sfornax::Event<struct %s_%s> evt(evtdata);\n", indentspacing(indent), targetEntity->name, targetQueue->name);
            generate_out(outfp, "%s(*", indentspacing(indent));
            generate_exp(outfp, entityName, signalName, block->comm.params.send.entity);
            generate_out(outfp, ").queue_%s.send(evt);\n", block->comm.params.send.queue);
            generate_out(outfp, "%sreturn %d; }\n", indentspacing(indent), block->label->id);
            break;
          }
          break;
        case iTHRSIGNAL:
        case iSYNCSIGNAL:
          /* We will use block->label->id instead of uniqnumber, which due to this particular implementation (!) is
           * equal to the uniqnumber when generating the cRECVREPLY entry.
           */
          switch(pass) {
          case 0:
            generate_out(outfp, "  fornax::Event<struct %s_%s> *_%d;\n", targetEntity->name, targetQueue->name, block->label->id);
            break;
          case 3:
            generate_out(outfp, "%sstate->vars._%d = new fornax::Event<struct %s_%s>(evtdata);\n", indentspacing(indent), block->label->id, targetEntity->name, targetQueue->name);
            generate_out(outfp, "%s(*", indentspacing(indent));
            generate_exp(outfp, entityName, signalName, block->comm.params.send.entity);
            generate_out(outfp, ").queue_%s.send(state, state->vars._%d);\n", block->comm.params.send.queue, block->label->id);
            generate_out(outfp, "%sreturn %d; }\n", indentspacing(indent), block->label->id);
            break;
          }
          break;
        default:
          ;
        }
      } else
        yyerror("cannot determine send target entity or queue type for context %s.%s looking for signal %s",entityName,signalName,block->comm.params.send);
      break;
    default:
      assert(!"Unknown comm block type");
    }
    break;
  case bLABEL:
    assert(!"label statement not allowed");
    break;
  case bJUMP:
    switch(pass) {
    case 3:
      generate_out(outfp,"%sreturn %d;\n",indentspacing(indent),
              block->label->id);
      break;
    }
    break;
  default:
    assert(!"Unknown block type");
  }
  if(block->next)
    generate_blk(outfp, entityName, signalName, block->next, indent, pass, uniqnumber);
}

void
generate(FILE *outfp, struct lst *list)
{
  short flag;
  struct itm *entity;
  struct itm *signal;
  struct itm *decl;
  struct itm *item;
  struct blklabel *codefrag;
  char varname[1024];

  if(list->first && list->first->type == iLITERAL) {
    generate_location(outfp,&list->first->location);
    generate_out(outfp,"%s",list->first->name);
  }
  generate_out(outfp,"#include <fornax.h>\n");
  generate_out(outfp,"\n");
  for(entity=list->first; entity; entity=entity->next) {
    if(entity->type == iENTITY) {
      generate_location(outfp,&entity->location);
      generate_out(outfp,"class %s;\n",entity->name);
    }
  }
  generate_out(outfp,"\n");
  for(entity=list->first; entity; entity=entity->next) {
    if(entity->type == iENTITY) {
      for(signal=entity->decls.first; signal; signal=signal->next) {
        switch(signal->type) {
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
        case iMAINSIGNAL:
          generate_location(outfp,&signal->location);
          if(signal->type == iMAINSIGNAL)
            generate_out(outfp,"struct %s_main {\n",entity->name);
          else
            generate_out(outfp,"struct %s_%s {\n",entity->name,signal->name);
          for(decl=signal->args.first; decl; decl=decl->next) {
            generate_location(outfp,&signal->location);
            generate_out(outfp,"  ");
            generate_typ(outfp, entity->name, signal->name, &decl->rttype, decl->name);
            generate_out(outfp,";\n");
          }
          if(signal->type == iSYNCSIGNAL || signal->type == iTHRSIGNAL) {
            if(signal->rttype.type != tVOID) {
              generate_location(outfp,&signal->location);
              generate_out(outfp,"  ");
              generate_typ(outfp, entity->name, signal->name, &signal->rttype, "_");
              generate_out(outfp,";\n");
            }
          }
          generate_out(outfp,"};\n");
          break;
        default:
          ;
        }
      }
    }
  }
  generate_out(outfp,"\n");
  for(entity=list->first; entity; entity=entity->next) {
    if(entity->type == iENTITY) {
      generate_location(outfp,&entity->location);
      generate_out(outfp,"struct %s_vars {\n",entity->name);
      for(item=entity->decls.first; item; item=item->next) {
        switch(item->type) {
        case iDECL:
          generate_out(outfp,"  ");
          snprintf(varname,sizeof(varname)-1,"* %s",item->name);
          generate_typ(outfp, entity->name, NULL, &item->rttype, varname);
          generate_out(outfp,";\n  ");
          snprintf(varname,sizeof(varname)-1,"_%s",item->name);
          generate_typ(outfp, entity->name, NULL, &item->rttype, varname);
          generate_out(outfp,";\n");
          break;
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
        case iMAINSIGNAL:
          generate_location(outfp,&item->location);
          if(item->type != iMAINSIGNAL)
            generate_out(outfp, "  fornax::Event<struct %s_%s> *_%s;\n", entity->name, item->name, item->name);
          for(decl=item->args.first; decl; decl=decl->next) {
            generate_location(outfp,&item->location);
            generate_out(outfp, "  ");
            if(item->type == iMAINSIGNAL)
              snprintf(varname,sizeof(varname)-1,"main_%s",decl->name);
            else
              snprintf(varname,sizeof(varname)-1,"%s_%s",item->name,decl->name);
            varname[sizeof(varname)-1] = '\0';
            generate_typ(outfp, entity->name, NULL, &decl->rttype, varname);
            generate_out(outfp, ";\n");
          }
          for(decl=item->decls.first; decl; decl=decl->next) {
            generate_location(outfp,&item->location);
            generate_out(outfp, "  ");
            if(item->type == iMAINSIGNAL)
              snprintf(varname,sizeof(varname)-1,"main_%s",decl->name);
            else
              snprintf(varname,sizeof(varname)-1,"%s_%s",item->name,decl->name);
            varname[sizeof(varname)-1] = '\0';
            generate_typ(outfp, entity->name, NULL, &decl->rttype, varname);
            generate_out(outfp, ";\n");
          }
          break;
        default:
          ;
        }
      }
      /* Generate block based items, Pass 0: Generate local event variables */
      for(codefrag=entity->labels; codefrag; codefrag=codefrag->next)
        generate_blk(outfp, entity->name, codefrag->signal->name, codefrag->entrypoint->next, 2, 0, codefrag->entrypoint->label->id);
      /* End Pass 0 */
      generate_out(outfp,"};\n");
    }
  }
  generate_out(outfp,"\n");
  for(flag=0, entity=list->first; entity; entity=entity->next) {
    if(flag)
      generate_out(outfp,"\n");
    else
      flag = 1;
    if(entity->type == iLITERAL) {
      if(entity != list->first) {
        generate_location(outfp,&entity->location);
        generate_out(outfp,"%s",entity->name);
      }
    } else if(entity->type == iENTITY) {
      generate_out(outfp,"class %s\n",entity->name);
      generate_out(outfp,"  : public fornax::Entity<struct %s_vars, struct %s_main>\n",
             entity->name,entity->name);
      generate_out(outfp,"{\n");
      generate_out(outfp,"private:\n");
      generate_out(outfp,"  struct %s_main mainargs;\n",entity->name);
      generate_out(outfp,"public:\n");
      for(signal=entity->decls.first; signal; signal=signal->next) {
        switch(signal->type) {
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
          generate_out(outfp,"  fornax::Queue<struct %s_%s> queue_%s;\n", entity->name,
                 signal->name, signal->name);
          break;
        default:
          ;
        }
      }
      /* Generate block based items, Pass 1: Output definition for multiple queue wait list */
      for(codefrag=entity->labels; codefrag; codefrag=codefrag->next)
        generate_blk(outfp, entity->name, codefrag->signal->name, codefrag->entrypoint->next, 2, 1, codefrag->entrypoint->label->id);
      /* End Pass 1 */
      generate_out(outfp,"  %s(fornax::System &system, std::string name)\n",
              entity->name);
      generate_out(outfp,"    : fornax::Entity<struct %s_vars, struct %s_main>"
              "(system,name)", entity->name, entity->name);
      for(signal=entity->decls.first; signal; signal=signal->next) {
        switch(signal->type) {
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
          generate_out(outfp,",\n      queue_%s(this, \"%s\", %d)", signal->name,
                 signal->name, signal->code->label->id);
          break;
        default:
          ;
        }
      }
      generate_out(outfp,"\n  {\n");
      /* Generate block based items, Pass 2: Output initialization for multiple queue wait list */
      for(codefrag=entity->labels; codefrag; codefrag=codefrag->next)
        generate_blk(outfp, entity->name, codefrag->signal->name, codefrag->entrypoint->next, 2, 2, codefrag->entrypoint->label->id);
      /* End Pass 2 */
      generate_out(outfp,"  };\n");
      generate_out(outfp,"  virtual ~%s() {};\n",entity->name);
      generate_out(outfp,"  virtual fornax::StateId step(fornax::State<struct "
              "%s_vars> *);\n", entity->name);


      for(signal=entity->decls.first; signal; signal=signal->next) {
        switch(signal->type) {
        case iASYNCSIGNAL:
        case iSYNCSIGNAL:
        case iTHRSIGNAL:
          generate_out(outfp, "  fornax::Event<struct %s_%s>* send_%s(fornax::Time _time", entity->name, signal->name, signal->name);
          for(decl=signal->args.first; decl; decl=decl->next) {
            generate_out(outfp, ", ");
            generate_typ(outfp, entity->name, signal->name, &decl->rttype, decl->name);
          }
          generate_out(outfp, ") {\n");
          generate_out(outfp, "    struct %s_%s evtdata = { ", entity->name, signal->name);
          for(decl=signal->args.first; decl; decl=decl->next) {
            generate_out(outfp, "%s", decl->name);
            if(decl->next)
              generate_out(outfp, ", ");
          }
          generate_out(outfp, " };\n");
          generate_out(outfp, "    return queue_%s.deliver(_time, evtdata);\n", signal->name);
          generate_out(outfp, "  };\n");
          break;
        case iMAINSIGNAL:
          generate_out(outfp,"  // C++ sucks, this next function should be "
                  "called main() but C++ does\n");
          generate_out(outfp,"  // not allow overloading of template functions\n");
          generate_out(outfp,"  void init(");
          for(decl=signal->args.first; decl; decl=decl->next) {
            generate_typ(outfp, entity->name, signal->name, &decl->rttype, decl->name);
            if(decl->next)
              generate_out(outfp,", ");
          }
          generate_out(outfp,") {\n");
          for(decl=signal->args.first; decl; decl=decl->next) {
            generate_out(outfp,"    mainargs.%s = %s;\n",decl->name,decl->name);
          }
          generate_out(outfp,"    main(fornax::Event<struct %s_main>(mainargs));\n",entity->name);
          generate_out(outfp,"  };\n");
          break;
        default:
          break;
        }
      }
      generate_out(outfp,"};\n");
    }




  }
  generate_out(outfp,"\n");
  for(flag=0, entity=list->first; entity; entity=entity->next) {
    if(entity->type == iENTITY) {
      if(flag)
        generate_out(outfp,"\n");
      else
        flag = 1;
      generate_out(outfp,"fornax::StateId\n");
      generate_out(outfp,"%s::step(fornax::State<struct %s_vars> *state)\n",
              entity->name, entity->name);
      generate_out(outfp,"{\n");
      generate_out(outfp,"  switch(state->id) {\n");
      for(codefrag=entity->labels; codefrag; codefrag=codefrag->next) {
        generate_out(outfp,"  case %d: {\n", codefrag->id);

        if(codefrag->id == 0)
          for(decl=entity->decls.first; decl; decl=decl->next)
            if(decl->type == iDECL) {
              generate_out(outfp,"    state->vars.%s = &(state->vars._%s);\n",
                      decl->name,decl->name);
            }
        for(signal=entity->decls.first; signal; signal=signal->next) {
          assert(signal->type!=iMAINSIGNAL || (signal->code->type==bLABEL &&
                                               signal->code->label->id == 0));
          assert(codefrag->id!=0 || signal->type!=iMAINSIGNAL ||
                 (signal->code->type==bLABEL && signal->code->label->id==0));
          switch(signal->type) {
          case iMAINSIGNAL:
            assert(signal->code->type == bLABEL);
            if(signal->code->label->id == codefrag->id) {
              assert(codefrag->entrypoint == signal->code);
              if(signal->args.first) {
                generate_out(outfp,"    struct %s_main &evtdata = args();\n",
                        entity->name);
                for(decl=signal->args.first; decl; decl=decl->next) {
                  generate_out(outfp,"    state->vars.main_%s = evtdata.%s;\n",
                          decl->name, decl->name);
                }
              }
            }
            break;
          case iASYNCSIGNAL:
          case iSYNCSIGNAL:
          case iTHRSIGNAL:
            assert(signal->code->type == bLABEL);
            if(signal->code->label->id == codefrag->id) {
              assert(codefrag->entrypoint == signal->code);
              generate_out(outfp,"    state->vars._%s = queue_%s.event();\n",
                      signal->name, signal->name);
              if(signal->type == iTHRSIGNAL)
                generate_out(outfp, "   state->satisfy();\n");
              if(signal->args.first) {
                generate_out(outfp,"    struct %s_%s &evtdata = state->vars._%s->"
                        "args();\n", entity->name, signal->name, signal->name);
                for(decl=signal->args.first; decl; decl=decl->next) {
                  generate_out(outfp,"    state->vars.%s_%s = evtdata.%s;\n",
                          signal->name, decl->name, decl->name);
                }
              }
            }
            break;
          default:
            ;
          }
        }

        assert(codefrag->entrypoint->type == bLABEL);
        generate_blk(outfp, entity->name, codefrag->signal->name, codefrag->entrypoint->next, 2, 3, codefrag->entrypoint->label->id);
        generate_out(outfp,"  }\n");
      }
      generate_out(outfp,"  default:\n");
      generate_out(outfp,"    return state->error();\n");
      generate_out(outfp,"  }\n");
      generate_out(outfp,"}\n");
    }
  }
}
