// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_secret_store.h"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(ROCPROFVIS_HAVE_LIBSECRET)
#include <libsecret/secret.h>
#endif

namespace RocProfVis
{
namespace View
{

namespace
{
    // Prefix so our entries are recognizable in the OS credential UI.
    const char* const SERVICE_NAME = "ROCm-Optiq";
}  // namespace

#if defined(_WIN32)

namespace
{
    std::wstring Widen(const std::string& utf8)
    {
        if(utf8.empty())
        {
            return std::wstring();
        }
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                       static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()),
                            wide.data(), size);
        return wide;
    }

    std::wstring TargetName(const std::string& key)
    {
        return Widen(std::string(SERVICE_NAME) + ":" + key);
    }
}  // namespace

bool SecretStore::IsAvailable()
{
    return true;
}

bool SecretStore::Set(const std::string& key, const std::string& secret)
{
    std::wstring target = TargetName(key);

    CREDENTIALW cred        = {};
    cred.Type               = CRED_TYPE_GENERIC;
    cred.TargetName         = const_cast<LPWSTR>(target.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
    cred.CredentialBlob     = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
    cred.Persist            = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteW(&cred, 0) == TRUE;
}

bool SecretStore::Get(const std::string& key, std::string& out_secret)
{
    out_secret.clear();

    PCREDENTIALW cred = nullptr;
    if(CredReadW(TargetName(key).c_str(), CRED_TYPE_GENERIC, 0, &cred) != TRUE)
    {
        return false;
    }

    if(cred->CredentialBlob != nullptr && cred->CredentialBlobSize > 0)
    {
        out_secret.assign(reinterpret_cast<const char*>(cred->CredentialBlob),
                          cred->CredentialBlobSize);
    }
    CredFree(cred);
    return true;
}

bool SecretStore::Erase(const std::string& key)
{
    if(CredDeleteW(TargetName(key).c_str(), CRED_TYPE_GENERIC, 0) == TRUE)
    {
        return true;
    }
    return GetLastError() == ERROR_NOT_FOUND;
}

#elif defined(__APPLE__)

namespace
{
    // Query matching a single generic-password item for the given key.
    CFDictionaryRef MakeIdentityQuery(const std::string& key)
    {
        CFStringRef service =
            CFStringCreateWithCString(nullptr, SERVICE_NAME, kCFStringEncodingUTF8);
        CFStringRef account =
            CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);

        const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
        const void* vals[] = { kSecClassGenericPassword, service, account };
        CFDictionaryRef query =
            CFDictionaryCreate(nullptr, keys, vals, 3, &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);

        CFRelease(service);
        CFRelease(account);
        return query;
    }
}  // namespace

bool SecretStore::IsAvailable()
{
    return true;
}

bool SecretStore::Set(const std::string& key, const std::string& secret)
{
    CFDictionaryRef query = MakeIdentityQuery(key);
    CFDataRef       data  = CFDataCreate(nullptr,
                                         reinterpret_cast<const UInt8*>(secret.data()),
                                         static_cast<CFIndex>(secret.size()));

    const void*     update_keys[] = { kSecValueData };
    const void*     update_vals[] = { data };
    CFDictionaryRef update        =
        CFDictionaryCreate(nullptr, update_keys, update_vals, 1,
                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecItemUpdate(query, update);
    if(status == errSecItemNotFound)
    {
        CFMutableDictionaryRef add = CFDictionaryCreateMutableCopy(nullptr, 0, query);
        CFDictionarySetValue(add, kSecValueData, data);
        status = SecItemAdd(add, nullptr);
        CFRelease(add);
    }

    CFRelease(update);
    CFRelease(data);
    CFRelease(query);
    return status == errSecSuccess;
}

bool SecretStore::Get(const std::string& key, std::string& out_secret)
{
    out_secret.clear();

    CFStringRef service =
        CFStringCreateWithCString(nullptr, SERVICE_NAME, kCFStringEncodingUTF8);
    CFStringRef account =
        CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);

    const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount,
                           kSecReturnData, kSecMatchLimit };
    const void* vals[] = { kSecClassGenericPassword, service, account,
                           kCFBooleanTrue, kSecMatchLimitOne };
    CFDictionaryRef query =
        CFDictionaryCreate(nullptr, keys, vals, 5, &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks);

    CFTypeRef result = nullptr;
    OSStatus  status = SecItemCopyMatching(query, &result);

    CFRelease(service);
    CFRelease(account);
    CFRelease(query);

    if(status != errSecSuccess || result == nullptr)
    {
        return false;
    }

    CFDataRef data = reinterpret_cast<CFDataRef>(result);
    out_secret.assign(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                      static_cast<size_t>(CFDataGetLength(data)));
    CFRelease(result);
    return true;
}

bool SecretStore::Erase(const std::string& key)
{
    CFDictionaryRef query  = MakeIdentityQuery(key);
    OSStatus        status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}

#elif defined(ROCPROFVIS_HAVE_LIBSECRET)

namespace
{
    const SecretSchema* Schema()
    {
        static const SecretSchema schema = {
            "com.amd.roc-optiq.ssh",
            SECRET_SCHEMA_NONE,
            {
                { "key", SECRET_SCHEMA_ATTRIBUTE_STRING },
                { nullptr, static_cast<SecretSchemaAttributeType>(0) },
            },
            0, 0, 0, 0, 0, 0, 0, 0
        };
        return &schema;
    }
}  // namespace

bool SecretStore::IsAvailable()
{
    GError*        error   = nullptr;
    SecretService* service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if(error != nullptr)
    {
        g_error_free(error);
        return false;
    }
    if(service == nullptr)
    {
        return false;
    }
    g_object_unref(service);
    return true;
}

bool SecretStore::Set(const std::string& key, const std::string& secret)
{
    GError*     error = nullptr;
    std::string label = std::string(SERVICE_NAME) + ": " + key;
    gboolean    ok    = secret_password_store_sync(Schema(), SECRET_COLLECTION_DEFAULT,
                                                   label.c_str(), secret.c_str(), nullptr,
                                                   &error, "key", key.c_str(), nullptr);
    if(error != nullptr)
    {
        g_error_free(error);
        return false;
    }
    return ok == TRUE;
}

bool SecretStore::Get(const std::string& key, std::string& out_secret)
{
    out_secret.clear();

    GError* error = nullptr;
    gchar*  value = secret_password_lookup_sync(Schema(), nullptr, &error, "key",
                                                key.c_str(), nullptr);
    if(error != nullptr)
    {
        g_error_free(error);
        return false;
    }
    if(value == nullptr)
    {
        return false;
    }
    out_secret.assign(value);
    secret_password_free(value);
    return true;
}

bool SecretStore::Erase(const std::string& key)
{
    GError* error = nullptr;
    secret_password_clear_sync(Schema(), nullptr, &error, "key", key.c_str(), nullptr);
    if(error != nullptr)
    {
        g_error_free(error);
        return false;
    }
    return true;
}

#else

// No credential store on this platform/build: report unavailable so callers
// fall back to prompting for the secret instead of persisting it.
bool SecretStore::IsAvailable()
{
    return false;
}

bool SecretStore::Set(const std::string&, const std::string&)
{
    return false;
}

bool SecretStore::Get(const std::string&, std::string& out_secret)
{
    out_secret.clear();
    return false;
}

bool SecretStore::Erase(const std::string&)
{
    return true;
}

#endif

}  // namespace View
}  // namespace RocProfVis
