#include "midi/MidiProof.h"

#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main()
{
    const auto first = folkpark::midi::createM0ProofMidi();
    const auto second = folkpark::midi::createM0ProofMidi();

    expect(first == second, "M0 MIDI output must be deterministic");
    const auto validation = folkpark::midi::validateM0ProofMidi(first);
    expect(validation.wasOk(), validation.getErrorMessage().toRawUTF8());

    if (failures == 0)
        std::cout << "PASS: deterministic MIDI proof writes and reopens with complete note lifecycle\n";
    return failures == 0 ? 0 : 1;
}
