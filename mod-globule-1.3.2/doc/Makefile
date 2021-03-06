.PHONY:		all clean distclean cleanest install

SUFFIXES:=      $(SUFFIXES) .html .pdf .ps .eps .ind .idx .bbl .bib .fig .dvi
SUFFIXES:=      $(SUFFIXES) .tex .gif .png .jpg
.SUFFIXES:      $(SUFFIXES)

LATEX=		latex
BIBTEX=		bibtex
MAKEINDEX=	makeindex
MAKEGLOSSARY=	makeglossary
DVIPSFLAGS=	-t a4

.tex.bbl:
	$(BIBTEX) $*
.tex.dvi:
	if grep >/dev/null '^[^%]*bibliography[^s]' $< ; then \
	  if [ ! -f $*.aux ] ; then \
	    $(LATEX) $< ; $(MAKE) $*.bbl ; fi ; fi
	$(LATEX) $< ; while `grep > /dev/null "Rerun" $*.log` ; do $(LATEX) $< ; done
	if `grep > /dev/null "No file .*\.gls" $*.log` ; then \
	  $(MAKEGLOSSARY) $* ; $(LATEX) $* ; fi
	if `grep > /dev/null "No file .*\.ind" $*.log` ; then \
	  $(MAKEINDEX) $*.idx ; $(LATEX) $< ; $(LATEX) $< ; fi
# Don't know why but after redoing the index we have to run LaTeX twice
.dvi.ps:
	dvips $(DVIPSFLAGS) -o $@ $<
.ps.pdf:
	ps2pdf $< $@
.fig.eps:
	fig2dev -L ps $*.fig $*.eps
.fig.jpg:
	fig2dev -L jpeg $*.fig $*.jpg
.gif.eps:
	giftoppm < $< | ppmtopgm | pnmtops -noturn -dpi 600 > $@
.jpg.eps:
	convert $< $@
.png.eps:
	convert $< $@

INPUTS:=$(wildcard *.tex)
FIGURES:=$(FIGURES) model.eps glowin1.eps glowin2.eps glowin3.eps glowin4.eps
FIGURES:=$(FIGURES) glowin5.eps glowin6.eps glowin7.eps glowin8.eps
FIGURES:=$(FIGURES) xyz-ab-v0.eps
IMAGES:=$(IMAGES) model.jpg xyz-ab-v0.jpg

.SECONDARY: $(FIGURES) doc.dvi

#
# Full re-build requires Hevea from:
#  http://pauillac.inria.fr/~maranget/hevea/
#

all:		doc.pdf doc.html

doc.dvi:	$(INPUTS) $(FIGURES)
doc.html:	doc.dvi $(IMAGES)
	hevea -exec xxdate.exe -fix -o $*.html $*.tex 2>&1 | tee $*.hlog ; while `grep > /dev/null "Run, run, again" $*.hlog` ; do hevea -exec xxdate.exe -fix -o $*.html $*.tex 2>&1 | tee $*.hlog ; done
	# apacheversion=`sed < doc.tex -e 's/\\newcommand{\\apacheversion}{\(.*\)}/\1/p' -e 'd'` ; globuleversion=`sed < doc.tex -e 's/\\newcommand{\\globuleversion}{\(.*\)}/\1/p' -e 'd'` ; echo "A: $$apacheversion" ; echo "G: $$globuleversion" ; sed -e "s/\\\\apacheversion/$$apacheversion/g" -e "s/\\\\globuleversion/$GLOBULEVERSION/g" < doc.html > doc.html.tmp ; mv doc.html.tmp doc.html
	hacha $@
	globuleversion="`sed < doc.tex -e 's/^\\\\newcommand{\\\\globuleversion}{\\([^}]*\\)}$$/\\1/p' -e d`" ; apacheversion="`sed < doc.tex -e 's/^\\\\newcommand{\\\\apacheversion}{\\([^}]*\\)}$$/\\1/p' -e d`" ; for f in *.html ; do sed < $$f -e 's/\\\([_{}]\)/\1/g' -e 's/\\vdots/.../g' -e 's/\\ldots/.../g' -e 's/\\textbf{\([^\\]*\)}/<b>\1<\/b>/g' -e 's/\\textit{\([^\\]*\)}/<i>\1<\/i>/g' -e 's/\\(\\backslash\\)//g' -e "s/\\\\apacheversion/$$apacheversion/g" -e "s/\\\\globuleversion/$$globuleversion/g" > $$f~ ; mv $$f~ $$f ; done

clean:
		rm -f *.aux *.lot *.toc *.lof *.log *.bbl *.blg *.dvi *.ps
		rm -f *.haux *.htoc *.hlog
		rm -f $(FIGURES) *~
distclean:	clean
cleanest:	clean
		rm -rf $(IMAGES) *.pdf *.html
install:	all
		if [ ! -d /home/berry/globule/apache/htdocs/globule-docs ] ; then mkdir /home/berry/globule/apache/htdocs/globule-docs ; fi
		cp Globule.css *.pdf *.jpg *.gif *.html /home/berry/globule/apache/htdocs/globule-docs
