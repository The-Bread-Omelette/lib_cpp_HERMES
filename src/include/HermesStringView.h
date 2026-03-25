// Copyright (c) ASM Assembly Systems GmbH & Co. KG
//
// C interface string view — a non-owning reference to a character sequence.
// This header is used by both C and C++ translation units.
//
#ifndef HERMES_STRING_VIEW_H
#define HERMES_STRING_VIEW_H

#include <stdint.h>
#include <stddef.h>   /* FIX: required for size_t */

#ifdef __cplusplus
extern "C" {
#endif

struct HermesStringView
{
    const char* m_pData;
    size_t      m_size;   /* FIX: field was missing in previous version */
};

#ifdef __cplusplus
}
#endif

#endif /* HERMES_STRING_VIEW_H */
