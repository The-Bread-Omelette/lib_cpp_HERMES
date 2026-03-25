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
// FIX: Replaced the custom Optional<T> class with std::optional<T>.
// The library requires C++17 (enforced in CMakeLists.txt), so std::optional
// is always available.
//
// FIX: Added operator<< for std::optional<T> in the Hermes namespace.
// The internal implementation files (AsioServer.cpp etc.) use BuildString()
// which calls operator<< on every argument. The old custom Optional<T> had
// operator<< defined on it. std::optional does not. Adding it here restores
// that behaviour without modifying any internal .cpp files.
//
#pragma once

#include <optional>
#include <ostream>

namespace Hermes
{
    // Drop-in alias — all code using Hermes::Optional<T> compiles unchanged.
    template<class T>
    using Optional = std::optional<T>;

    // operator<< for Hermes::Optional<T> (= std::optional<T>).
    // Required by internal StringBuilder usage in AsioServer.cpp and friends.
    // Prints the contained value if present, or "<empty>" if not.
    template<class S, class T>
    S& operator<<(S& s, const std::optional<T>& o)
    {
        if (o.has_value())
            s << *o;
        else
            s << "<empty>";
        return s;
    }
}