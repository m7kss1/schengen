SHELL := /bin/bash
.ONESHELL:

MAIN := manual.tex
PDF := manual.pdf
BASE := $(basename $(MAIN))

.PHONY: opmanual sow clean

opmanual:
	set -euo pipefail
	command -v pdflatex >/dev/null
	if [ ! -f ../TechDoc.cls ] && [ -f ./TechDoc.cls ]; then
		ln -sf opmanual/TechDoc.cls ../TechDoc.cls
	fi
	pdflatex --interaction=nonstopmode "$(MAIN)"
	pdflatex --interaction=nonstopmode "$(MAIN)"
	pdflatex --interaction=nonstopmode "$(MAIN)"
	test -f "$(PDF)"

sow: opmanual

clean:
	set -euo pipefail
	rm -f "$(BASE).aux" \
	      "$(BASE).fdb_latexmk" \
	      "$(BASE).fls" \
	      "$(BASE).log" \
	      "$(BASE).out" \
	      "$(BASE).pdf" \
	      "$(BASE).toc" \
	      "tz.aux" \
	      "tz.fdb_latexmk" \
	      "tz.fls" \
	      "tz.log" \
	      "tz.out" \
	      "tz.pdf" \
	      "tz.toc"
