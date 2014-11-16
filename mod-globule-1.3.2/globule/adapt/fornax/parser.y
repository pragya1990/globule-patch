%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include "proto.h"
%}

%start system
%expect 99

%union {
  struct tok tok;
  struct itm itm;
  struct nam nam;
  struct typ typ;
  struct exp exp;
  struct blk blk;
  struct lst lst;
}

%type <lst> system entities signals decls decl idents opt_list list opt_params params
%type <itm> entity signal entry param fullbody
%type <blk> body stmts stmt elif_stmt else_stmt
%type <typ> type_sign opt_type_sign type_size type_int opt_type_int type_bool
%type <typ> type_signed type_basic type
%type <nam> ident
%type <exp> opt_expr expr
%type <tok> elif_tok opt_scolon

%type <tok> T__BOOL T__BOOLEAN T__BYTE T__CHAR T__DO T__DOUBLE T__ELIF T__ELSE T__ELSIF
%type <tok> T__ENTITY T__FALSE T__FLOAT T__FOR T__IF T__INT T__INTEGER T__LONG T__MAIN T__RETURN
%type <tok> T__SHORT T__SIGNAL T__SIGNED T__STRING T__STRUCT T__THREADED T__TRUE
%type <tok> T__TYPENAME T__UNSIGNED T__VARS T__VOID T__WHILE
%type <tok> T__RECV T__REPLY T__SEND T__TIME T__WAIT
%type <tok> T_LPAR T_RPAR T_LCUR T_RCUR T_LBRA T_RBRA
%type <tok> T_DEFINE T_DCOLON T_ASSIGN T_COLON T_QUEST T_REF
%type <tok> T_ISMINUS T_ISPLUS T_ISMULT T_ISDIV T_ISMOD
%type <tok> T_IS T_EQ T_NE T_GE T_LE T_GT T_LT T_OR T_AND T_BAR T_TILDE T_AMP T_EXCL
%type <tok> T_SCOLON T_MINUS T_PLUS T_POW T_STAR T_DIV T_COMMA T_DOT T_INCR T_DECR
%type <tok> T_INTEGER T_IDENT T_NUMBER T_STRING T_LITERAL
%type <tok> T_ERROR

%token T__BOOL T__BOOLEAN T__BYTE T__CHAR T__DO T__DOUBLE T__ELIF T__ELSE T__ELSIF
%token T__ENTITY T__FALSE T__FLOAT T__FOR T__IF T__INT T__INTEGER T__LONG T__MAIN T__RETURN
%token T__SHORT T__SIGNAL T__SIGNED T__STRING T__STRUCT T__THREADED T__TRUE
%token T__TYPENAME T__UNSIGNED T__VARS T__VOID T__WHILE
%token T__RECV T__REPLY T__SEND T__TIME T__WAIT
%token T_LPAR T_RPAR T_LCUR T_RCUR T_LBRA T_RBRA
%token T_DEFINE T_DCOLON T_ASSIGN T_COLON T_QUEST T_REF
%token T_ISMINUS T_ISPLUS T_ISMULT T_ISDIV T_ISMOD
%token T_IS T_EQ T_NE T_GE T_LE T_GT T_LT T_OR T_AND T_BAR T_TILDE T_AMP T_EXCL
%token T_SCOLON T_MINUS T_PLUS T_POW T_STAR T_DIV T_COMMA T_DOT T_INCR T_DECR
%token T_INTEGER T_IDENT T_NUMBER T_STRING T_LITERAL
%token T_ERROR

%left T_DOT T_REF SUBSCRIBE FUNCTION POSTFIX
%right PREFIX T_TILDE T_NOT
%right UNARYMINUS UNARYPLUS
%left T_AMP DEREFERENCE
%right T_POW
%left T_STAR T_DIV T_MOD
%left T_PLUS T_MINUS
%left T_LT T_LE T_GT T_GE
%left T_EQ T_NE
%right T_EXCL
%left T_AND
%left T_OR
%left T_QUEST
%right T_IS T_ASSIGN T_ISMINUS T_ISPLUS T_ISMULT T_ISDIV T_ISMOD
%left T_COMMA

%%

system		: entities							{ $$ = $1;
										  decl_collect(&($$));
										  /* { FILE *fp; fp = fopen("cfornaxc.output","w"); output(fp,&($$)); fclose(fp); } */
										  transform(&($$));
										  generate(outfp, &($$));
										}
		;
entities	: entity entities						{ $$ = mklist2($1,$2); }
		| entity							{ $$ = mklist1($1); }
		;
entity		: T__ENTITY ident T_LCUR decls signals entry T_RCUR opt_scolon	{ $$ = mkitem(iENTITY,$1,$2.name,mklist3($4,$5,$6)); }
		| T_LITERAL							{ $$ = mkitem(iLITERAL,$1); }
		;
