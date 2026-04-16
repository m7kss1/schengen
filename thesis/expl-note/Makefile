SHELL := /bin/bash
.ONESHELL:

MAIN := expl-note.tex
PDF := expl-note.pdf
BASE := $(basename $(MAIN))
IMAGE_DIR := image
FIG_TEX := $(IMAGE_DIR)/textpool-diagram.tex $(IMAGE_DIR)/serialization-diagram.tex $(IMAGE_DIR)/duckdb-integration-diagram.tex
FIG_PDF := $(FIG_TEX:.tex=.pdf)

.PHONY: expl-note clean

expl-note: $(FIG_PDF)
	set -euo pipefail
	command -v pdflatex >/dev/null
	pdflatex --interaction=nonstopmode "$(MAIN)"
	pdflatex --interaction=nonstopmode "$(MAIN)"
	pdflatex --interaction=nonstopmode "$(MAIN)"
	test -f "$(PDF)"

$(IMAGE_DIR)/%.pdf: $(IMAGE_DIR)/%.tex
	set -euo pipefail
	cd "$(IMAGE_DIR)"
	pdflatex --interaction=nonstopmode "$(*F).tex"
	pdflatex --interaction=nonstopmode "$(*F).tex"
	test -f "$(*F).pdf"

clean:
	set -euo pipefail
	rm -f "$(BASE).aux" \
	      "$(BASE).fdb_latexmk" \
	      "$(BASE).fls" \
	      "$(BASE).log" \
	      "$(BASE).out" \
	      "$(BASE).pdf" \
	      "$(BASE).toc" \
	      "$(IMAGE_DIR)"/*.aux \
	      "$(IMAGE_DIR)"/*.log \
	      "$(IMAGE_DIR)"/*.pdf
