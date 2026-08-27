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

#include <stdio.h>
struct Vec { double x; double y; };
struct Vec add(struct Vec* self, struct Vec* other) {
    return (struct Vec){(self->x + other->x), (self->y + other->y)};
}
int main() {
    struct Vec a = {1.0, 2.0};
    struct Vec b = {3.0, 4.0};
    struct Vec c = add(&a, &b);
    printf("c.x=%g c.y=%g\n", c.x, c.y);
    return 0;
}
