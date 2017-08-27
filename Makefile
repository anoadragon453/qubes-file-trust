CXXFLAGS = -pthread -Werror -std=c++11 -g -O
INSTALL = install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = ${INSTALL} -m 0644
PYTHON = python3

compile:
	$(CXX) $(CXXFLAGS) -o qubes-trust-daemon qubes-trust-daemon.cpp

install:
	$(INSTALL_PROGRAM) qvm-open-trust-based $(DESTDIR)/usr/bin/qvm-open-trust-based
	$(PYTHON) setup.py install --root /$(DESTDIR) --force $(EX_SETUP_OPTS)

clean:
	rm -f qubes-trust-daemon 
