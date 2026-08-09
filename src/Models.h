#pragma once
#include <string>

// ---------------------------------------------------------------------------
// Customer / Account record
// ---------------------------------------------------------------------------
struct Customer {
    long long   accountNumber = 0;
    std::string name;
    std::string cnic;          // national ID used to verify identity (e.g. for PIN reset)
    std::string phone;
    std::string address;
    double      balance = 0.0;
    std::string pin;           // 4-digit PIN, stored as a string ("0000"-"9999")
    std::string status;        // "Active" | "Inactive" | "Locked"
    std::string createdDate;   // yyyy-mm-dd HH:MM:SS
};

// ---------------------------------------------------------------------------
// Transaction / audit log entry
// ---------------------------------------------------------------------------
struct Transaction {
    long long   accountNumber = 0;
    std::string type;          // "AccountCreated" | "Deposit" | "Withdrawal" |
                                // "PinReset" | "StatusChange" | "Update" | "AccountDeleted"
    double      amount = 0.0;
    double      balanceAfter = 0.0;
    std::string timestamp;
    std::string note;          // free-text detail, e.g. "Status: Active -> Locked"
};
