//===-- BenchmarkRunner.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the abstract BenchmarkRunner class for measuring a certain execution
/// property of instructions (e.g. latency).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_EXEGESIS_BENCHMARKRUNNER_H
#define LLVM_TOOLS_LLVM_EXEGESIS_BENCHMARKRUNNER_H

#include "Assembler.h"
#include "BenchmarkCode.h"
#include "BenchmarkResult.h"
#include "LlvmState.h"
#include "MCInstrDescView.h"
#include "SnippetRepetitor.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/Error.h"
#include <cstdlib>
#include <memory>
#include <vector>

namespace llvm {
namespace exegesis {

// Common code for all benchmark modes.
class BenchmarkRunner {
public:
  explicit BenchmarkRunner(const LLVMState &State,
                           Benchmark::ModeE Mode,
                           BenchmarkPhaseSelectorE BenchmarkPhaseSelector);

  virtual ~BenchmarkRunner();

  class RunnableConfiguration {
    friend class BenchmarkRunner;

  public:
    ~RunnableConfiguration() = default;
    RunnableConfiguration(RunnableConfiguration &&) = default;

    RunnableConfiguration(const RunnableConfiguration &) = delete;
    RunnableConfiguration &operator=(RunnableConfiguration &&) = delete;
    RunnableConfiguration &operator=(const RunnableConfiguration &) = delete;

  private:
    RunnableConfiguration() = default;

    Benchmark InstrBenchmark;
    object::OwningBinary<object::ObjectFile> ObjectFile;
  };

  Expected<RunnableConfiguration>
  getRunnableConfiguration(const BenchmarkCode &Configuration,
                           unsigned NumRepetitions, unsigned LoopUnrollFactor,
                           const SnippetRepetitor &Repetitor) const;

  Expected<Benchmark> runConfiguration(RunnableConfiguration &&RC,
                                                  bool DumpObjectToDisk) const;

  // Scratch space to run instructions that touch memory.
  struct ScratchSpace {
    static constexpr const size_t kAlignment = 1024;
    static constexpr const size_t kSize = 1 << 20; // 1MB.
    ScratchSpace()
        : UnalignedPtr(std::make_unique<char[]>(kSize + kAlignment)),
          AlignedPtr(
              UnalignedPtr.get() + kAlignment -
              (reinterpret_cast<intptr_t>(UnalignedPtr.get()) % kAlignment)) {}
    char *ptr() const { return AlignedPtr; }
    void clear() { std::memset(ptr(), 0, kSize); }

  private:
    const std::unique_ptr<char[]> UnalignedPtr;
    char *const AlignedPtr;
  };

  // A helper to measure counters while executing a function in a sandboxed
  // context.
  class FunctionExecutor {
  public:
    virtual ~FunctionExecutor();
    // FIXME deprecate this.
    virtual Expected<int64_t> runAndMeasure(const char *Counters) const = 0;

    virtual Expected<llvm::SmallVector<int64_t, 4>>
    runAndSample(const char *Counters) const = 0;
  };

protected:
  const LLVMState &State;
  const Benchmark::ModeE Mode;
  const BenchmarkPhaseSelectorE BenchmarkPhaseSelector;

private:
  virtual Expected<std::vector<BenchmarkMeasure>>
  runMeasurements(const FunctionExecutor &Executor) const = 0;

  Expected<SmallString<0>> assembleSnippet(const BenchmarkCode &BC,
                                           const SnippetRepetitor &Repetitor,
                                           unsigned MinInstructions,
                                           unsigned LoopBodySize) const;

  Expected<std::string> writeObjectFile(StringRef Buffer) const;

  const std::unique_ptr<ScratchSpace> Scratch;
};

} // namespace exegesis
} // namespace llvm

#endif // LLVM_TOOLS_LLVM_EXEGESIS_BENCHMARKRUNNER_H
