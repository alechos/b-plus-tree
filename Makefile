CC      = gcc
CFLAGS  = -Wall -Wextra -O2
INCLUDE = -I ./include/
LDFLAGS = -L ./lib/ -Wl,-rpath,./lib/ -lbf

SRC     = ./src/record.c ./src/bp_file.c ./src/bp_datanode.c ./src/bp_indexnode.c

.PHONY: all test clean

all: test benchmark

test:
	@echo "Compiling test..."
	$(CC) $(CFLAGS) $(INCLUDE) ./tests/test_bp.c $(SRC) $(LDFLAGS) -o ./build/test_bp
	@echo "Running tests..."
	./build/test_bp

clean:
	@echo "Cleaning..."
	rm -f ./build/test_bp data.db
