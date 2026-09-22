// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Schema.h"

#include "schema_v1.inc"
#include "schema_v2.inc"

namespace terminal {

std::string_view schemaV1()
{
    return kSchemaV1;
}

std::string_view schemaV2()
{
    return kSchemaV2;
}

}  // namespace terminal
