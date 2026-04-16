SHELL := /bin/bash
.ONESHELL:

MAIN := tz.tex
PDF := tz.pdf
BASE := $(basename $(MAIN))

.PHONY: sow clean

sow:
	set -euo pipefail
	command -v pdflatex >/dev/null
	if [ ! -f ../TechDoc.cls ] && [ -f ./TechDoc.cls ]; then
		ln -sf sow/TechDoc.cls ../TechDoc.cls
	fi
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
