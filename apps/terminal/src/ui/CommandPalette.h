// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "ui/CommandMatch.h"
#include "ui/Commands.h"

#include <string>
#include <vector>

namespace terminal {

// Ctrl+K. Filters every command by typing; Up/Down pick, Enter runs, Esc closes.
class CommandPalette
{
public:
    void open();
    // False while closed, so the shell does not build the command list every frame.
    [[nodiscard]] bool wantsCommands() const noexcept { return open_request_ || visible_; }
    // commands must outlive the call; the chosen one runs after the popup closes.
    void draw(const CommandList& commands);

private:
    struct Match
    {
        int index{0};
        int score{0};
    };

    void rank(const CommandList& commands);
    void remember(const std::string& key);

    bool open_request_{false};
    bool visible_{false};
    bool focus_input_{false};
    bool rerank_{true};
    char query_[128]{};
    std::string ranked_query_;
    std::size_t ranked_count_{0};
    std::vector<Match> matches_;
    int selected_{0};
    bool scroll_to_selected_{false};
    // Most recent first, by "group/label". In memory only.
    std::vector<std::string> recent_;
};

}  // namespace terminal
