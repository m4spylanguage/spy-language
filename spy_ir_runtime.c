/*
 * Spy Programming Language Engine & Compiler
 * Copyright (C) 2026 Valuvajjala Vivek Vardhan Rao
 *
 * Author: Valuvajjala Vivek Vardhan Rao
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
const char* spy_substr(const char* s, int start, int len) {
    size_t slen = strlen(s);
    if (start < 0) start = 0;
    if ((size_t)start > slen) start = (int)slen;
    if (len < 0) len = 0;
    if ((size_t)(start + len) > slen) len = (int)(slen - start);
    char* r = (char*)malloc(len + 1);
    memcpy(r, s + start, len);
    r[len] = '\0';
    return r;
}
const char* spy_chr(int n) {
    char* r = (char*)malloc(2);
    r[0] = (char)n;
    r[1] = '\0';
    return r;
}
const char* spy_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    if (len > 0) fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}
const char* spy_strcat(const char* a, const char* b) {
    size_t alen = strlen(a), blen = strlen(b);
    char* r = (char*)malloc(alen + blen + 1);
    memcpy(r, a, alen);
    memcpy(r + alen, b, blen);
    r[alen + blen] = '\0';
    return r;
}
const char* spy_format(const char* fmt, ...) {
    static char bufs[8][4096];
    static int idx = 0;
    char* buf = bufs[idx & 7]; idx++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    return buf;
}
void* spy_alloc(size_t size) { return malloc(size); }
double spy_strlen(const char* s) { return (double)strlen(s); }
const char* spy_str_from_double(double val) {
    char* r = (char*)malloc(64);
    snprintf(r, 64, "%g", val);
    return r;
}