signals		: signal signals						{ $$ = mklist2($1,$2); }
		|								{ $$ = mklist0(); }
		;
signal		: T__THREADED type ident T_LPAR opt_params T_RPAR fullbody	{ $$ = mkitem(iTHRSIGNAL,$1,$3.name,$2,$5,$7); }
		| T__SIGNAL type ident T_LPAR opt_params T_RPAR fullbody	{ $$ = mkitem(iSYNCSIGNAL,$1,$3.name,$2,$5,$7); }
		| T__SIGNAL ident T_LPAR opt_params T_RPAR fullbody		{ $$ = mkitem(iASYNCSIGNAL,$1,$2.name,mktype1(tVOID),$4,$6); }
		;
entry		: T__MAIN T_LPAR opt_params T_RPAR fullbody			{ $$ = mkitem(iMAINSIGNAL,$1,"",mktype1(tVOID),$3,$5); }
		;
opt_scolon	: T_SCOLON							{ $$ = $1; }
		|								{ $$ = mktoken0(); }
		;
decls		: decl T_SCOLON decls						{ $$ = mklistjoin($1,$3); }
		|								{ $$ = mklist0(); }
		;
decl		: type idents							{ $$ = mklist0();
										  { struct itm *item;
										  for(item=$2.first; item; item=item->next) {
										    assert(item->type == iNAME);
										    $$ = mklist2(mkitem(iDECL,item->name,$1),$$);
										  } }
										}
		;
idents		: ident T_COMMA idents						{ $$ = mklist2(mkitem(iNAME,$1.name), $3); }
		| ident								{ $$ = mklist1(mkitem(iNAME,$1.name)); }
		;
type_sign	: T__SIGNED							{ $$ = mktype1(tSIGNED); }
		| T__UNSIGNED							{ $$ = mktype1(tUNSIGNED); }
		;
opt_type_sign	: type_sign							{ $$ = $1; }
		|								{ $$ = mktype1(tSIGNED); }
		;
type_size	: T__SHORT							{ $$ = mktype1(tSHORT); }
		| T__LONG							{ $$ = mktype1(tLONG); }
		| T__LONG T__LONG						{ $$ = mktype1(tLONGLONG); }
		;
type_int	: T__INT							{ $$ = mktype1(tINTEGER); }
		| T__INTEGER							{ $$ = mktype1(tINTEGER); }
		;
opt_type_int	: type_int							{ $$ = $1; }
		|								{ $$ = mktype1(tINTEGER); }
		;
type_bool	: T__BOOL							{ $$ = mktype1(tBOOLEAN); }
		| T__BOOLEAN							{ $$ = mktype1(tBOOLEAN); }
		;
type_signed	: T__CHAR							{ $$ = mktype1(tCHARACTER); }
		| T__BYTE							{ $$ = mktype1(tBYTE); }
		| type_int							{ $$ = $1; }
		;
type_basic	: opt_type_sign type_size opt_type_int				{ $$ = mktype3($1,$2,$3); }
		| opt_type_sign type_signed 					{ $$ = mktype2($1,$2); }
		| type_sign							{ $$ = $1; }
		| type_bool							{ $$ = $1; }
		| T__FLOAT							{ $$ = mktype1(tFLOAT); }
		| T__DOUBLE							{ $$ = mktype1(tDOUBLE); }
		| T__STRING							{ $$ = mktype1(tSTRING); }
		| T__TYPENAME ident						{ $$ = mktypei(tTYPENAME,$2); }
		| T__ENTITY ident						{ $$ = mktypei(tENTITY,$2); }
		| T__VOID							{ $$ = mktype1(tVOID); }
		;
type		: type_basic							{ $$ = $1; }
		| type_basic T_LBRA T_RBRA					{ $$ = mktypex1($1); }
		| type_basic T_LBRA expr T_RBRA					{ $$ = mktypex2($1,$3); }
		;
opt_list	: list								{ $$ = $1; }
		|								{ $$ = mklist0(); }
		;
list		: expr T_COMMA list						{ $$ = mklist2(mkitem(iEXPR,$1),$3); }
		| expr								{ $$ = mklist1(mkitem(iEXPR,$1)); }
		;
opt_params	: params							{ $$ = $1; }
		|								{ $$ = mklist0(); }
		;
params		: param T_COMMA params						{ $$ = mklist2($1,$3); }
		| param								{ $$ = mklist1($1); }
		;
param		: type ident							{ $$ = mkitem(iDECL,$2.name,$1); }
		;
fullbody	: T_LCUR decls stmts T_RCUR					{ $$ = mkitem(iBODY,$2,$3); }
		;
body		: T_LCUR stmts T_RCUR						{ $$ = $2; }
		;
