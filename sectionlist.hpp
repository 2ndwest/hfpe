#pragma once

#include <optional>
#include <string>

std::optional<std::string> find_section_id(std::string sectionlist_html,
                                           std::string section_name);
