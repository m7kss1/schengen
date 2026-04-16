SHELL := /bin/bash
.ONESHELL:

MAIN := pmi.tex
PDF := pmi.pdf
BASE := $(basename $(MAIN))

.PHONY: pmi clean

pmi:
	set -euo pipefail
	command -v pdflatex >/dev/null
	pdflatex --interaction=nonstopmode "$(MAIN)"
	pdflatex --interaction=nonstopmode "$(MAIN)"
	pdflatex --interaction=nonstopmode "$(MAIN)"
	test -f "$(PDF)"

clean:
	set -euo pipefail
	rm -f "$(BASE).aux" \
	      "$(BASE).fdb_latexmk" \
	      "$(BASE).fls" \
	      "$(BASE).log" \
	      "$(BASE).out" \
	      "$(BASE).pdf" \
	      "$(BASE).toc"
