//===--- FrontendOptions.cpp ----------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/Frontend/FrontendOptions.h"

#include "swift/AST/DiagnosticsFrontend.h"
#include "swift/Option/Options.h"
#include "swift/Parse/Lexer.h"
#include "swift/Strings.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/Path.h"

using namespace swift;
using namespace llvm::opt;

bool FrontendOptions::needsProperModuleName(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::DumpParse:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::DumpInterfaceHash:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
    return false;
  case ActionType::EmitPCH:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitModuleOnly:
  case ActionType::MergeModules:
    return true;
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;
  case ActionType::EmitAssembly:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
    return true;
  }
  llvm_unreachable("Unknown ActionType");
}

bool FrontendOptions::isActionImmediate(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::DumpParse:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::DumpInterfaceHash:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::EmitPCH:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitModuleOnly:
  case ActionType::MergeModules:
    return false;
  case ActionType::Immediate:
  case ActionType::REPL:
    return true;
  case ActionType::EmitAssembly:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
    return false;
  }
  llvm_unreachable("Unknown ActionType");
}

void FrontendOptions::forAllOutputPaths(
    std::function<void(const std::string &)> fn) const {
  if (RequestedAction != FrontendOptions::ActionType::EmitModuleOnly &&
      RequestedAction != FrontendOptions::ActionType::MergeModules) {
    for (const std::string &OutputFileName : OutputFilenames) {
      fn(OutputFileName);
    }
  }
  const std::string *outputs[] = {
    &ModuleOutputPath,
    &ModuleDocOutputPath,
    &ObjCHeaderOutputPath
  };
  for (const std::string *next : outputs) {
    if (!next->empty())
      fn(*next);
  }
}


StringRef FrontendOptions::originalPath() const {
  if (hasNamedOutputFile())
    // Put the serialized diagnostics file next to the output file.
    return getSingleOutputFilename();

  // If we have a primary input, so use that as the basis for the name of the
  // serialized diagnostics file, otherwise fall back on the
  // module name.
  const auto input = Inputs.getUniquePrimaryInput();
  return input ? llvm::sys::path::filename(input->file())
               : StringRef(ModuleName);
}

bool FrontendOptions::isOutputFileDirectory() const {
  return hasNamedOutputFile() &&
         llvm::sys::fs::is_directory(getSingleOutputFilename());
}

const char *
FrontendOptions::suffixForPrincipalOutputFileForAction(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
    return nullptr;

  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::DumpParse:
  case ActionType::DumpInterfaceHash:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
    return nullptr;

  case ActionType::EmitPCH:
    return PCH_EXTENSION;

  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
    return SIL_EXTENSION;

  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
    return SIB_EXTENSION;

  case ActionType::MergeModules:
  case ActionType::EmitModuleOnly:
    return SERIALIZED_MODULE_EXTENSION;

  case ActionType::Immediate:
  case ActionType::REPL:
    // These modes have no frontend-generated output.
    return nullptr;

  case ActionType::EmitAssembly:
    return "s";

  case ActionType::EmitIR:
    return "ll";

  case ActionType::EmitBC:
    return "bc";

  case ActionType::EmitObject:
    return "o";

  case ActionType::EmitImportedModules:
    return "importedmodules";
  }
}

bool FrontendOptions::hasUnusedDependenciesFilePath() const {
  return !DependenciesFilePath.empty() &&
         !canActionEmitDependencies(RequestedAction);
}

bool FrontendOptions::canActionEmitDependencies(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::DumpParse:
  case ActionType::DumpInterfaceHash:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;
  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::MergeModules:
  case ActionType::EmitModuleOnly:
  case ActionType::EmitPCH:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitAssembly:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
    return true;
  }
}

bool FrontendOptions::hasUnusedObjCHeaderOutputPath() const {
  return !ObjCHeaderOutputPath.empty() && !canActionEmitHeader(RequestedAction);
}

bool FrontendOptions::canActionEmitHeader(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::DumpParse:
  case ActionType::DumpInterfaceHash:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::PrintAST:
  case ActionType::EmitPCH:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;
  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::MergeModules:
  case ActionType::EmitModuleOnly:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitAssembly:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
    return true;
  }
}

bool FrontendOptions::hasUnusedLoadedModuleTracePath() const {
  return !LoadedModuleTracePath.empty() &&
         !canActionEmitLoadedModuleTrace(RequestedAction);
}

bool FrontendOptions::canActionEmitLoadedModuleTrace(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::Parse:
  case ActionType::DumpParse:
  case ActionType::DumpInterfaceHash:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;
  case ActionType::Typecheck:
  case ActionType::MergeModules:
  case ActionType::EmitModuleOnly:
  case ActionType::EmitPCH:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitAssembly:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
    return true;
  }
}

bool FrontendOptions::hasUnusedModuleOutputPath() const {
  return !ModuleOutputPath.empty() && !canActionEmitModule(RequestedAction);
}

bool FrontendOptions::hasUnusedModuleDocOutputPath() const {
  return !ModuleDocOutputPath.empty() && !canActionEmitModule(RequestedAction);
}

bool FrontendOptions::canActionEmitModule(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::DumpParse:
  case ActionType::DumpInterfaceHash:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::PrintAST:
  case ActionType::EmitPCH:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::EmitSILGen:
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;
  case ActionType::MergeModules:
  case ActionType::EmitModuleOnly:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitAssembly:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
    return true;
  }
}

bool FrontendOptions::canActionEmitModuleDoc(ActionType action) {
  return canActionEmitModule(action);
}

bool FrontendOptions::doesActionProduceOutput(ActionType action) {
  switch (action) {
  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::DumpParse:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::DumpInterfaceHash:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::EmitPCH:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::EmitModuleOnly:
  case ActionType::EmitAssembly:
  case ActionType::EmitIR:
  case ActionType::EmitBC:
  case ActionType::EmitObject:
  case ActionType::EmitImportedModules:
  case ActionType::MergeModules:
    return true;

  case ActionType::NoneAction:
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;
  }
  llvm_unreachable("Unknown ActionType");
}

bool FrontendOptions::doesActionProduceTextualOutput(ActionType action) {
  switch (action) {
  case ActionType::NoneAction:
  case ActionType::EmitPCH:
  case ActionType::EmitSIBGen:
  case ActionType::EmitSIB:
  case ActionType::MergeModules:
  case ActionType::EmitModuleOnly:
  case ActionType::EmitBC:
  case ActionType::EmitObject:
  case ActionType::Immediate:
  case ActionType::REPL:
    return false;

  case ActionType::Parse:
  case ActionType::Typecheck:
  case ActionType::DumpParse:
  case ActionType::DumpInterfaceHash:
  case ActionType::DumpAST:
  case ActionType::EmitSyntax:
  case ActionType::PrintAST:
  case ActionType::DumpScopeMaps:
  case ActionType::DumpTypeRefinementContexts:
  case ActionType::EmitImportedModules:
  case ActionType::EmitSILGen:
  case ActionType::EmitSIL:
  case ActionType::EmitAssembly:
  case ActionType::EmitIR:
    return true;
  }
}
