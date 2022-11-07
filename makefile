OBJ_C = entry.o data.o client_stub.o network_client.o sdmessage.pb-c.o
OBJ_S = tree_server.o entry.o data.o tree.o tree_skel.o network_server.o sdmessage.pb-c.o

main: tree-client tree-server

cliente-lib.o:	required_dirs $(OBJ_C)
	ld -r $(addprefix object/,$(OBJ_C)) -o object/cliente-lib.o

tree-client: required_dirs cliente-lib.o tree_client.o
	gcc object/cliente-lib.o object/tree_client.o -lprotobuf-c -o binary/tree-client

tree-server: required_dirs $(OBJ_S) 
	gcc $(addprefix object/,$(OBJ_S)) -lprotobuf-c -o binary/tree-server

tree.o:
	cp extra_obj/tree.o.extra object/tree.o

%.o: source/%.c
	gcc $< -c -I include -o object/$@ -g -Wall

required_dirs:
	mkdir -p object
	mkdir -p binary

clean:
	rm -f object/* binary/*