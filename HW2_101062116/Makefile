CC	= cc
CFLAGS	= -g -O0 -Wall -Wextra -pedantic -pthread

all: HW2_101062116_Cli HW2_101062116_Ser

HW2_101062116_Cli: HW2_101062116_Cli.c
	$(CC) $(CFLAGS) HW2_101062116_Cli.c -o HW2_101062116_Cli

HW2_101062116_Ser: HW2_101062116_Ser.c
	$(CC) $(CFLAGS) HW2_101062116_Ser.c -o HW2_101062116_Ser

clean:
	-rm -rf HW2_101062116_Cli HW2_101062116_Ser
