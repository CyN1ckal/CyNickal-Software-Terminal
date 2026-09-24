// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Schema.h"

#include "schema_v4.inc"
#include "schema_v5.inc"

namespace terminal {

std::string_view schemaV4()
{
    return kSchemaV4;
}

std::string_view schemaV5()
{
    return kSchemaV5;
}

}  // namespace terminal
