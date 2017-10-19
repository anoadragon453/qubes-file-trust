CXXFLAGS = -pthread -Werror -std=c++11 -g -O
EX_SETUP_OPTS?= --install-layout=deb
INSTALL_OPTS = -D
INSTALL = install $(INSTALL_OPTS)
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = ${INSTALL} -m 0644
PYTHON = python3

PANDOC=pandoc -s -f rst -t man
DOCS=$(patsubst %.rst,%.1.gz,$(wildcard *.rst))

build:
	$(CXX) $(CXXFLAGS) -o qubes-trust-daemon qubes-trust-daemon.cpp
	$(MAKE) -C doc manpages

install:
	$(INSTALL_PROGRAM) qvm-open-trust-based $(DESTDIR)/usr/bin/qvm-open-trust-based
	$(INSTALL_PROGRAM) qubes-trust-daemon $(DESTDIR)/usr/bin/qubes-trust-daemon
	$(PYTHON) setup.py install --root $(DESTDIR)/ --force $(EX_SETUP_OPTS)

	$(MAKE) -C doc install

tests:
	$(MAKE) -C qubesfiletrust -B tests PYTHON=$(PYTHON)

%.1: %.rst
	$(PANDOC) $< > $@

%.1.gz: %.1
	gzip -f $<

clean:
	rm -f qubes-trust-daemon 
