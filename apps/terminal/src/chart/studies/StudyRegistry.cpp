// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/StudyRegistry.h"

#include "registry/CallbackRegistry.h"

namespace terminal {
namespace {

CallbackRegistry<StudyType>& registry() noexcept
{
    static CallbackRegistry<StudyType> types;
    return types;
}

}  // namespace

void registerStudy(const StudyType& type)
{
    registry().add(type);
}

const StudyType* findStudy(std::string_view id) noexcept
{
    return registry().find(id);
}

std::span<const StudyType* const> studyTypes() noexcept
{
    return registry().all();
}

}  // namespace terminal
