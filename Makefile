PACKAGE = cartosphere
VERSION = 0.0.1

TARNAME = $(PACKAGE)
DISTDIR = $(TARNAME)-$(VERSION)

export prefix=/usr/local

export ROOT_DIR=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

# FFTW 3
export FFTWINC=-I/usr/local/opt/fftw/include
export FFTWLIB=-L/usr/local/opt/fftw/lib

# Eigen 3
export EIGENINC=-I/usr/local/include/eigen3

# glog
export LOCALIB=-L/usr/local/lib

all clean uninstall cartosphere:
	$(MAKE) -C src $@
.PHONY: all clean uninstall

install: all
	$(MAKE) -C src $@
.PHONY: install

dist: $(DISTDIR).tar.gz
.PHONY: dist

distcheck: $(DISTDIR).tar.gz
	gzip -cd $+ | tar xvf -
	$(MAKE) -C $(DISTDIR) all check
	$(MAKE) -C $(DISTDIR) prefix=\
	$$(PWD)/$(DISTDIR)/_inst install uninstall
	$(MAKE) -C $(DISTDIR) clean
	rm -rf $(DISTDIR)
	@echo "*** Package $(DISTDIR).tar.gz ready for distribution ***"
.PHONY: distcheck

$(DISTDIR).tar.gz: FORCE $(DISTDIR)
	tar chof - $(DISTDIR) |\
		gzip -9 -c >$(DISTDIR).tar.gz
	rm -rf $(DISTDIR)

$(DISTDIR):
	mkdir -p $(DISTDIR)/src
	cp Makefile $(DISTDIR)
	cp src/Makefile $(DISTDIR)/src
	cp src/main.c $(DISTDIR)/src

FORCE:
	-rm $(DISTDIR).tar.gz &> /dev/null
	-rm -rf $(DISTDIR) &> /dev/null
