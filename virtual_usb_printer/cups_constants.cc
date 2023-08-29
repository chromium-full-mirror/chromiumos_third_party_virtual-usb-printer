// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cups_constants.h"

#include <iostream>
#include <type_traits>

std::ostream& operator<<(std::ostream& os, const IppTag& tag) {
  os << static_cast<std::underlying_type<IppTag>::type>(tag);
  return os;
}
