#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace folkpark::platform
{
class SecureCredential final
{
public:
    SecureCredential() = default;
    explicit SecureCredential(std::vector<std::uint8_t> bytesToOwn);
    ~SecureCredential();

    SecureCredential(const SecureCredential&) = delete;
    SecureCredential& operator=(const SecureCredential&) = delete;
    SecureCredential(SecureCredential&& other) noexcept;
    SecureCredential& operator=(SecureCredential&& other) noexcept;

    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return storage; }
    [[nodiscard]] bool empty() const noexcept { return storage.empty(); }

private:
    void clear() noexcept;
    std::vector<std::uint8_t> storage;
};

struct CredentialReadResult
{
    juce::Result status = juce::Result::ok();
    std::optional<SecureCredential> credential;
};

class CredentialStore
{
public:
    static constexpr std::size_t maximumCredentialBytes = 16 * 1024;
    virtual ~CredentialStore() = default;

    [[nodiscard]] virtual juce::Result store(
        const juce::String& providerId,
        std::span<const std::uint8_t> credential) = 0;
    [[nodiscard]] virtual CredentialReadResult read(const juce::String& providerId) const = 0;
    [[nodiscard]] virtual juce::Result remove(const juce::String& providerId) = 0;
};

class MacKeychainCredentialStore final : public CredentialStore
{
public:
    static constexpr auto productionService = "com.folkpark.audio.assistant";

    explicit MacKeychainCredentialStore(
        juce::String service = productionService);

    [[nodiscard]] static bool platformSupported() noexcept;
    [[nodiscard]] juce::Result store(
        const juce::String& providerId,
        std::span<const std::uint8_t> credential) override;
    [[nodiscard]] CredentialReadResult read(const juce::String& providerId) const override;
    [[nodiscard]] juce::Result remove(const juce::String& providerId) override;

private:
    [[nodiscard]] juce::Result validateProvider(const juce::String& providerId) const;
    juce::String serviceName;
};
}
