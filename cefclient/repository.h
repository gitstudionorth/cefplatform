#pragma once

#include <sstream>
#include "cefclient/sqlite3.h"

namespace storage {
    std::string getSessions();
    void insertSession(char *name, char *status);
};

