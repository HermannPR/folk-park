#include "CredentialStore.h"

#include <Security/Security.h>

#include <algorithm>
#include <array>
#include <utility>

namespace folkpark::platform
{
namespace
{
template <typename Type>
class ScopedCf final
{
public:
    explicit ScopedCf(Type valueToOwn = nullptr) : value(valueToOwn) {}
    ~ScopedCf() { if (value != nullptr) CFRelease(value); }
    ScopedCf(const ScopedCf&) = delete;
    ScopedCf& operator=(const ScopedCf&) = delete;
    ScopedCf(ScopedCf&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
    [[nodiscard]] Type get() const noexcept { return value; }

private:
    Type value;
};

bool validIdentifier(const juce::String& value, int maximumLength)
{
    if (value.isEmpty() || value.length() > maximumLength)
        return false;
    for (auto cursor = value.getCharPointer(); !cursor.isEmpty(); ++cursor)
    {
        const auto character = *cursor;
        const auto asciiLetter = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z');
        const auto asciiDigit = character >= '0' && character <= '9';
        if (!asciiLetter && !asciiDigit
            && character != '.' && character != '-' && character != '_')
            return false;
    }
    return true;
}

ScopedCf<CFStringRef> cfString(const juce::String& value)
{
    return ScopedCf<CFStringRef>(CFStringCreateWithCString(
        kCFAllocatorDefault, value.toRawUTF8(), kCFStringEncodingUTF8));
}

ScopedCf<CFMutableDictionaryRef> commonQuery(const juce::String& service,
                                             const juce::String& providerId)
{
    ScopedCf<CFMutableDictionaryRef> query(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    const auto serviceValue = cfString(service);
    const auto accountValue = cfString(providerId);
    if (query.get() == nullptr || serviceValue.get() == nullptr || accountValue.get() == nullptr)
        return ScopedCf<CFMutableDictionaryRef>();

    CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query.get(), kSecAttrService, serviceValue.get());
    CFDictionarySetValue(query.get(), kSecAttrAccount, accountValue.get());
    return query;
}

juce::Result keychainFailure(OSStatus status, const char* operation)
{
    ScopedCf<CFStringRef> description(SecCopyErrorMessageString(status, nullptr));
    juce::String detail;
    if (description.get() != nullptr)
    {
        std::array<char, 512> utf8{};
        if (CFStringGetCString(description.get(), utf8.data(),
                             static_cast<CFIndex>(utf8.size()), kCFStringEncodingUTF8))
            detail = juce::String::fromUTF8(utf8.data());
    }
    auto message = "macOS Keychain " + juce::String(operation) + " failed ("
        + juce::String(static_cast<int>(status)) + ")";
    if (detail.isNotEmpty())
        message += ": " + detail;
    return juce::Result::fail(message);
}
}

SecureCredential::SecureCredential(std::vector<std::uint8_t> bytesToOwn)
    : storage(std::move(bytesToOwn))
{
}

SecureCredential::~SecureCredential()
{
    clear();
}

SecureCredential::SecureCredential(SecureCredential&& other) noexcept
    : storage(std::move(other.storage))
{
}

SecureCredential& SecureCredential::operator=(SecureCredential&& other) noexcept
{
    if (this != &other)
    {
        clear();
        storage = std::move(other.storage);
    }
    return *this;
}

void SecureCredential::clear() noexcept
{
    auto* cursor = reinterpret_cast<volatile std::uint8_t*>(storage.data());
    for (std::size_t index = 0; index < storage.size(); ++index)
        cursor[index] = 0;
    storage.clear();
    storage.shrink_to_fit();
}

MacKeychainCredentialStore::MacKeychainCredentialStore(juce::String service)
    : serviceName(std::move(service))
{
}

bool MacKeychainCredentialStore::platformSupported() noexcept
{
    return true;
}

juce::Result MacKeychainCredentialStore::validateProvider(
    const juce::String& providerId) const
{
    if (!validIdentifier(serviceName, 128))
        return juce::Result::fail("Keychain service identifier is invalid");
    if (!validIdentifier(providerId, 64))
        return juce::Result::fail("Provider identifier is invalid");
    return juce::Result::ok();
}

juce::Result MacKeychainCredentialStore::store(
    const juce::String& providerId,
    std::span<const std::uint8_t> credential)
{
    if (const auto validation = validateProvider(providerId); validation.failed())
        return validation;
    if (credential.empty() || credential.size() > maximumCredentialBytes)
        return juce::Result::fail("Credential is empty or exceeds the native byte bound");

    auto query = commonQuery(serviceName, providerId);
    ScopedCf<CFDataRef> data(CFDataCreate(kCFAllocatorDefault, credential.data(),
                                         static_cast<CFIndex>(credential.size())));
    if (query.get() == nullptr || data.get() == nullptr)
        return juce::Result::fail("Could not prepare a bounded Keychain request");

    ScopedCf<CFMutableDictionaryRef> update(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    if (update.get() == nullptr)
        return juce::Result::fail("Could not prepare a bounded Keychain update");
    CFDictionarySetValue(update.get(), kSecValueData, data.get());
    CFDictionarySetValue(update.get(), kSecAttrAccessible,
                         kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);

    auto status = SecItemUpdate(query.get(), update.get());
    if (status == errSecSuccess)
        return juce::Result::ok();
    if (status != errSecItemNotFound)
        return keychainFailure(status, "update");

    CFDictionarySetValue(query.get(), kSecValueData, data.get());
    CFDictionarySetValue(query.get(), kSecAttrAccessible,
                         kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);
    status = SecItemAdd(query.get(), nullptr);
    if (status == errSecDuplicateItem)
    {
        auto retryQuery = commonQuery(serviceName, providerId);
        status = retryQuery.get() == nullptr ? errSecParam
                                             : SecItemUpdate(retryQuery.get(), update.get());
    }
    return status == errSecSuccess ? juce::Result::ok()
                                   : keychainFailure(status, "store");
}

CredentialReadResult MacKeychainCredentialStore::read(
    const juce::String& providerId) const
{
    if (const auto validation = validateProvider(providerId); validation.failed())
        return {validation, std::nullopt};

    auto query = commonQuery(serviceName, providerId);
    if (query.get() == nullptr)
        return {juce::Result::fail("Could not prepare a bounded Keychain read"), std::nullopt};
    CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef rawResult = nullptr;
    const auto status = SecItemCopyMatching(query.get(), &rawResult);
    ScopedCf<CFTypeRef> result(rawResult);
    if (status == errSecItemNotFound)
        return {juce::Result::ok(), std::nullopt};
    if (status != errSecSuccess)
        return {keychainFailure(status, "read"), std::nullopt};
    if (result.get() == nullptr || CFGetTypeID(result.get()) != CFDataGetTypeID())
        return {juce::Result::fail("Keychain returned an unsupported credential value"),
                std::nullopt};

    const auto data = static_cast<CFDataRef>(result.get());
    const auto length = CFDataGetLength(data);
    if (length <= 0 || length > static_cast<CFIndex>(maximumCredentialBytes))
        return {juce::Result::fail("Keychain credential is outside the native byte bound"),
                std::nullopt};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    CFDataGetBytes(data, CFRangeMake(0, length), bytes.data());
    return {juce::Result::ok(), SecureCredential(std::move(bytes))};
}

juce::Result MacKeychainCredentialStore::remove(const juce::String& providerId)
{
    if (const auto validation = validateProvider(providerId); validation.failed())
        return validation;
    auto query = commonQuery(serviceName, providerId);
    if (query.get() == nullptr)
        return juce::Result::fail("Could not prepare a bounded Keychain removal");
    const auto status = SecItemDelete(query.get());
    return status == errSecSuccess || status == errSecItemNotFound
        ? juce::Result::ok() : keychainFailure(status, "removal");
}
}
