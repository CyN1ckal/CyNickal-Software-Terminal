// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace terminal {

// Fixed table of process callbacks. The first registration of an id wins.
// A null id, a null process, or a full table is ignored.
// Type must have a const char* id and a process pointer.
template <typename Type>
struct CallbackRegistry
{
    static constexpr int kCapacity = 32;
    const Type* types[kCapacity]{};
    int count{0};

    void add(const Type& type) noexcept
    {
        if (type.id == nullptr || type.process == nullptr || count >= kCapacity)
        {
            return;
        }
        for (int index = 0; index < count; ++index)
        {
            const Type* existing = types[static_cast<std::size_t>(index)];
            if (existing != nullptr && existing->id != nullptr &&
                type.id == std::string_view{existing->id})
            {
                return;
            }
        }
        types[static_cast<std::size_t>(count)] = &type;
        ++count;
    }

    [[nodiscard]] const Type* find(std::string_view id) const noexcept
    {
        for (int index = 0; index < count; ++index)
        {
            const Type* type = types[static_cast<std::size_t>(index)];
            if (type != nullptr && type->id != nullptr && id == type->id)
            {
                return type;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::span<const Type* const> all() const noexcept
    {
        return {types, static_cast<std::size_t>(count)};
    }
};

}  // namespace terminal
