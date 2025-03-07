SRC = alink_drone.c
OBJ = ./alink_drone

.PHONY: clean adaptive-link
adaptive-link: $(OBJ)

$(OBJ): $(SRC:%.c=%.o)
	$(CC) $^ -rdynamic $(OPT) -o $@

%.o: %.c Makefile
	$(CC) -c $< $(OPT) -o $@

clean:
	rm -rf $(SRC:%.c=%.o) $(OBJ)