#include "market_data/SqliteVersion.h"

#include "sqlite3.h"

namespace myapp {

int sqliteLibVersionNumber()
{
    return sqlite3_libversion_number();
}

}  // namespace myapp
