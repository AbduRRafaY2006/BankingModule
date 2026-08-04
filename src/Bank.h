#pragma once
#include <vector>
#include <string>
#include <optional>
#include "Models.h"
#include "FileManager.h"

struct SummaryReport {
    int    totalAccounts = 0;
    int    activeAccounts = 0;
    int    inactiveAccounts = 0;
    int    lockedAccounts = 0;
    double totalBalance = 0.0;
    int    totalTransactions = 0;
};

// Result type used by mutating operations so the UI can show a clear
// success/failure message without throwing exceptions across layers.
struct OpResult {
    bool ok = false;
    std::string message;
    static OpResult Ok(std::string m = "") { return {true, std::move(m)}; }
    static OpResult Fail(std::string m) { return {false, std::move(m)}; }
};

class Bank {
public:
    explicit Bank(std::string dataDir = "data");

    // ----- Create -----------------------------------------------------------
    // Auto-generates the account number; validates PIN is exactly 4 digits
    // and initial deposit is non-negative.
    OpResult createAccount(const std::string& name, const std::string& cnic,
                            const std::string& phone, const std::string& address,
                            double initialDeposit, const std::string& pin,
                            Customer* out = nullptr);

    // ----- Read / Search ------------------------------------------------------
    std::vector<Customer> allAccounts() const;
    std::optional<Customer> findByAccountNumber(long long accNo) const;
    std::vector<Customer> searchByName(const std::string& query) const; // case-insensitive substring

    // ----- Update -----------------------------------------------------------
    OpResult updateAccount(long long accNo, const std::string& name, const std::string& phone,
                            const std::string& address);

    // ----- Status ------------------------------------------------------------
    OpResult setStatus(long long accNo, const std::string& newStatus); // "Active"/"Inactive"/"Locked"

    // ----- Transactions --------------------------------------------------------
    OpResult deposit(long long accNo, double amount);
    OpResult withdraw(long long accNo, double amount);
    std::vector<Transaction> allTransactions() const;
    std::vector<Transaction> transactionsFor(long long accNo) const;

    // ----- PIN reset (requires identity verification) ---------------------------
    // Verification: caller must supply the account's CNIC as it is on file.
    OpResult resetPin(long long accNo, const std::string& cnicVerification, const std::string& newPin);

    // ----- Delete -----------------------------------------------------------
    // Only accounts with status "Inactive" (i.e. closed) may be deleted.
    OpResult deleteClosedAccount(long long accNo);

    // ----- Reporting ---------------------------------------------------------
    SummaryReport generateSummaryReport() const;

private:
    FileManager fm_;
    static bool isFourDigitPin(const std::string& pin);
};
