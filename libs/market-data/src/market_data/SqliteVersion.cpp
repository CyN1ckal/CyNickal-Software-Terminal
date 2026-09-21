#include "market_data/SqliteVersion.h"

#include "sqlite3.h"

namespace terminal {

int sqliteLibVersionNumber()
{
    return sqlite3_libversion_number();
}

}  // namespace terminal
