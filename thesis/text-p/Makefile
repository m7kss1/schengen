SHELL := /bin/bash
.ONESHELL:

MAIN := text-p.tex
PDF := text-p.pdf
BASE := $(basename $(MAIN))

.PHONY: text-p clean

text-p:
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