stmts		: stmts stmt							{ $$ = mkblock2($1,$2); }
		|								{ $$ = mkblock0(); }
		;
stmt		: T__IF T_LPAR expr T_RPAR stmt elif_stmt					{ $$ = mkblockif($3,$5,$6); }
		| T__FOR T_LPAR opt_list T_SCOLON opt_list T_SCOLON opt_list T_RPAR stmt	{ $$ = mkblockfor(mkexprl($3),mkexprl($5),mkexprl($7),$9); }
		| T__DO body T__WHILE T_LPAR expr T_RPAR					{ $$ = mkblockdowhile($2,$5); }
		| T__WHILE T_LPAR expr T_RPAR stmt						{ $$ = mkblockwhile($3,$5); }
		| T__WAIT expr T_SCOLON								{ $$ = mkblockwait($2); }
		| T__REPLY opt_expr T_SCOLON							{ $$ = mkblockreply($2); }
		| T__RECV idents T_SCOLON							{ $$ = mkblockrecv($2); }
		| T__SEND expr T_REF ident T_LPAR opt_list T_RPAR T_SCOLON			{ $$ = mkblocksend($2,$4,mkexprl($6),mkexpr0()); }
		| expr T_IS     T__SEND expr T_REF ident T_LPAR opt_list T_RPAR T_SCOLON	{ $$ = mkblocksend($4,$6,mkexprl($8),$1); }
		| expr T_ASSIGN T__SEND expr T_REF ident T_LPAR opt_list T_RPAR T_SCOLON	{ $$ = mkblocksend($4,$6,mkexprl($8),$1); }
		| opt_expr T_SCOLON								{ $$ = mkblock1($1); }
		| body										{ $$ = $1; }
		;
elif_tok	: T__ELIF					{ $$ = $1; }
		| T__ELSIF					{ $$ = $1; }
		;
elif_stmt	: elif_tok T_LPAR expr T_RPAR stmt elif_stmt	{ $$ = mkblockelif($3,$5,$6); }
		| else_stmt					{ $$ = mkblockelse($1); }
		;
else_stmt	: T__ELSE stmt					{ $$ = $2; }
		|						{ $$ = mkblock0(); }
		;
opt_expr	: expr						{ $$ = $1; }
		|						{ $$ = mkexpr0(); }
		;
expr		: T_LPAR expr T_RPAR				{ $$ = mkexprtet($1,$2,$3); }
		| T_AMP expr					{ $$ = mkexprte($1,$2); }
		| T_STAR expr %prec DEREFERENCE			{ $$ = mkexprte($1,$2); }
		| ident T_LBRA expr T_RBRA %prec SUBSCRIBE	{ $$ = mkexpretet(mkident($1),$2,$3,$4); }
		| expr T_LPAR opt_list T_RPAR %prec FUNCTION	{ $$ = mkexpretlt($1,$2,$3,$4); }
		| expr T_QUEST expr T_COLON expr %prec T_QUEST	{ $$ = mkexpretete($1,$2,$3,$4,$5); }
		| expr T_IS expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_ASSIGN expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_ISMINUS expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_ISPLUS expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_ISMULT expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_ISDIV expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_ISMOD expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_GE expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_LE expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_GT expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_LT expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_EQ expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_NE expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_OR expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_AND expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_MINUS expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_PLUS expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_POW expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_STAR expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_DIV expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_DOT expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_REF expr				{ $$ = mkexprete($1,$2,$3); }
		| expr T_DECR %prec POSTFIX			{ $$ = mkexpret($1,$2); }
		| expr T_INCR %prec POSTFIX			{ $$ = mkexpret($1,$2); }
		| T_INCR expr %prec PREFIX			{ $$ = mkexprte($1,$2); }
		| T_DECR expr %prec PREFIX			{ $$ = mkexprte($1,$2); }
		| T_EXCL expr					{ $$ = mkexprte($1,$2); }
		| T_MINUS expr %prec UNARYMINUS			{ $$ = mkexprte($1,$2); }
		| T_PLUS expr %prec UNARYPLUS			{ $$ = mkexprte($1,$2); }
		| T_INTEGER					{ $$ = mkexprt($1); }
		| T_NUMBER					{ $$ = mkexprt($1); }
		| T__TRUE					{ $$ = mkexprt($1); }
		| T__FALSE					{ $$ = mkexprt($1); }
		| T_STRING					{ $$ = mkstring($1); }
		| ident						{ $$ = mkident($1); }
		| T__TIME					{ $$ = mkident(mkname1($1)); }
		| T_LITERAL					{ $$ = mkliteral($1); }
		;
ident		: ident T_DCOLON T_IDENT			{ $$ = mkname2($1,$3); }
		| T_IDENT					{ $$ = mkname1($1); }
		;

%%
