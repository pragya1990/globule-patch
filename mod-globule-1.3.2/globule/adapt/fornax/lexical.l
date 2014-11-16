%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <errno.h>
#include "proto.h"
#include "parser.h"

char *argv0;

static struct stack {
  YY_BUFFER_STATE buffer;
  int line;
  char *file;
  struct stack *prev;
} *top = NULL;
static int line;       // current line number
static char *file;     // current file name
static int numoflines; // number of lines in token
static unsigned char leadinbuf[1048576];      /* no bound check is made */
static int leadinidx;
static unsigned char interptokenbuf[1048576]; /* no bound check is made */
static int interptokenidx;
FILE *outfp;
char *outfname;
char *outfbase;
int option_linedirectives;

int mktokenstring(int num, unsigned char *buf, int len);

%}

%pointer
%x STRING
%x COMMENT
%x INCLUDE
%x LITERAL

%%

[ \t]			{ mkleadin(); }
\r\n			{ mkleadin(); ++line; }
[\n\r]			{ mkleadin(); ++line; }

\/\/[^\n]*\n		{ mkleadin(); ++line; }
\/\*			{ mkleadin(); BEGIN(COMMENT); }
<COMMENT>[^*\n]*	{ mkleadin(); }
<COMMENT>"*"+[^*/\n]*	{ mkleadin(); }
<COMMENT>\n		{ mkleadin(); ++line; }
<COMMENT>\*+\/		{ mkleadin(); BEGIN(0); }

\"			{ interptokenidx = 0; BEGIN(STRING); }
<STRING>\"		{ BEGIN(0);
			  return mktokenstring(T_STRING,interptokenbuf,interptokenidx);
			}
<STRING>\n		{ yyerror("unterminated string context"); }
<STRING>\\[0-7]{1,3}	{ int result;
			  (void)sscanf(yytext+1,"%o",&result);
			  if(result > 0xff)
			    yyerror("character constant is out-of-bounds");
			  interptokenbuf[interptokenidx++] = result;
			}
<STRING>\\[0-9]+	{ yyerror("bad escape sequence"); }
<STRING>\\n		{ interptokenbuf[interptokenidx++] = '\n'; }
<STRING>\\t		{ interptokenbuf[interptokenidx++] = '\t'; }
<STRING>\\r		{ interptokenbuf[interptokenidx++] = '\r'; }
<STRING>\\b		{ interptokenbuf[interptokenidx++] = '\b'; }
<STRING>\\f		{ interptokenbuf[interptokenidx++] = '\f'; }
<STRING>\\(.|\n)	{ interptokenbuf[interptokenidx++] = yytext[1]; }
<STRING>[^\\\n\"]+	{ strncpy((char *)&interptokenbuf[interptokenidx],yytext,yyleng);
			  interptokenidx += yyleng;
			}

