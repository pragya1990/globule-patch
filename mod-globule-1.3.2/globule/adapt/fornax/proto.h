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
extern char *argv0;

extern FILE *outfp;
extern char *outfname;
extern char *outfbase;
extern int option_linedirectives;

#define CHECK(EX) do { if(EX) { int err = errno; fprintf(stderr, "operation" \
 " \"%s\" failed on line %d: %s (%d)\n", #EX, __LINE__, strerror(err), err); \
 abort(); }} while(0)

extern char *strndup(char *source, int len);
extern struct exp *expdup(struct exp *org);
extern struct blk *blkdup(struct blk *org);
extern struct itm *itmdup(struct itm *org);

/* extern FILE *yyin; */
extern int yyerror(char *msg,...);
extern int yylex(void);
extern int yyparse();

struct location {
  char *file;
  int line;
};
struct tok {
  char *leadin;
  char *contents;
  struct location location;
};
struct lst {
  struct itm *first;
  struct itm *last;
};
struct exp {
  enum exptype { ePLAIN, eSTRING, eIDENT, eLITERAL } type;
  struct exp *next;
  struct exp *children;
  char *contents;
  char *leadin;
  struct location location;
};
struct blklabel {
  int id;
  struct blk *entrypoint;
  struct itm *signal;
  struct blklabel *next;
  struct blklabel *prev;
};
extern struct blk blktemplate;
struct blk {
  enum blktype {
    bPLAIN,
    bCOMM,
    bIF,
    bELIF,
    bELSE,
    bFOR,
    bDOWHILE,
    bWHILE,
    bJUMP,
    bLABEL
  } type;
  struct exp *condition;
  struct exp *preloop;
  struct exp *postloop;
  struct blk *truebranch;
  struct blk *falsebranch;
  struct blklabel *label;
  struct blk *next;
  struct {
    enum commtype { cWAIT, cREPLY, cRECV, cSEND, cRECVREPLY } type;
    union {
      struct exp *time;    /* for cWAIT  */
      struct exp *rtvalue; /* for cREPLY */
      struct lst queues;   /* for cRECV  */
      struct {
        char *queue;
        struct exp *entity;
        struct exp *args;
        struct exp *lvalue;
      } send;             /* for cSEND */
      struct {
        struct exp *lvalue;
      } recvreply;        /* for cRECVREPLY */
    } params;
  } comm;
};
struct nam {
  char *name;
  /* char *path; */
  /* char *full; */
};
struct typ {
#define tSIGNED     0x10
#define tUNSIGNED   0x20
#define tARRAY      0x40
#define tVOID        1
#define tBOOLEAN     2
#define tBYTE        3
#define tCHARACTER   4
#define tSHORT       5
#define tINTEGER     6
#define tLONG        7
#define tLONGLONG    8
#define tFLOAT       9
#define tDOUBLE     10
#define tSTRING     11
#define tTYPENAME   12
#define tENTITY     13
  int type;
  char *name;
  struct exp *length;
};
enum itmtype {
  iENTITY,
  iLITERAL,
  iASYNCSIGNAL,
  iSYNCSIGNAL,
  iTHRSIGNAL,
  iMAINSIGNAL,
  iDECL,
  iNAME,
  iEXPR,
  iBODY
};
struct itm {
  enum itmtype type;
  struct itm *next;
  char *name; /* for iLITERAL the literal text */
  struct lst decls;
  struct lst args;
  struct blk *code;
  struct typ rttype;
  struct blklabel *labels;
  struct location location;
};

struct itm mkitem(enum itmtype type,...);

extern int mktoken(int num);
extern struct tok mktoken0();
extern void mkleadin();

extern struct typ mktype1(enum itmtype specifier);
extern struct typ mktype2(struct typ t1, struct typ t2);
extern struct typ mktype3(struct typ t1, struct typ t2, struct typ t3);
extern struct typ mktypei(enum itmtype specifier, struct nam name);
extern struct typ mktypex1(struct typ t1);
extern struct typ mktypex2(struct typ t1, struct exp e);

extern struct blk mkblock0();
extern struct blk mkblock1(struct exp e);
extern struct blk mkblock2(struct blk b1, struct blk b2);
extern struct blk mkblockif(struct exp e, struct blk b1, struct blk b2);
extern struct blk mkblockelif(struct exp e, struct blk b1, struct blk b2);
extern struct blk mkblockelse(struct blk b);
extern struct blk mkblockfor(struct exp, struct exp, struct exp, struct blk);
extern struct blk mkblockdowhile(struct blk b, struct exp e);
extern struct blk mkblockwhile(struct exp e, struct blk b);
extern struct blk mkblockwait(struct exp e);
extern struct blk mkblockreply(struct exp e);
extern struct blk mkblockrecv(struct lst l);
extern struct blk mkblocksend(struct exp, struct nam, struct exp, struct exp);

extern struct lst mklist0();
extern struct lst mklist1(struct itm i);
extern struct lst mklist2(struct itm i, struct lst l);
extern struct lst mklist3(struct lst l1, struct lst l2, struct itm i);
extern struct lst mklistjoin(struct lst l1, struct lst l2);

extern struct nam mkname0();
extern struct nam mkname1(struct tok t);
extern struct nam mkname2(struct nam n, struct tok t);

extern struct exp mkexprl(struct lst l);
extern struct exp mkident(struct nam n);
extern struct exp mkstring(struct tok t);
extern struct exp mkliteral(struct tok t);
extern struct exp mkexpr0();
extern struct exp mkexprt(struct tok t);
extern struct exp mkexprtet(struct tok t1, struct exp e, struct tok t2);
extern struct exp mkexprte(struct tok t, struct exp e);
extern struct exp mkexpretet(struct exp, struct tok, struct exp, struct tok);
extern struct exp mkexpretlt(struct exp, struct tok, struct lst, struct tok);
extern struct exp mkexpretete(struct exp,struct tok,struct exp,struct tok,
                              struct exp);
extern struct exp mkexprete(struct exp e1, struct tok t, struct exp e2);
extern struct exp mkexpret(struct exp e, struct tok t);
extern struct exp mkexprt(struct tok t);

extern char *indentspacing(int indentlevel);
extern struct itm *gettype(char *entity, char *signal, struct exp *expr);
extern int gettargettype(char *, char *, struct exp *targetExpr, char *qname,
                         struct itm **entity, struct itm **queue);

extern void output_exp(FILE *outfp, struct exp *expr);
extern void output_typ(FILE *outfp, struct typ *type);
extern void output_blk(FILE *outfp, struct blk *block, int indent);
extern void output_itm(FILE *outfp, struct itm *item, int indent);
extern void output(FILE *outfp, struct lst *l);

extern void generate_typ(FILE *, char *, char *, struct typ *, char *);
extern void generate_exp(FILE *, char *, char *, struct exp *);
extern void generate_blk(FILE *, char *, char *, struct blk *, int, int, int);
extern void generate(FILE *outfp, struct lst *l);

extern char *decl_getident(char *entity, char *signal, char *variable);
extern struct itm *decl_getitem(char *entity, char *signal, char *variable);
extern void decl_collect(struct lst *list);
extern void decl_dump();

extern void transform(struct lst *l);
