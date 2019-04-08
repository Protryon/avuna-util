/*
 * streams.c
 *
 *  Created on: Nov 17, 2015
 *      Author: root
 */

#include <avuna/streams.h>
#include <stdio.h>
#include <unistd.h>

ssize_t readLine(int fd, char* line, size_t len) {
    if (len >= 1) line[0] = 0;
    char b = 0;
    ssize_t s = 0;
    size_t i = 0;
    do {
        s = read(fd, &b, 1);
        if ((s == 0 && i == 0) || s < 0) {
            return -1;
        }
        if (s == 0) {
            break;
        }
        if (s > 0 && b != 13 && b != 10) {
            line[i++] = b;
        }
    } while (b > -1 && s > 0 && b != 10 && i < len - 1);
    line[i] = 0;
    return i;
}

ssize_t writeLine(int fd, char* line, size_t len) {
    static char nl[2] = {0x0A, 0x0D};
    size_t i = 0;
    while (i < len) {
        ssize_t x = write(fd, line + i, len - i);
        if (x < 0) return -1;
        i += x;
    }
    size_t i2 = 0;
    while (i2 < 2) {
        ssize_t y = write(fd, nl + i2, 2 - i2);
        if (y < 0) return -1;
        i2 += y;
    }
    return i;
}
