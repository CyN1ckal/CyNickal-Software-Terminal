// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/StudyRegistry.h"

#include <cstddef>

namespace terminal {
namespace {

struct Registry
{
    static constexpr int kCapacity = 32;
    const StudyType* types[kCapacity]{};
    int count{0};
};

Registry& registry() noexcept
{
    static Registry types;
    return types;
}

}  // namespace

void registerStudy(const StudyType& type) noexcept
{
    Registry& types = registry();
    if (type.id == nullptr || type.process == nullptr || types.count >= Registry::kCapacity)
    {
        return;
    }
    for (int index = 0; index < types.count; ++index)
    {
        const StudyType* existing = types.types[static_cast<std::size_t>(index)];
        if (existing != nullptr && existing->id != nullptr && type.id == std::string_view{existing->id})
        {
            return;
        }
    }
    types.types[static_cast<std::size_t>(types.count)] = &type;
    ++types.count;
}

const StudyType* findStudy(std::string_view id) noexcept
{
    const Registry& types = registry();
    for (int index = 0; index < types.count; ++index)
    {
        const StudyType* type = types.types[static_cast<std::size_t>(index)];
        if (type != nullptr && type->id != nullptr && id == type->id)
        {
            return type;
        }
    }
    return nullptr;
}

std::span<const StudyType* const> studyTypes() noexcept
{
    const Registry& types = registry();
    return {types.types, static_cast<std::size_t>(types.count)};
}

}  // namespace terminal
