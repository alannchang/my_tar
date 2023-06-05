CFLAGS = -Wall -Wextra -Werror
TARGET = my_tar
OBJ = my_tar.o
SRC = my_tar.c
all : $(TARGET)

$(TARGET) : $(OBJ)
	gcc $(CFLAGS) -o $(TARGET) $(OBJ) 

$(OBJ) : $(SRC)
	gcc $(CFLAGS) -c $(SRC)

clean:
	rm -f *.o

fclean: clean
	rm -f $(TARGET)

re: fclean all