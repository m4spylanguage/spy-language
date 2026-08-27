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

#include "spy/platforms/PlatformWindows.h"

namespace spy {

std::string PlatformWindows::get_system_includes() {
    return "#include <stdio.h>\n"
           "#include <stdlib.h>\n"
           "#include <string.h>\n"
           "#include <math.h>\n"
           "#include <stdint.h>\n"
           "#include <time.h>\n"
           "#include <sys/stat.h>\n"
           "#include <windows.h>\n"
           "#include <io.h>\n"
           "#include <process.h>\n";
}

std::string PlatformWindows::get_default_cc() {
    return "cl.exe";
}

std::string PlatformWindows::get_linker_flags() {
    return "user32.lib gdi32.lib";
}

} // namespace spy
