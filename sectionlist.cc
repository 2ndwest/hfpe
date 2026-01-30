#include "sectionlist.hpp"
#include <optional>
#include <string>

// https://eduapps.mit.edu/mitpe/student/registration/sectionList

std::optional<std::string> find_section_id(std::string sectionlist_html,
                                           std::string section_name) {
  size_t secname_pos = sectionlist_html.find(section_name);
  if (secname_pos == std::string::npos) {
    return std::nullopt;
  }

  size_t secid_start = sectionlist_html.rfind("sectionId=", secname_pos);
  if (secid_start == std::string::npos) {
    return std::nullopt;
  }

  // Skip past "sectionId=" prefix
  secid_start += 10;

  size_t secid_end = sectionlist_html.find("\"", secid_start);
  if (secid_end == std::string::npos || secid_end >= secname_pos) {
    return std::nullopt;
  }

  return sectionlist_html.substr(secid_start, secid_end - secid_start);
}
