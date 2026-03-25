/***********************************************************************
Copyright 2018 ASM Assembly Systems GmbH & Co. KG

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

// Copyright (c) ASM Assembly Systems GmbH & Co. KG
//
// FIX: The original file implemented a custom StringView class to avoid
// std::string_view which requires C++17. Since this library now requires
// C++17 (enforced in CMakeLists), we use std::string_view directly.
// All code using Hermes::StringView continues to compile unchanged.
//
#pragma once

#include <string_view>

namespace Hermes
{
    // Drop-in alias — all code using Hermes::StringView continues to compile.
    using StringView = std::string_view;
}
