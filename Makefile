.PHONY: install-dev lint format example

PYTHON ?= python

install-dev:
	$(PYTHON) tools/install_dev.py

lint:
	$(PYTHON) tools/lint.py

format:
	$(PYTHON) tools/format.py

example:
	cmake --build .tools/build --config Release
	$(PYTHON) tools/legacy_example.py
