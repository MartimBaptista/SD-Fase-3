main: tree-client tree-server

cliente-lib.o:
	(cd Client && make cliente-lib.o)

tree-client:
	(cd Client && make tree-client)

tree-server:
	(cd Server && make tree-server)

clean:
	(cd Server && make clean)
	(cd Client && make clean)	