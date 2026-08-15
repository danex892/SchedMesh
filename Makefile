.PHONY: install-dev lint format

PYTHON ?= python

install-dev:
	$(PYTHON) tools/install_dev.py

lint:
	$(PYTHON) tools/lint.py

format:
	$(PYTHON) tools/format.py
