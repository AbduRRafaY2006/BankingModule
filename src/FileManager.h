#pragma once
#include <vector>
#include <string>
#include "Models.h"

// All persistence is plain-text, pipe-delimited files on disk under dataDir.
//   accounts.dat      -> one Customer per line
//   transactions.dat  -> one Transaction per line (append-only log)
//   counter.dat       -> single integer, the last issued account number
class FileManager {
public:
    explicit FileManager(std::string dataDir = "data");

    // Ensures data directory + files exist.
    void init();

    // ----- Accounts -------------------------------------------------------
    std::vector<Customer> loadAccounts() const;
    void saveAccounts(const std::vector<Customer>& accounts) const; // full rewrite

    // ----- Transactions (append-only) --------------------------------------
    void appendTransaction(const Transaction& t) const;
    std::vector<Transaction> loadTransactions() const;
    std::vector<Transaction> loadTransactionsFor(long long accountNumber) const;

    // ----- Account number generation ---------------------------------------
    long long nextAccountNumber();

    std::string accountsPath() const { return dataDir_ + "/accounts.dat"; }
    std::string transactionsPath() const { return dataDir_ + "/transactions.dat"; }
    std::string counterPath() const { return dataDir_ + "/counter.dat"; }

    static std::string now(); // current timestamp "yyyy-mm-dd HH:MM:SS"

private:
    std::string dataDir_;

    static std::string escape(const std::string& field);   // encode | and \n
    static std::string unescape(const std::string& field);
};
