#include "FileManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace fs = std::filesystem;

FileManager::FileManager(std::string dataDir) : dataDir_(std::move(dataDir)) {}

void FileManager::init() {
    if (!fs::exists(dataDir_)) {
        fs::create_directories(dataDir_);
    }
    if (!fs::exists(accountsPath())) {
        std::ofstream f(accountsPath());
    }
    if (!fs::exists(transactionsPath())) {
        std::ofstream f(transactionsPath());
    }
    if (!fs::exists(counterPath())) {
        std::ofstream f(counterPath());
        f << 100000000; // first generated account number will be 100000001
    }
}

std::string FileManager::now() {
    auto t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Fields are pipe-delimited; escape literal '|' and newlines so records never
// get corrupted by user-entered text (e.g. an address containing '|').
std::string FileManager::escape(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        if (c == '|') out += "\\p";
        else if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string FileManager::unescape(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '\\' && i + 1 < field.size()) {
            char n = field[i + 1];
            if (n == 'p') { out += '|'; ++i; continue; }
            if (n == 'n') { out += '\n'; ++i; continue; }
            if (n == '\\') { out += '\\'; ++i; continue; }
        }
        out += field[i];
    }
    return out;
}

static std::vector<std::string> splitLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '|' && !(i > 0 && line[i - 1] == '\\')) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    fields.push_back(cur);
    return fields;
}

std::vector<Customer> FileManager::loadAccounts() const {
    std::vector<Customer> out;
    std::ifstream f(accountsPath());
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto parts = splitLine(line);
        if (parts.size() < 9) continue;
        Customer c;
        c.accountNumber = std::stoll(parts[0]);
        c.name          = unescape(parts[1]);
        c.cnic          = unescape(parts[2]);
        c.phone         = unescape(parts[3]);
        c.address       = unescape(parts[4]);
        c.balance       = std::stod(parts[5]);
        c.pin           = unescape(parts[6]);
        c.status        = unescape(parts[7]);
        c.createdDate   = unescape(parts[8]);
        out.push_back(c);
    }
    return out;
}

void FileManager::saveAccounts(const std::vector<Customer>& accounts) const {
    std::ofstream f(accountsPath(), std::ios::trunc);
    for (const auto& c : accounts) {
        f << c.accountNumber << '|'
          << escape(c.name) << '|'
          << escape(c.cnic) << '|'
          << escape(c.phone) << '|'
          << escape(c.address) << '|'
          << c.balance << '|'
          << escape(c.pin) << '|'
          << escape(c.status) << '|'
          << escape(c.createdDate) << '\n';
    }
}

void FileManager::appendTransaction(const Transaction& t) const {
    std::ofstream f(transactionsPath(), std::ios::app);
    f << t.accountNumber << '|'
      << escape(t.type) << '|'
      << t.amount << '|'
      << t.balanceAfter << '|'
      << escape(t.timestamp) << '|'
      << escape(t.note) << '\n';
}

std::vector<Transaction> FileManager::loadTransactions() const {
    std::vector<Transaction> out;
    std::ifstream f(transactionsPath());
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto parts = splitLine(line);
        if (parts.size() < 6) continue;
        Transaction t;
        t.accountNumber = std::stoll(parts[0]);
        t.type          = unescape(parts[1]);
        t.amount        = std::stod(parts[2]);
        t.balanceAfter  = std::stod(parts[3]);
        t.timestamp     = unescape(parts[4]);
        t.note          = unescape(parts[5]);
        out.push_back(t);
    }
    return out;
}

std::vector<Transaction> FileManager::loadTransactionsFor(long long accountNumber) const {
    auto all = loadTransactions();
    std::vector<Transaction> out;
    for (auto& t : all) if (t.accountNumber == accountNumber) out.push_back(t);
    return out;
}

long long FileManager::nextAccountNumber() {
    long long last = 100000000;
    {
        std::ifstream f(counterPath());
        if (f) f >> last;
    }
    long long next = last + 1;
    std::ofstream f(counterPath(), std::ios::trunc);
    f << next;
    return next;
}
