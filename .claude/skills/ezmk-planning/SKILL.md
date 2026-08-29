---
name: ezmk-planning
description: How to work with EazyMake version plans — plans/ directory structure, plan document format, and updating execution status.
---

# EazyMake Planning

## Directory structure

```
plans/
├── README.md              # Version index + roadmap + dependency graph (series-level)
├── 0.x.x/                 # Early development version plans (0.1.6 ~ 0.2.6) — flat
│   ├── README.md          #   series version index
│   └── 0.1.6.md ~ 0.2.6.md
├── 1.0.0/                 # Plans leading to 1.0.0 (0.9.0 ~ 1.0.0) — flat
│   ├── README.md          #   series version index
│   └── 0.9.0.md ~ 0.9.10.md, 1.0.0.md
├── 1.1.x/                 # 1.1.x series docs (1.1.0 formal + 1.1.1/1.1.2/1.1.3 patches) — flat
│   ├── README.md          #   series version index (merged)
│   ├── 1.1.0-dev.1.md ~ 1.1.0-pre.3.md, 1.1.0.md
│   └── 1.1.1.md / 1.1.2.md / 1.1.3.md
├── 1.2.x/                 # 1.2.x series (1.2.0 dev/pre/formal + 1.2.1~1.2.5 patches) — flat
│   ├── README.md          #   series version index (merged)
│   └── 1.2.0-dev.N.md / 1.2.0-pre.N.md / 1.2.0.md / 1.2.1.md ~ 1.2.5.md
├── 1.3.x/                 # 1.3.x series (1.3.0 dev/pre/formal + 1.3.1~1.3.6 patches) — flat
│   ├── README.md          #   series version index (merged)
│   └── 1.3.0-dev.N.md / 1.3.0-pre.N.md / 1.3.0.md / 1.3.1.md ~ 1.3.6.md
├── 1.4.x/                 # 1.4.x series (1.4.0 dev/pre — current, e.g. 1.4.0-pre.1) — flat
│   ├── README.md          #   series version index (merged)
│   └── 1.4.0-dev.1.md ~ 1.4.0-dev.7.md / 1.4.0-pre.1.md
└── plan.md                # (repo root) Current execution plan — mirrors the active version's design doc
```

- **`plans/README.md`** — top-level index: brief description + link for each major series (0.x.x / 1.0.0 / 1.1.x / 1.2.x / 1.3.x / 1.4.x), roadmap, dependency graph, and cross-version concerns.
- **`0.x.x/`** — plans for early development versions (0.1.x ~ 0.2.x). All completed.
- **`1.0.0/`** — plans for release versions leading up to 1.0.0 (0.9.0 ~ 1.0.0).
- **`1.1.x/`** — plans for the 1.1.x series: 1.1.0 release series (dev sub-versions `1.1.0-dev.N`, pre-releases `1.1.0-pre.N`, final `1.1.0` plan) plus the 1.1.1/1.1.2/1.1.3 patch plans. All files are flat in this folder (no subfolders).
- **`1.2.x/`** — plans for the 1.2.x series: 1.2.0 dev/pre sub-versions + the final `1.2.0` plan + the `1.2.1` ~ `1.2.5` patch plans. All files are flat in this folder (no subfolders).
- **`1.3.x/`** — plans for the 1.3.x series: 1.3.0 dev/pre sub-versions + the final `1.3.0` plan + the `1.3.1` ~ `1.3.6` patch plans. All files are flat in this folder (no subfolders).
- **`1.4.x/`** — plans for the current 1.4.x series: `1.4.0-dev.1` ~ `1.4.0-dev.7` + `1.4.0-pre.1`. All files are flat in this folder (no subfolders).
- **`<series>/README.md`** — per-series version index: overview + version table (theme / deliverables / dependency) with links to each plan file.
- **`plans/README.md`** — master index with version summaries, dependency graph (Mermaid), and cross-version concerns.
- **`plan.md`** (repo root) — the **current execution plan**, derived from the active version's design doc. Contains checkboxes `[ ]` / `[x]` tracking per-phase progress.

## Plan document format

Each version plan in `plans/0.x.x/`, `plans/1.0.0/`, `plans/1.1.x/`, `plans/1.2.x/`, `plans/1.3.x/`, or `plans/1.4.x/` follows this convention:

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

> 详细设计：plans/<series>/<version>.md   # e.g. plans/1.1.x/1.1.0.md

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

1. **Create the design doc**: `plans/<series>/<version>.md` — `0.x.x/` for early dev, `1.0.0/` for the 1.0.0 series, `1.1.x/` for the 1.1.x patch series, `1.2.x/` for the 1.2.x series (1.2.0 dev/pre + 1.2.1~1.2.5 patches, flat), `1.3.x/` for the 1.3.x series (1.3.0 dev/pre + 1.3.1~1.3.6 patches, flat), `1.4.x/` for the current 1.4.x series (1.4.0 dev/pre, flat)
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
| `0.1.6` ~ `0.2.6` | Early development (0.x.x/ folder) |
| `0.9.0` ~ `1.0.0` | Release candidates and stable release (1.0.0/ folder) |
| `1.x.0-dev.N` (e.g. `1.2.0-dev.N`, `1.3.0-dev.N`, `1.4.0-dev.N`) | Development sub-versions — incremental development |
| `1.x.0-pre.N` (e.g. `1.2.0-pre.N`, `1.3.0-pre.N`, `1.4.0-pre.N`) | Pre-releases — release-phase closing (documentation/checkpoints) |
| `1.x.0` (e.g. `1.1.0`, `1.2.0`, `1.3.0`) | Final merged formal release (1.x.x/ folder) |
| `1.x.N` (e.g. `1.2.1` ~ `1.2.5`, `1.3.1` ~ `1.3.6`) | Patch releases after the formal version |

All completed dev sub-versions (e.g. 1.4.0 dev.1 ~ dev.7) will eventually be merged into the final `1.x.0` release.
