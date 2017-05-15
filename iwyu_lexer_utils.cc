//===--- iwyu_lexer_utils.cc - clang-lexer utilities for iwyu -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "iwyu_lexer_utils.h"

#include <cstring>
#include <string>
#include <vector>

#include "iwyu_verrs.h"
#include "port.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Token.h"

using clang::Lexer;
using clang::LangOptions;
using clang::SourceLocation;
using clang::SourceManager;
using clang::SourceRange;
using clang::Token;
using std::string;
using std::vector;

namespace include_what_you_use {

// SourceManagerCharacterDataGetter method implementations.
SourceManagerCharacterDataGetter::SourceManagerCharacterDataGetter(
    const SourceManager& source_manager)
    : source_manager_(source_manager) {
}

const char* SourceManagerCharacterDataGetter::GetCharacterData(
    SourceLocation loc) const {
  bool invalid;
  const char* data = source_manager_.getCharacterData(loc, &invalid);
  CHECK_(!invalid);
  CHECK_(data);
  return data;
}

string GetSourceTextUntilLogicalEndOfLine(
    SourceLocation start_loc,
    const CharacterDataGetterInterface& data_getter) {
  const char* data = data_getter.GetCharacterData(start_loc);
  const char* newline_data = 0;
  bool split_line = false;
  string source_text;
  do {
    newline_data = strchr(data, '\n');
    if (!newline_data) {
      source_text.append(data);
    } else {
      source_text.append(data, newline_data - data);
      split_line = (newline_data != data && newline_data[-1] == '\\');
      if (split_line) {
        source_text.push_back('\n');
      }
      data = newline_data + 1;
    }
  } while (newline_data && split_line);
  return source_text;
}

SourceLocation GetLocationAfter(
    SourceLocation start_loc, const string& needle,
    const CharacterDataGetterInterface& data_getter) {
  CHECK_(start_loc.isValid() && "GetLocationAfter takes only valid locations");
  const char* data = data_getter.GetCharacterData(start_loc);
  const char* needle_loc = strstr(data, needle.c_str());
  if (!needle_loc)
    return SourceLocation();   // invalid source location
  return start_loc.getLocWithOffset(needle_loc - data + needle.length());
}

string GetIncludeNameAsWritten(
    SourceLocation include_loc,
    const CharacterDataGetterInterface& data_getter) {
  const string data = GetSourceTextUntilLogicalEndOfLine(include_loc, data_getter);
  if (data.empty())
    return data;
  string::size_type endpos = string::npos;
  if (data[0] == '<') {
    endpos = data.find('>', 1);
  } else if (data[0] == '"') {
    endpos = data.find('"', 1);
  } else {
    CHECK_UNREACHABLE_("Unexpected token being #included");
  }
  CHECK_(endpos != string::npos && "No end-character found for #include");
  return data.substr(0, endpos+1);
}

// Get the text of a given token.
string GetTokenText(const Token& token,
                    const CharacterDataGetterInterface& data_getter) {
  const char* text = data_getter.GetCharacterData(token.getLocation());
  return string(text, token.getLength());
}

// Given the range of an #if or #elif statement, determine the
// symbols which are arguments to "defined". This allows iwyu to
// treat these symbols as if #ifdef was used instead.
vector<Token> FindArgumentsToDefined(
    SourceRange range,
    const CharacterDataGetterInterface& data_getter) {
  const char* text = data_getter.GetCharacterData(range.getBegin());
  const char* text_end = data_getter.GetCharacterData(range.getEnd());

  // Ugh. The lexer wants the text to be nul-terminated. Make a copy.
  const unsigned range_length = text_end - text;
  const string range_str(text, range_length);
  const char* range_cstr = range_str.c_str();

  VERRS(8) << "Lexing: " << range_str << "\n";
  Lexer lexer(range.getBegin(), LangOptions(), range_cstr, range_cstr,
              range_cstr + range_length);

  vector<Token> ret;
  Token token;
  enum { kLookingForDefined,
         kExpectingLeftParenOrDefinedIdentifier,
         kExpectingDefinedIdentifier } state = kLookingForDefined;
  while (!lexer.LexFromRawLexer(token)) {
    VERRS(8) << "Processing token \""
             << GetTokenText(token, data_getter)
             << "\" of type " << token.getName()
             << " in state " << state << "\n";
    switch (state) {
      case kLookingForDefined:
        if (token.getKind() == clang::tok::raw_identifier) {
          if (GetTokenText(token, data_getter) == "defined") {
            state = kExpectingLeftParenOrDefinedIdentifier;
          }
        }
        break;
      case kExpectingLeftParenOrDefinedIdentifier:
        if (token.getKind() == clang::tok::l_paren) {
          state = kExpectingDefinedIdentifier;
          continue;
        }
        // Fall through.
      case kExpectingDefinedIdentifier:
        CHECK_(token.getKind() == clang::tok::raw_identifier);
        ret.push_back(token);
        state = kLookingForDefined;
        break;
    }
  }
  return ret;
}

}  // namespace include_what_you_use
