#pragma once

#include <cstddef>

namespace ezmk::skills {

// 1.5.0-dev.8: user-project skills embedded at build time from
// .claude/skills/ezmk-user-*/. The table is generated into
// src/skill_data.cpp by scripts/embed_skills.py (gitignored).
struct Skill {
    const char* name;
    const char* description;
    const char* content;
};

// Returns the embedded table and writes its size to count. May be null when
// Python was unavailable at build time (count == 0).
const Skill* all(std::size_t& count);

} // namespace ezmk::skills
