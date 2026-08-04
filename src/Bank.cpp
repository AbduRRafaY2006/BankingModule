#include "Bank.h"
#include <algorithm>
#include <cctype>

Bank::Bank(std::string dataDir) : fm_(std::move(dataDir)) {
    fm_.init();
}

bool Bank::isFourDigitPin(const std::string& pin) {
    if (pin.size() != 4) return false;
    return std::all_of(pin.begin(), pin.end(), [](unsigned char c) { return std::isdigit(c); });
}

OpResult Bank::createAccount(const std::string& name, const std::string& cnic,
                              const std::string& phone, const std::string& address,
                              double initialDeposit, const std::string& pin,
                              Customer* out) {
    if (name.empty()) return OpResult::Fail("Name is required.");
    if (!isFourDigitPin(pin)) return OpResult::Fail("PIN must be exactly 4 digits.");
    if (initialDeposit < 0) return OpResult::Fail("Initial deposit cannot be negative.");

    Customer c;
    c.accountNumber = fm_.nextAccountNumber();
    c.name = name;
    c.cnic = cnic;
    c.phone = phone;
    c.address = address;
    c.balance = initialDeposit;
    c.pin = pin;
    c.status = "Active";
    c.createdDate = FileManager::now();

    auto accounts = fm_.loadAccounts();
    accounts.push_back(c);
    fm_.saveAccounts(accounts);

    Transaction t;
    t.accountNumber = c.accountNumber;
    t.type = "AccountCreated";
    t.amount = initialDeposit;
    t.balanceAfter = initialDeposit;
    t.timestamp = c.createdDate;
    t.note = "Account opened with initial deposit.";
    fm_.appendTransaction(t);

    if (out) *out = c;
    return OpResult::Ok("Account " + std::to_string(c.accountNumber) + " created successfully.");
}

std::vector<Customer> Bank::allAccounts() const {
    return fm_.loadAccounts();
}

std::optional<Customer> Bank::findByAccountNumber(long long accNo) const {
    auto accounts = fm_.loadAccounts();
    for (auto& c : accounts) if (c.accountNumber == accNo) return c;
    return std::nullopt;
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    return r;
}

std::vector<Customer> Bank::searchByName(const std::string& query) const {
    auto accounts = fm_.loadAccounts();
    std::vector<Customer> out;
    std::string q = toLower(query);
    for (auto& c : accounts) {
        if (toLower(c.name).find(q) != std::string::npos) out.push_back(c);
    }
    return out;
}

OpResult Bank::updateAccount(long long accNo, const std::string& name, const std::string& phone,
                              const std::string& address) {
    auto accounts = fm_.loadAccounts();
    for (auto& c : accounts) {
        if (c.accountNumber == accNo) {
            if (!name.empty()) c.name = name;
            if (!phone.empty()) c.phone = phone;
            if (!address.empty()) c.address = address;
            fm_.saveAccounts(accounts);

            Transaction t;
            t.accountNumber = accNo;
            t.type = "Update";
            t.amount = 0;
            t.balanceAfter = c.balance;
            t.timestamp = FileManager::now();
            t.note = "Customer record updated.";
            fm_.appendTransaction(t);
            return OpResult::Ok("Account " + std::to_string(accNo) + " updated.");
        }
    }
    return OpResult::Fail("Account not found.");
}

OpResult Bank::setStatus(long long accNo, const std::string& newStatus) {
    if (newStatus != "Active" && newStatus != "Inactive" && newStatus != "Locked")
        return OpResult::Fail("Invalid status.");
    auto accounts = fm_.loadAccounts();
    for (auto& c : accounts) {
        if (c.accountNumber == accNo) {
            std::string old = c.status;
            c.status = newStatus;
            fm_.saveAccounts(accounts);

            Transaction t;
            t.accountNumber = accNo;
            t.type = "StatusChange";
            t.amount = 0;
            t.balanceAfter = c.balance;
            t.timestamp = FileManager::now();
            t.note = "Status: " + old + " -> " + newStatus;
            fm_.appendTransaction(t);
            return OpResult::Ok("Account " + std::to_string(accNo) + " is now " + newStatus + ".");
        }
    }
    return OpResult::Fail("Account not found.");
}

