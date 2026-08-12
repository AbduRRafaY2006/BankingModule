#include "AdminAuth.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdint>

namespace fs = std::filesystem;

namespace {

// Fixed pepper mixed into every hash so the stored value isn't a bare,
// rainbow-table-able hash of the password alone. Not meant to be
// cryptographic-grade security -- just keeps the password out of plain
// text on disk, matching the spirit of the rest of this file-based project.
constexpr char PEPPER[] = "BankATMSystem::admin::v1";

// Small deterministic FNV-1a 64-bit hash. No external dependencies needed,
// and (unlike std::hash) its output is stable across builds/runs, so a
// credentials file written today still verifies correctly tomorrow.
std::string hashPassword(const std::string& password) {
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
    auto feed = [&hash](const std::string& s) {
        for (unsigned char c : s) {
            hash ^= c;
            hash *= 1099511628211ULL;  // FNV prime
        }
    };
    feed(PEPPER);
    feed(password);
    feed(PEPPER);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

std::string credentialsPath(const std::string& dataDir) {
    return dataDir + "/admin_credentials.dat";
}

}  // namespace

namespace AdminAuth {

void ensureCredentialsFile(const std::string& dataDir) {
    if (!fs::exists(dataDir)) {
        fs::create_directories(dataDir);
    }
    const std::string path = credentialsPath(dataDir);
    if (fs::exists(path)) return;

    std::ofstream f(path, std::ios::trunc);
    // Default login: admin / admin123
    // To change the password, delete this file and restart (it will be
    // regenerated with the default), or replace the hash on this line
    // with the output of AdminAuth's hashPassword() for a new password.
    f << "admin|" << hashPassword("admin123") << "\n";
}

bool verify(const std::string& dataDir, const std::string& username, const std::string& password) {
    std::ifstream f(credentialsPath(dataDir));
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto sep = line.find('|');
        if (sep == std::string::npos) continue;

        std::string storedUser = line.substr(0, sep);
        std::string storedHash = line.substr(sep + 1);
        if (storedUser == username && storedHash == hashPassword(password)) {
            return true;
        }
    }
    return false;
}

}  // namespace AdminAuth
