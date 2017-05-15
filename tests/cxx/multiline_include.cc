//===--- iterator.cc - test input file for iwyu ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Multiline include needs to be handled as if written on a single line

#include <algo\
rithm>
#include <l\
i\
s\
t>
#include <str\
ing>

void foo(std::string);

/**** IWYU_SUMMARY

tests/cxx/multiline_include.cc should add these lines:

tests/cxx/multiline_include.cc should remove these lines:
- #include <algo\
rithm>  // lines XX-XX+1
- #include <l\
i\
s\
t>  // lines XX-XX+3

The full include-list for tests/cxx/multiline_include.cc:
#include <str\
ing>

***** IWYU_SUMMARY */