\`			{ interptokenidx = 0;  numoflines = 0;  BEGIN(LITERAL); }
<LITERAL>\`		{ BEGIN(0);
			  mktokenstring(T_LITERAL,interptokenbuf,interptokenidx);
			  line += numoflines;
			  return T_LITERAL;
			}
<LITERAL>\n		{ strcpy((char *)&interptokenbuf[interptokenidx],"\n");
			  interptokenidx += 1;
			  ++numoflines;
			}
<LITERAL>[^\`\n]+	{ strncpy((char *)&interptokenbuf[interptokenidx],yytext,yyleng);
			  interptokenidx += yyleng;
			}

\0			{ return EOF; }
\(			{ return mktoken(T_LPAR); }
\)			{ return mktoken(T_RPAR); }
\{			{ return mktoken(T_LCUR); }
\}			{ return mktoken(T_RCUR); }
\[			{ return mktoken(T_LBRA); }
\]			{ return mktoken(T_RBRA); }
::=			{ return mktoken(T_DEFINE); }
::			{ return mktoken(T_DCOLON); }
:=			{ return mktoken(T_ASSIGN); }
:			{ return mktoken(T_COLON); }
-=			{ return mktoken(T_ISMINUS); }
\+=			{ return mktoken(T_ISPLUS); }
\*=			{ return mktoken(T_ISMULT); }
\/=			{ return mktoken(T_ISDIV); }
\%=			{ return mktoken(T_ISMOD); }
\>=			{ return mktoken(T_GE); }
\<=			{ return mktoken(T_LE); }
\>			{ return mktoken(T_GT); }
\<			{ return mktoken(T_LT); }
==			{ return mktoken(T_EQ); }
!=			{ return mktoken(T_NE); }
=			{ return mktoken(T_IS); }
!			{ return mktoken(T_EXCL); }
\|\|			{ return mktoken(T_OR); }
\|			{ return mktoken(T_BAR); }
&&			{ return mktoken(T_AND); }
&			{ return mktoken(T_AMP); }
-\>			{ return mktoken(T_REF); }
~			{ return mktoken(T_TILDE); }
;			{ return mktoken(T_SCOLON); }
\+\+			{ return mktoken(T_INCR); }
\-\-			{ return mktoken(T_DECR); }
-			{ return mktoken(T_MINUS); }
\+			{ return mktoken(T_PLUS); }
\*\*			{ return mktoken(T_POW); }
\*			{ return mktoken(T_STAR); }
\/			{ return mktoken(T_DIV); }
,			{ return mktoken(T_COMMA); }
\.			{ return mktoken(T_DOT); }
\?			{ return mktoken(T_QUEST); }

^\#include\ +\"		{ leadinidx = 0; BEGIN(INCLUDE); }
^\#include\ +<		{ leadinidx = 0; BEGIN(INCLUDE); }
include\ *\"		{ leadinidx = 0; BEGIN(INCLUDE); }
<INCLUDE>\">		{ leadinidx = 0; BEGIN(0); }
<INCLUDE>[^ \t\n\">]+	{ struct stack *next;
			  CHECK(!(next = (struct stack *)malloc(sizeof(struct stack))));
			  next->line   = line;
			  next->file   = file;
			  next->buffer = YY_CURRENT_BUFFER;
			  next->prev   = top;
			  top = next;
			  line = 0;
			  file = strndup(yytext, yyleng);
			  if(!(yyin = fopen(file, "r")))
			    yyerror("Cannot open input file");
			  yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
			  BEGIN(0);
			}

bool			{ return mktoken(T__BOOL);     }
boolean			{ return mktoken(T__BOOLEAN);  }
byte			{ return mktoken(T__BYTE);     }
char			{ return mktoken(T__CHAR);     }
double			{ return mktoken(T__DOUBLE);   }
do			{ return mktoken(T__DO);       }
elif			{ return mktoken(T__ELIF);     }
else			{ return mktoken(T__ELSE);     }
elsif			{ return mktoken(T__ELSIF);    }
entity			{ return mktoken(T__ENTITY);   }
false			{ return mktoken(T__FALSE);    }
float			{ return mktoken(T__FLOAT);    }
for			{ return mktoken(T__FOR);      }
if			{ return mktoken(T__IF);       }
int			{ return mktoken(T__INT);      }
integer			{ return mktoken(T__INTEGER);  }
long			{ return mktoken(T__LONG);     }
main			{ return mktoken(T__MAIN);     }
return			{ return mktoken(T__RETURN);   }
short			{ return mktoken(T__SHORT);    }
signal			{ return mktoken(T__SIGNAL);   }
signed			{ return mktoken(T__SIGNED);   }
string			{ return mktoken(T__STRING);   }
struct			{ return mktoken(T__STRUCT);   }
threaded		{ return mktoken(T__THREADED); }
true			{ return mktoken(T__TRUE);     }
typename		{ return mktoken(T__TYPENAME); }
unsigned		{ return mktoken(T__UNSIGNED); }
vars			{ return mktoken(T__VARS);     }
void			{ return mktoken(T__VOID);     }
while			{ return mktoken(T__WHILE);    }
RECV			{ return mktoken(T__RECV);     }
REPLY			{ return mktoken(T__REPLY);    }
SEND			{ return mktoken(T__SEND);     }
TIME			{ return mktoken(T__TIME);     }
WAIT			{ return mktoken(T__WAIT);     }

(([0-9]+\.[0-9]*([eE][0-9]+)?)|([0-9]+[eE][0-9]+)|(\.[0-9]+([eE][0-9]+)?)) {
			  return mktoken(T_NUMBER);
			}

[0-9]+			{ return mktoken(T_INTEGER);
			}
[A-Za-z_][A-Za-z0-9_]*	{ return mktoken(T_IDENT);
			}

<*><<EOF>>		{ struct stack *prev;
			  if(top) {
			    prev = top->prev;
			    free(file);
			    file = prev->file;
			    line = prev->line;
			    top  = prev->prev;
			    yy_delete_buffer(YY_CURRENT_BUFFER);
			    yy_switch_to_buffer(prev->buffer);
			    free(prev);
			  } else
			    yyterminate();
			}
<*>.			{ yyerror("unexpected input"); return(T_ERROR); }

%%

/****************************************************************************/

void
mkleadin()
{
  strncpy(&leadinbuf[leadinidx], yytext, yyleng);
  leadinidx += yyleng;
}

struct tok
mktoken0()
{
  struct tok result;
  result.leadin        = NULL;
  result.location.file = NULL;
  result.location.line = -1;
  result.contents      = strdup("");
  return result;
}

int
mktoken(int num)
{
  yylval.tok.leadin        = strndup(leadinbuf, leadinidx);
  yylval.tok.location.file = file;
  yylval.tok.location.line = line;
  yylval.tok.contents      = strndup(yytext, yyleng);
  leadinidx = 0;
  return num;
}

int
mktokenstring(int num, unsigned char *buf, int len)
{
  yylval.tok.leadin        = strndup(leadinbuf, leadinidx);
  yylval.tok.location.file = file;
  yylval.tok.location.line = line;
  yylval.tok.contents      = strndup((char *)buf, len);
  leadinidx = 0;
  return num;
}

int
yyerror(char *msg,...)
{
  va_list ap;
  va_start(ap, msg);
  fprintf(stderr,"%s: ", argv0);
  vfprintf(stderr, msg, ap);
  fprintf(stderr,"\n");
  if(yyin)
    fprintf(stderr,"In %s at line %d while reading:\n%s\n",file,line+1,yytext);
  va_end(ap);
  abort();
}

static int innfiles; /* number of input files */
static char **infnames; /* input file names */

int
yywrap()
{
  if(innfiles > 0) {
    if(yyin != NULL)
      fclose(yyin);
    file = *infnames;
    line = 0;
    ++infnames;
    --innfiles;
    CHECK(!(yyin = fopen(file, "r")));
    return 0;
  } else {
    yyin = NULL;
    return 1;
  }
}

int
main(int argc, char *argv[], char **envp)
{
  int ch;
#ifdef HAVE_GETOPT_LONG
  static struct option options[] = {
    { "output",   required_argument, NULL, 'o' },
    { "no-lines", no_argument,       NULL, 'L' },
    { NULL, 0, NULL, '\0' }
  };
#endif

  /* Get the name of the program without path */
  if((argv0 = strrchr(argv[0],'/')) == NULL)
    argv0 = argv[0];
  else
    ++argv0;

  outfp    = NULL;
  outfname = NULL;
  outfbase = NULL;
  option_linedirectives = 1;
  /* Parse options to program and get arguments */
  opterr = 0;
#ifdef HAVE_GETOPT_LONG
  while((ch = getopt_long(argc, argv, "o:L", options, NULL)) >= 0) {
#else
  while((ch = getopt(argc, argv, "o:L")) >= 0) {
#endif
    switch(ch) {
    case 'o':
      if(strlen(optarg)>3 && !strcmp(&optarg[strlen(optarg)-4],".cpp")) {
	CHECK(!(outfname = strdup(optarg)));
	CHECK(!(outfbase = malloc(strlen(optarg)+1-4)));
	strncpy(outfbase, optarg, strlen(optarg)+1-4);
	outfbase[strlen(optarg)-4-1] = '\0';
      } else if(strlen(optarg)>3 && !strcmp(&optarg[strlen(optarg)-3],".cc")) {
	CHECK(!(outfname = strdup(optarg)));
	CHECK(!(outfbase = malloc(strlen(optarg)+1-3)));
	strncpy(outfbase, optarg, strlen(optarg)+1-3);
	outfbase[strlen(optarg)-3-1] = '\0';
      } else {
	CHECK(!(outfbase = strdup(optarg)));
	CHECK(!(outfname = malloc(strlen(optarg)+1+3)));
	sprintf(outfname,"%s.cc",optarg);
      }
      if(!(outfp = fopen(outfname, "w")))
	fprintf(stderr,"%s: cannot open output file %s\n",argv0,outfname);
      break;
    case 'L':
      option_linedirectives = 0;
      break;
    case '?':
      fprintf(stderr,"%s: invalid option %c\n",argv0,optopt);
      abort();
    }
  }
  if(outfp == NULL)
    outfp = stdout;

  innfiles = argc - optind;
  infnames = &argv[optind];

  yyin = NULL;
  if(!yywrap())
    if(yyparse())
      yyerror("error in input");

  if(outfp != stdout)
    fclose(outfp);

  exit(0);
}
