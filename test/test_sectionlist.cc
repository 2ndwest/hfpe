#include "sectionlist.h"
#include <cassert>
#include <fstream>
#include <sstream>

int main(void) {
  std::ifstream file("test/sectionlist_resp.html");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string sectionlist_html = buffer.str();

  assert(find_section_id(sectionlist_html, "PE.0720-1") ==
         "CB80ED8E12092FD100000199C5B0B3DD");
  assert(find_section_id(sectionlist_html, "PE.0800-1 ") ==
         "07A060AB12092FD100000199C5B0B3B0");
  assert(find_section_id(sectionlist_html, "PE.0626-2") ==
         "2C7EB43712092FD100000199C5B0B3C8");
  assert(find_section_id(sectionlist_html, "PE.9999-0") == std::nullopt);
}