OpResult Bank::deposit(long long accNo, double amount) {
    if (amount <= 0) return OpResult::Fail("Deposit amount must be positive.");
    auto accounts = fm_.loadAccounts();
    for (auto& c : accounts) {
        if (c.accountNumber == accNo) {
            if (c.status != "Active") return OpResult::Fail("Account is not active.");
            c.balance += amount;
            fm_.saveAccounts(accounts);

            Transaction t;
            t.accountNumber = accNo;
            t.type = "Deposit";
            t.amount = amount;
            t.balanceAfter = c.balance;
            t.timestamp = FileManager::now();
            fm_.appendTransaction(t);
            return OpResult::Ok("Deposited " + std::to_string(amount) + " to account " + std::to_string(accNo) + ".");
        }
    }
    return OpResult::Fail("Account not found.");
}

OpResult Bank::withdraw(long long accNo, double amount) {
    if (amount <= 0) return OpResult::Fail("Withdrawal amount must be positive.");
    auto accounts = fm_.loadAccounts();
    for (auto& c : accounts) {
        if (c.accountNumber == accNo) {
            if (c.status != "Active") return OpResult::Fail("Account is not active.");
            if (c.balance < amount) return OpResult::Fail("Insufficient balance.");
            c.balance -= amount;
            fm_.saveAccounts(accounts);

            Transaction t;
            t.accountNumber = accNo;
            t.type = "Withdrawal";
            t.amount = amount;
            t.balanceAfter = c.balance;
            t.timestamp = FileManager::now();
            fm_.appendTransaction(t);
            return OpResult::Ok("Withdrew " + std::to_string(amount) + " from account " + std::to_string(accNo) + ".");
        }
    }
    return OpResult::Fail("Account not found.");
}

std::vector<Transaction> Bank::allTransactions() const {
    return fm_.loadTransactions();
}

std::vector<Transaction> Bank::transactionsFor(long long accNo) const {
    return fm_.loadTransactionsFor(accNo);
}

OpResult Bank::resetPin(long long accNo, const std::string& cnicVerification, const std::string& newPin) {
    if (!isFourDigitPin(newPin)) return OpResult::Fail("New PIN must be exactly 4 digits.");
    auto accounts = fm_.loadAccounts();
    for (auto& c : accounts) {
        if (c.accountNumber == accNo) {
            if (c.cnic.empty() || c.cnic != cnicVerification)
                return OpResult::Fail("Identity verification failed: CNIC does not match record.");
            c.pin = newPin;
            fm_.saveAccounts(accounts);

            Transaction t;
            t.accountNumber = accNo;
            t.type = "PinReset";
            t.amount = 0;
            t.balanceAfter = c.balance;
            t.timestamp = FileManager::now();
            t.note = "PIN reset after identity verification.";
            fm_.appendTransaction(t);
            return OpResult::Ok("PIN for account " + std::to_string(accNo) + " has been reset.");
        }
    }
    return OpResult::Fail("Account not found.");
}

OpResult Bank::deleteClosedAccount(long long accNo) {
    auto accounts = fm_.loadAccounts();
    auto it = std::find_if(accounts.begin(), accounts.end(),
                            [&](const Customer& c) { return c.accountNumber == accNo; });
    if (it == accounts.end()) return OpResult::Fail("Account not found.");
    if (it->status != "Inactive")
        return OpResult::Fail("Only an Inactive (closed) account can be deleted. Deactivate it first.");

    Transaction t;
    t.accountNumber = accNo;
    t.type = "AccountDeleted";
    t.amount = 0;
    t.balanceAfter = 0;
    t.timestamp = FileManager::now();
    t.note = "Closed account deleted by admin.";
    fm_.appendTransaction(t);

    accounts.erase(it);
    fm_.saveAccounts(accounts);
    return OpResult::Ok("Account " + std::to_string(accNo) + " deleted.");
}

SummaryReport Bank::generateSummaryReport() const {
    SummaryReport r;
    auto accounts = fm_.loadAccounts();
    r.totalAccounts = static_cast<int>(accounts.size());
    for (auto& c : accounts) {
        r.totalBalance += c.balance;
        if (c.status == "Active") r.activeAccounts++;
        else if (c.status == "Inactive") r.inactiveAccounts++;
        else if (c.status == "Locked") r.lockedAccounts++;
    }
    r.totalTransactions = static_cast<int>(fm_.loadTransactions().size());
    return r;
}
