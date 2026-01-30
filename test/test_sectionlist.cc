#include "sectionlist.hpp"
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
}
