all:	secse-main 

secse-main: secse-main.pdf

main:	main.pdf

TEX-FILES =	*.tex
DIAGRAMS=making.pdf software_lifecycle.pdf

secse-main.pdf: ${TEX-FILES} *.bib $(DIAGRAMS)
	pdflatex secse-main
	-bibtex secse-main
	pdflatex secse-main
	pdflatex secse-main

secse-main.rtf: ${TEX-FILES} *.bib $(DIAGRAMS)
	latex2rtf secse-main


main.pdf: ${TEX-FILES} *.bib $(DIAGRAMS)
	pdflatex main
	-bibtex main
	pdflatex main
	pdflatex main
	pdflatex main

clean:
	rm -f *.aux *.bbl *.blg *.ent *.log *.dvi *.bak *~ main.ps main.pdf secse-main.pdf

