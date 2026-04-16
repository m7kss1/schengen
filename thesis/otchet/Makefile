SHELL := /bin/bash
.ONESHELL:

MAIN := main.tex
BASE := $(basename $(MAIN))
PDF := $(BASE).pdf

.PHONY: otchet clean

otchet:
	set -euo pipefail
	command -v pdflatex >/dev/null
	command -v biber >/dev/null
	pdflatex --interaction=nonstopmode "$(MAIN)" || true
	test -f "$(BASE).bcf"
	biber "$(BASE)"
	pdflatex --interaction=nonstopmode "$(MAIN)" || true
	pdflatex --interaction=nonstopmode "$(MAIN)" || true
	test -f "$(PDF)"

clean:
	set -euo pipefail
	rm -f "$(BASE).aux" \
	      "$(BASE).bcf" \
	      "$(BASE).bbl" \
	      "$(BASE).blg" \
	      "$(BASE).fdb_latexmk" \
	      "$(BASE).fls" \
	      "$(BASE).log" \
	      "$(BASE).out" \
	      "$(BASE).pdf" \
	      "$(BASE).run.xml" \
	      "$(BASE).toc"
