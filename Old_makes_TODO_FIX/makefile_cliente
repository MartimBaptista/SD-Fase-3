OBJ = entry.o data.o client_stub.o network_client.o sdmessage.pb-c.o

tree-client: required_dirs cliente-lib.o tree_client.o
	gcc object/cliente-lib.o object/tree_client.o -lprotobuf-c -o binary/tree-client

cliente-lib.o: required_dirs $(OBJ)
	ld -r $(addprefix object/,$(OBJ)) -o object/cliente-lib.o

%.o: source/%.c
	gcc $< -c -I include -o object/$@ -g -Wall

required_dirs:
	mkdir -p object
	mkdir -p binary

clean:
	rm -f object/* binary/*
