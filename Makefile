
version = 0.8.0

DIRS = phxrpc codegen \
	   plugin_elpp/file

all:
	for d in $(DIRS); do \
		cd $$d; make; cd -; \
	done
	@( cd sample; test -f Makefile || ./regen.sh; make )

boost:
	@( cd plugin_boost; make )

dist: clean phxrpc-$(version).src.tar.gz

phxrpc-$(version).src.tar.gz:
	@find . -type f | grep -v CVS | grep -v .svn | grep -v third_party | sed s:^./:phxrpc-$(version)/: > MANIFEST
	@(cd ..; ln -s phxrpc phxrpc-$(version))
	(cd ..; tar cvf - `cat phxrpc/MANIFEST` | gzip > phxrpc/phxrpc-$(version).src.tar.gz)
	@(cd ..; rm phxrpc-$(version))

clean:
	$(RM) lib/*;
	@( cd sample; test -f Makefile && make clean )
	for d in $(DIRS); do \
		cd $$d; make clean; cd -; \
	done
