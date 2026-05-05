//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/console.h"
#include "embed/api.h"

#include <catch2/catch.hpp>

#include <algorithm>
#include <string>
#include <vector>

extern KorkApi::Vm* sVM;

namespace
{
   struct CapturedLine
   {
      ConsoleLogEntry::Level level;
      std::string text;
   };

   void captureLine(U32 level, const char* text, void* userPtr)
   {
      auto* lines = static_cast<std::vector<CapturedLine>*>(userPtr);
      lines->push_back({ static_cast<ConsoleLogEntry::Level>(level), text ? text : "" });
   }

   class ConsumerGuard
   {
   public:
      ConsumerGuard(KorkApi::ConsumerCallback cb, void* userPtr)
         : mCallback(cb), mUserPtr(userPtr)
      {
         Con::addConsumer(cb, userPtr);
      }

      ~ConsumerGuard()
      {
         Con::removeConsumer(mCallback, mUserPtr);
      }

   private:
      KorkApi::ConsumerCallback mCallback;
      void* mUserPtr;
   };

   std::vector<CapturedLine> compileScript(const std::string& script, const char* filename)
   {
      std::vector<CapturedLine> lines;
      ConsumerGuard guard(captureLine, &lines);
      KorkApi::CompiledBlock block{};
      REQUIRE(sVM->compileCodeBlock(script.c_str(), filename, &block));
      sVM->freeCompiledBlock(block);
      return lines;
   }

   bool hasWarning(const std::vector<CapturedLine>& lines, const std::string& needle)
   {
      return std::any_of(lines.begin(), lines.end(), [&](const CapturedLine& line) {
         return line.level == ConsoleLogEntry::Warning && line.text.find(needle) != std::string::npos;
      });
   }
}

TEST_CASE("Break inside switch emits a warning", "[Compiler][Switch]") {
   const std::vector<CapturedLine> lines = compileScript(
      "switch (%foo)\n"
      "{\n"
      "   case 1:\n"
      "      break;\n"
      "}\n",
      "switchBreakWarning.cs");

   REQUIRE(hasWarning(lines, "'break' inside switch is unnecessary"));
}

TEST_CASE("Break inside a loop nested in switch does not warn", "[Compiler][Switch]") {
   const std::vector<CapturedLine> lines = compileScript(
      "switch (%foo)\n"
      "{\n"
      "   case 1:\n"
      "      while (%bar)\n"
      "      {\n"
      "         break;\n"
      "      }\n"
      "}\n",
      "switchLoopBreak.cs");

   REQUIRE_FALSE(hasWarning(lines, "'break' inside switch is unnecessary"));
}
