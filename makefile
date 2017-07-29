FILENAME = isSame.c error.c

OUTPUT = isSame

main: ${FILENAME}
	gcc -std=c99 -Wall -Werror -Wpedantic ${FILENAME} -o ${OUTPUT}
debug:
	gcc -std=c99 -Wall -Werror -Wpedantic ${FILENAME} -o ${OUTPUT} -DDEBUG
clean:
	rm -vv isSame
