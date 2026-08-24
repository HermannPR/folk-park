#include "platform/CredentialStore.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

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

bool equals(std::span<const std::uint8_t> actual,
            std::span<const std::uint8_t> expected)
{
    return actual.size() == expected.size()
        && std::equal(actual.begin(), actual.end(), expected.begin());
}
}

int main()
{
    using namespace folkpark::platform;
    const auto service = "com.folkpark.audio.assistant.tests." +
        juce::Uuid().toString().removeCharacters("-");
    MacKeychainCredentialStore store(service);
    constexpr auto provider = "bounded-test-provider";

    expect(MacKeychainCredentialStore::platformSupported(),
           "The macOS build must expose native Keychain support");
    expect(store.remove(provider).wasOk(),
           "Exact test-account cleanup must be idempotent before the test");
    auto absent = store.read(provider);
    expect(absent.status.wasOk() && !absent.credential.has_value(),
           "An absent Keychain item must be distinguishable from a read failure");

    const std::array<std::uint8_t, 9> first{0, 1, 2, 3, 4, 5, 0xfe, 0xff, 0x7f};
    expect(store.store(provider, first).wasOk(),
           "A bounded opaque credential must be written to Keychain");
    auto firstRead = store.read(provider);
    expect(firstRead.status.wasOk() && firstRead.credential.has_value()
               && equals(firstRead.credential->bytes(), first),
           "Keychain must return the exact opaque credential bytes");

    const std::array<std::uint8_t, 5> replacement{9, 8, 7, 6, 5};
    expect(store.store(provider, replacement).wasOk(),
           "Storing the same provider must update instead of duplicating");
    auto replacementRead = store.read(provider);
    expect(replacementRead.status.wasOk() && replacementRead.credential.has_value()
               && equals(replacementRead.credential->bytes(), replacement),
           "Updated Keychain bytes must replace the earlier value");

    const std::vector<std::uint8_t> oversized(
        CredentialStore::maximumCredentialBytes + 1, 0x2a);
    const auto nonAsciiProvider = juce::String::fromUTF8("non-ascii-\xc3\xb1");
    expect(store.store(provider, {}).failed()
               && store.store(provider, oversized).failed()
               && store.store("../invalid", replacement).failed()
               && store.store(nonAsciiProvider, replacement).failed(),
           "Empty, oversized, and invalid-identifier writes must fail before Keychain");
    expect(store.read("../invalid").status.failed()
               && store.remove("../invalid").failed(),
           "Invalid identifiers must never reach read or removal queries");

    expect(store.remove(provider).wasOk(),
           "The exact temporary Keychain credential must be removable");
    const auto removed = store.read(provider);
    expect(removed.status.wasOk() && !removed.credential.has_value(),
           "Removed credentials must not remain readable");

    if (failures == 0)
        std::cout << "PASS: bounded native macOS Keychain credential storage\n";
    return failures == 0 ? 0 : 1;
}
