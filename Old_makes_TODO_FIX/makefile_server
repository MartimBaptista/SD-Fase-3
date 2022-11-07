OBJ = tree_server.o entry.o data.o tree.o tree_skel.o network_server.o sdmessage.pb-c.o

tree-server: required_dirs $(OBJ) 
	gcc $(addprefix object/,$(OBJ)) -lprotobuf-c -o binary/tree-server

tree.o:
	cp extra_obj/tree.o.extra object/tree.o

%.o: source/%.c
	gcc $< -c -I include -o object/$@ -g -Wall

required_dirs:
	mkdir -p object
	mkdir -p binary

clean:
	rm -f object/* binary/*