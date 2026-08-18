// 1.2.3: `ezmk example` — scaffold built-in examples from the embedded table
// (src/example_data.cpp, generated at build time from examples/).
#include "ezmk/example.hpp"
#include "ezmk/i18n.hpp"
#include "ezmk/util.hpp"

#include <algorithm>
#include <map>
#include <string>

namespace ezmk::example {

void list_examples() {
    const auto& examples = embedded_examples();
    util::info_line(i18n::get(i18n::I18nKey::example_list_header));
    for (auto& e : examples) {
        util::info_line(i18n::fmt(i18n::I18nKey::example_list_item,
                                  {{"name", e.name},
                                   {"description", e.description}}));
    }
}

void create_example(const std::string& name, const fs::path& output_dir) {
    const auto& examples = embedded_examples();
    auto it = std::find_if(examples.begin(), examples.end(),
                           [&](const Example& e) { return name == e.name; });
    if (it == examples.end()) {
        // Build the available list for the error message.
        std::string list;
        for (size_t i = 0; i < examples.size(); ++i) {
            if (i > 0) list += ", ";
            list += examples[i].name;
        }
        util::fatal(i18n::I18nKey::example_not_found,
                    {{"name", name}, {"list", list}});
    }

    fs::path root = output_dir / name;
    if (util::file_exists(root)) {
        util::fatal(i18n::I18nKey::example_exists, {{"path", root.string()}});
    }

    util::info(i18n::I18nKey::example_generating, {{"name", name}});

    // Write every embedded file, creating parent directories as needed.
    for (auto& f : it->files) {
        fs::path target = root / f.path;
        if (!util::file_write(target, f.content)) {
            util::fatal("failed to write file: " + target.string());
        }
    }

    util::info(i18n::I18nKey::example_created, {{"name", name},
                                                {"path", root.string()}});
}

} // namespace ezmk::example
