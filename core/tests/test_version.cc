// SPDX-License-Identifier: GPL-3.0-only
#include "dp3core/zarr/version.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
  const std::string version = dp3core::zarr::LibraryVersion();
  if (version.empty() || version != "0.1.0") {
    std::cerr << "LibraryVersion returned '" << version << "'\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
