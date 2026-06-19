// Single translation unit that compiles the tinyexr implementation. Uses the bundled miniz (the
// default backend) for the DEFLATE codec, so no external zlib is required.
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
