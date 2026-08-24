#include "diagnostics/Diagnostics.h"

#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}
}

int main()
{
    using namespace folkpark::diagnostics;
    Snapshot snapshot;
    snapshot.productVersion = "0.1.0";
    snapshot.buildType = "Release";
    snapshot.architecture = "x86_64";
    snapshot.wrapperFormat = "VST3";
    snapshot.hostName = "FL Studio/../../secret\nproject.flp";
    snapshot.hostVersion = "2025.2";
    snapshot.sampleRate = 48000.0;
    snapshot.maximumBlockSize = 512;
    snapshot.activeVoices = 4;
    snapshot.preset = ServiceCode::missingAssets;
    snapshot.database = ServiceCode::ready;
    snapshot.nonFiniteOutputSamples = 3;
    snapshot.rejectedProjectStates = 2;

    const auto first = formatReport(snapshot);
    const auto second = formatReport(snapshot);
    bool passed = true;
    passed &= expect(first == second, "the same typed snapshot must format deterministically");
    passed &= expect(first.getNumBytesAsUTF8() < maximumReportBytes,
                     "the report must remain below 4 KiB");
    passed &= expect(first.contains("host_name: FL Studio\n"),
                     "host text must stop before path separators or appended lines");
    const auto hostLine = first.fromFirstOccurrenceOf("host_name: ", false, false)
                              .upToFirstOccurrenceOf("\n", false, false);
    passed &= expect(!first.contains("../") && !hostLine.containsChar('\n')
                         && !hostLine.contains("project.flp"),
                     "diagnostics must not retain path syntax or appended project-like text");
    passed &= expect(first.contains("preset_status: missing-assets"),
                     "subsystem states must use fixed codes");
    passed &= expect(first.contains("fault_nonfinite_output_samples: 3"),
                     "audio fault counters must be represented without messages");

    PreviewSession session;
    const auto preview = session.create(snapshot);
    passed &= expect(!preview.id.isEmpty() && preview.text == first,
                     "preview must expose an opaque ID and exact report text");
    passed &= expect(!session.textForCopy("stale-preview").has_value(),
                     "a stale or invented preview ID must not be copyable");
    const auto copy = session.textForCopy(preview.id);
    passed &= expect(copy.has_value() && *copy == preview.text,
                     "copy must return the exact currently previewed text");
    session.clear();
    passed &= expect(!session.textForCopy(preview.id).has_value(),
                     "clearing the session must invalidate the prior preview");

    if (passed)
        std::cout << "Diagnostics contract tests passed\n";
    return passed ? 0 : 1;
}
