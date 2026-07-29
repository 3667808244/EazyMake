---
name: ezmk-planning
description: How to work with EazyMake version plans — plans/ directory structure, plan document format, and updating execution status.
trigger:
  - glob: plans/**/*.md
  - glob: plan.md
---

# EazyMake Planning

## Directory structure

```
plans/
├── README.md              # Version index + roadmap + dependency graph
├── dev/                   # Early development version plans (0.1.6 ~ 0.2.6)
│   ├── 0.1.6.md
│   ├── 0.1.7.md
│   └── ...
├── release/               # Release version plans (0.9.0 ~ 1.1.0)
│   ├── 0.9.0.md
│   ├── 1.0.0.md
│   ├── 1.1.0-dev.1.md     # 1.1.0 sub-versions (dev.1 ~ dev.6)
│   ├── 1.1.0-dev.2.md
│   └── ...
└── (root)
    └── plan.md             # Current execution plan — mirrors the active version's design doc
```

- **`dev/`** — plans for early development versions (0.1.x ~ 0.2.x). All completed.
- **`release/`** — plans for release versions (0.9.0+) and current dev sub-versions. Each sub-version (e.g. 1.1.0-dev.3) has its own detailed design document here.
- **`plans/README.md`** — master index with version summaries, dependency graph (Mermaid), and cross-version concerns.
- **`plan.md`** (repo root) — the **current execution plan**, derived from the active version's design doc. Contains checkboxes `[ ]` / `[x]` tracking per-phase progress.

## Plan document format

Each version plan in `plans/release/` or `plans/dev/` follows this convention:

```markdown
# EazyMake <version> — <short title>

---

## 1 背景
（Why this version is needed）

## 2 目标
（Numbered table of deliverables with priorities）

## 3 详细设计
（Subsections with technical design decisions, file formats, APIs）

## 4 执行步骤
（Phased implementation steps with checkboxes）

## 5 兼容性
（Compatibility matrix — what changes, impact, mitigation）

## 6 跨版本关注点
（Dependencies, maintenance notes, relationship to other versions）
```

## `plan.md` format

The root-level `plan.md` is the executable version of the design doc. It follows the same structure but is focused on **actionable phases**:

```markdown
# EazyMake <version> 执行计划

> 详细设计：plans/release/<version>.md

---

## 1 背景
## 2 目标
## 3 执行阶段
### 阶段一：... (with [ ] / [x] checkboxes)
### 阶段二：...
## 4 关键设计决策
## 5 兼容性矩阵
## 6 延后项
```

## Workflow for adding a new version plan

1. **Create the design doc**: `plans/release/<version>.md` (or `plans/dev/<version>.md` for early dev)
   - Follow the standard format above
   - Include §1~§6 sections
2. **Create the execution plan**: `plan.md` in repo root
   - Link to the design doc in the header
   - Convert design §4 (执行步骤) into actionable checkboxes
3. **Update `plans/README.md`**:
   - Add entry under "当前执行" (current execution)
   - Add row in the version summary table
   - Update the Mermaid dependency graph if needed

## Workflow for updating execution status

As each phase is completed:

1. **Mark checkboxes** in `plan.md`: change `- [ ]` to `- [x]`
2. **Commit** with message: `chore: 阶段N完成 — <brief summary>`
3. **When all phases done**:
   - Move the version from "当前执行" to "已完成" in `plans/README.md`
   - Update `CHANGES.md` with the version changelog
   - Push

## Version numbering conventions

| Pattern | Meaning |
|---------|---------|
| `0.1.6` ~ `0.2.6` | Early development (dev/ folder) |
| `0.9.0` ~ `1.0.0` | Release candidates and stable release (release/ folder) |
| `1.1.0-dev.N` | Sub-versions of 1.1.0 (incremental development) |
| `1.1.0` | Final merged release |

All completed dev sub-versions (dev.1 ~ dev.6) will eventually be merged into the final `1.1.0` release.
