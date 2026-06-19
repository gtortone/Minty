#pragma once

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define VERSION_MAJOR 1
#define VERSION_MINOR 3
#define VERSION STR(VERSION_MAJOR) "." STR(VERSION_MINOR)

