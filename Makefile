.PHONY: install-dev lint format example download-testdata benchmark

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

download-testdata:
	$(PYTHON) tools/xhstt_benchmarks.py download

benchmark: download-testdata
	cmake --build .tools/build --config Release
	$(PYTHON) tools/xhstt_benchmarks.py run
