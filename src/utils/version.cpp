#include "version.hpp"

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "799ee98"
#endif

namespace ChessEngine {
std::string get_version_string() {
    return std::string("0.1.1-") + GIT_COMMIT_HASH;
}
}
