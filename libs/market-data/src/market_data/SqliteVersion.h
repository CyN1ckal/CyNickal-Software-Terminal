#pragma once

namespace terminal {

// Thin wrap of sqlite3_libversion_number so tests/GUI never include sqlite3.h.
[[nodiscard]] int sqliteLibVersionNumber();

}  // namespace terminal
