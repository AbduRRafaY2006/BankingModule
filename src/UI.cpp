#include "UI.h"
#include "Widgets.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>

using ui::Button;
using ui::Label;
using ui::ListBox;
using ui::TextBox;
using ui::Widget;

namespace {

constexpr float WIN_W = 1150.f;
constexpr float WIN_H = 720.f;
constexpr float SIDEBAR_W = 230.f;

std::string money(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

std::string statusGlyph(const std::string& s) { return "\u25CF " + s; }

sf::Color statusColor(const std::string& s) {
    if (s == "Active") return ui::theme.success;
    if (s == "Locked") return ui::theme.danger;
    return ui::theme.textDim;
}

sf::Font loadFontOrThrow(const std::string& file) {
    sf::Font font;
    for (const std::string& base : {"assets/", "../assets/", "./"}) {
        if (font.loadFromFile(base + file)) return font;
    }
    throw std::runtime_error("Could not load font: " + file + " (looked in assets/, ../assets/, ./)");
}

// Adds a widget to both the owning list (keeps it alive) and the current
// tab's list (used for event dispatch + drawing). Returns a raw pointer.
template <typename T, typename... Args>
T* add(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab, Args&&... args) {
    auto w = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = w.get();
    tab.push_back(raw);
    owner.push_back(std::move(w));
    return raw;
}

}  // namespace

void RunAdminUI(Bank& bank) {
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                             "Admin Dashboard", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Font regular = loadFontOrThrow("DejaVuSans.ttf");
    sf::Font bold = loadFontOrThrow("DejaVuSans-Bold.ttf");

    std::vector<std::unique_ptr<Widget>> owner;  // keeps every widget alive
    int currentTab = 0;
    std::vector<std::string> tabNames = {"Dashboard", "New Account", "Accounts", "Transactions", "Reset PIN", "Delete & Report"};

    // ------------------------------------------------------------------
    // Sidebar navigation buttons
    // ------------------------------------------------------------------
    std::vector<std::unique_ptr<Widget>> sidebarOwner;
    std::vector<Button*> navButtons;
    for (size_t i = 0; i < tabNames.size(); ++i) {
        auto btn = std::make_unique<Button>(regular, tabNames[i], [&currentTab, i] { currentTab = static_cast<int>(i); },
                                             ui::theme.sidebarBg, ui::theme.textLight);
        btn->setPosition(15.f, 90.f + i * 46.f);
        btn->setSize(SIDEBAR_W - 30.f, 38.f);
        navButtons.push_back(btn.get());
        sidebarOwner.push_back(std::move(btn));
    }

    // ==================================================================
    // TAB 0: Dashboard
    // ==================================================================
    std::vector<Widget*> tabDashboard;
    // Cards are drawn directly (not Widgets) so they can pull live numbers
    // each frame; nothing to add to tabDashboard for interaction.

    // ==================================================================
    // TAB 1: New Account
    // ==================================================================
    std::vector<Widget*> tabNewAccount;
    float x0 = SIDEBAR_W + 40.f;
    float y = 70.f;

    auto naName = add<TextBox>(owner, tabNewAccount, regular, "Full name");
    naName->setPosition(x0, y + 25.f); naName->setSize(400.f, 34.f);
    auto naCnic = add<TextBox>(owner, tabNewAccount, regular, "CNIC / National ID");
    naCnic->setPosition(x0, y + 85.f); naCnic->setSize(400.f, 34.f);
    auto naPhone = add<TextBox>(owner, tabNewAccount, regular, "Phone number");
    naPhone->setPosition(x0, y + 145.f); naPhone->setSize(400.f, 34.f);
    auto naAddress = add<TextBox>(owner, tabNewAccount, regular, "Address");
    naAddress->setPosition(x0, y + 205.f); naAddress->setSize(400.f, 34.f);
    auto naDeposit = add<TextBox>(owner, tabNewAccount, regular, "Initial deposit e.g. 500.00");
    naDeposit->setPosition(x0, y + 265.f); naDeposit->setSize(400.f, 34.f);
    auto naPin = add<TextBox>(owner, tabNewAccount, regular, "4-digit PIN", /*digitsOnly=*/true);
    naPin->setPosition(x0, y + 325.f); naPin->setSize(400.f, 34.f);

    auto naMsg = add<Label>(owner, tabNewAccount, regular, "", 14, ui::theme.success);
    naMsg->setPosition(x0, y + 420.f);

    auto naCreateBtn = add<Button>(owner, tabNewAccount, regular, "Create Account",
        [&bank, naName, naCnic, naPhone, naAddress, naDeposit, naPin, naMsg] {
            double deposit = 0.0;
            try { deposit = std::stod(naDeposit->value().empty() ? "0" : naDeposit->value()); }
            catch (...) { naMsg->setText("Initial deposit must be a valid number."); return; }
            Customer created;
            auto res = bank.createAccount(naName->value(), naCnic->value(), naPhone->value(), naAddress->value(),
                                           deposit, naPin->value(), &created);
            naMsg->setText(res.message);
            if (res.ok) {
                naName->clear(); naCnic->clear(); naPhone->clear();
                naAddress->clear(); naDeposit->clear(); naPin->clear();
            }
        },
        ui::theme.accent, ui::theme.textLight);
    naCreateBtn->setPosition(x0, y + 375.f); naCreateBtn->setSize(180.f, 36.f);

    // ==================================================================
    // TAB 2: Accounts (search / list / update / status / deposit-withdraw)
    // ==================================================================
    std::vector<Widget*> tabAccounts;
    auto accountsData = std::make_shared<std::vector<Customer>>(bank.allAccounts());

    auto formatAccountRow = [](const Customer& c) {
        std::ostringstream row;
        row << std::left << std::setw(12) << c.accountNumber << " | " << std::setw(20) << c.name.substr(0, 20)
            << " | " << std::setw(10) << c.status << " | " << money(c.balance);
        return row.str();
    };

    float ax = SIDEBAR_W + 30.f;
    auto accSearchBox = add<TextBox>(owner, tabAccounts, regular, "Search by name (blank = all)");
    accSearchBox->setPosition(ax, 65.f); accSearchBox->setSize(300.f, 32.f);

    auto accList = add<ListBox>(owner, tabAccounts, regular, 24);
    accList->setPosition(ax, 110.f);
    accList->setSize(430.f, 190.f);

    auto rebuildAccountList = [accountsData, accList, formatAccountRow] {
        std::vector<std::string> rows;
        for (auto& c : *accountsData) rows.push_back(formatAccountRow(c));
        accList->setItems(rows);
    };
    rebuildAccountList();

    auto accSearchBtn = add<Button>(owner, tabAccounts, regular, "Search / Refresh All",
        [&bank, accSearchBox, accountsData, rebuildAccountList] {
            if (accSearchBox->value().empty()) *accountsData = bank.allAccounts();
            else *accountsData = bank.searchByName(accSearchBox->value());
            rebuildAccountList();
        },
        ui::theme.accent, ui::theme.textLight);
    accSearchBtn->setPosition(ax + 310.f, 65.f); accSearchBtn->setSize(140.f, 32.f);

    // Detail label showing the currently selected customer
    auto accDetail = add<Label>(owner, tabAccounts, regular, "Select an account above.", 14, ui::theme.textDark);
    accDetail->setPosition(ax + 470.f, 65.f);

    // Editable fields, prefilled by "Load Selected"
    float ex = ax;
    float ey = 320.f;
    auto accEditName = add<TextBox>(owner, tabAccounts, regular, "New name (blank = unchanged)");
    accEditName->setPosition(ex, ey + 25.f); accEditName->setSize(300.f, 32.f);
    auto accEditPhone = add<TextBox>(owner, tabAccounts, regular, "New phone (blank = unchanged)");
    accEditPhone->setPosition(ex, ey + 65.f); accEditPhone->setSize(300.f, 32.f);
    auto accEditAddress = add<TextBox>(owner, tabAccounts, regular, "New address (blank = unchanged)");
    accEditAddress->setPosition(ex, ey + 105.f); accEditAddress->setSize(300.f, 32.f);

    auto accMsg = add<Label>(owner, tabAccounts, regular, "", 13, ui::theme.success);
    accMsg->setPosition(ex, ey + 320.f);

    // Helper capturing "currently selected Customer*" via list index.
    auto currentSelected = [accountsData, accList]() -> Customer* {
        int idx = accList->selectedIndex();
        if (idx < 0 || idx >= static_cast<int>(accountsData->size())) return nullptr;
        return &(*accountsData)[idx];
    };

    auto accLoadBtn = add<Button>(owner, tabAccounts, regular, "Load Selected Into Editor",
        [currentSelected, accEditName, accEditPhone, accEditAddress, accMsg] {
            auto* c = currentSelected();
            if (!c) { accMsg->setText("No account selected."); return; }
            accEditName->setValue(c->name);
            accEditPhone->setValue(c->phone);
            accEditAddress->setValue(c->address);
            accMsg->setText("Loaded account " + std::to_string(c->accountNumber) + " into editor.");
        },
        ui::theme.warning, sf::Color::Black);
    accLoadBtn->setPosition(ax + 470.f, 230.f); accLoadBtn->setSize(230.f, 32.f);

    auto refreshAfterMutation = [&bank, accountsData, rebuildAccountList] {
        *accountsData = bank.allAccounts();
        rebuildAccountList();
    };

    auto accSaveBtn = add<Button>(owner, tabAccounts, regular, "Save Update",
        [&bank, currentSelected, accEditName, accEditPhone, accEditAddress, accMsg, refreshAfterMutation] {
            auto* c = currentSelected();
            if (!c) { accMsg->setText("No account selected."); return; }
            auto res = bank.updateAccount(c->accountNumber, accEditName->value(), accEditPhone->value(), accEditAddress->value());
            accMsg->setText(res.message);
            if (res.ok) refreshAfterMutation();
        },
        ui::theme.accent, ui::theme.textLight);
    accSaveBtn->setPosition(ex, ey + 145.f); accSaveBtn->setSize(150.f, 32.f);

    auto makeStatusBtn = [&](const std::string& label, const std::string& status, float bx, sf::Color color) {
        return add<Button>(owner, tabAccounts, regular, label,
            [&bank, currentSelected, status, accMsg, refreshAfterMutation] {
                auto* c = currentSelected();
                if (!c) { accMsg->setText("No account selected."); return; }
                auto res = bank.setStatus(c->accountNumber, status);
                accMsg->setText(res.message);
                if (res.ok) refreshAfterMutation();
            },
            color, ui::theme.textLight);
    };
    auto accActivateBtn = makeStatusBtn("Activate", "Active", 0, ui::theme.success);
    accActivateBtn->setPosition(ex, ey + 195.f); accActivateBtn->setSize(95.f, 32.f);
    auto accDeactivateBtn = makeStatusBtn("Deactivate", "Inactive", 0, ui::theme.textDim);
    accDeactivateBtn->setPosition(ex + 105.f, ey + 195.f); accDeactivateBtn->setSize(105.f, 32.f);
    auto accLockBtn = makeStatusBtn("Lock", "Locked", 0, ui::theme.danger);
    accLockBtn->setPosition(ex + 220.f, ey + 195.f); accLockBtn->setSize(80.f, 32.f);

    auto accDepositAmt = add<TextBox>(owner, tabAccounts, regular, "Amount");
    accDepositAmt->setPosition(ax + 470.f, ey + 25.f); accDepositAmt->setSize(140.f, 32.f);
    auto accDepositBtn = add<Button>(owner, tabAccounts, regular, "Deposit",
        [&bank, currentSelected, accDepositAmt, accMsg, refreshAfterMutation] {
            auto* c = currentSelected();
            if (!c) { accMsg->setText("No account selected."); return; }
            double amt = 0;
            try { amt = std::stod(accDepositAmt->value()); } catch (...) { accMsg->setText("Invalid amount."); return; }
            auto res = bank.deposit(c->accountNumber, amt);
            accMsg->setText(res.message);
            if (res.ok) { refreshAfterMutation(); accDepositAmt->clear(); }
        },
        ui::theme.success, ui::theme.textLight);
    accDepositBtn->setPosition(ax + 620.f, ey + 25.f); accDepositBtn->setSize(110.f, 32.f);

    auto accWithdrawAmt = add<TextBox>(owner, tabAccounts, regular, "Amount");
    accWithdrawAmt->setPosition(ax + 470.f, ey + 65.f); accWithdrawAmt->setSize(140.f, 32.f);
    auto accWithdrawBtn = add<Button>(owner, tabAccounts, regular, "Withdraw",
        [&bank, currentSelected, accWithdrawAmt, accMsg, refreshAfterMutation] {
            auto* c = currentSelected();
            if (!c) { accMsg->setText("No account selected."); return; }
            double amt = 0;
            try { amt = std::stod(accWithdrawAmt->value()); } catch (...) { accMsg->setText("Invalid amount."); return; }
            auto res = bank.withdraw(c->accountNumber, amt);
            accMsg->setText(res.message);
            if (res.ok) { refreshAfterMutation(); accWithdrawAmt->clear(); }
        },
        ui::theme.danger, ui::theme.textLight);
    accWithdrawBtn->setPosition(ax + 620.f, ey + 65.f); accWithdrawBtn->setSize(110.f, 32.f);

    // ==================================================================
    // TAB 3: Transactions
    // ==================================================================
    std::vector<Widget*> tabTransactions;
    auto txData = std::make_shared<std::vector<Transaction>>(bank.allTransactions());

    auto txList = add<ListBox>(owner, tabTransactions, regular, 24);
    txList->setPosition(SIDEBAR_W + 30.f, 110.f);
    txList->setSize(WIN_W - SIDEBAR_W - 60.f, 500.f);

    auto rebuildTxList = [txData, txList] {
        std::vector<std::string> rows;
        for (auto& t : *txData) {
            std::ostringstream row;
            row << std::left << std::setw(20) << t.timestamp << " | " << std::setw(11) << t.accountNumber << " | "
                << std::setw(15) << t.type << " | " << std::setw(10) << money(t.amount) << " | bal: " << money(t.balanceAfter);
            rows.push_back(row.str());
        }
        txList->setItems(rows);
    };
    rebuildTxList();

    auto txFilterBox = add<TextBox>(owner, tabTransactions, regular, "Account # (blank = all)", true);
    txFilterBox->setPosition(SIDEBAR_W + 30.f, 65.f); txFilterBox->setSize(220.f, 32.f);

    auto txFilterBtn = add<Button>(owner, tabTransactions, regular, "Filter by Account #",
        [&bank, txFilterBox, txData, rebuildTxList] {
            try {
                long long accNo = std::stoll(txFilterBox->value());
                *txData = bank.transactionsFor(accNo);
            } catch (...) { *txData = {}; }
            rebuildTxList();
        },
        ui::theme.accent, ui::theme.textLight);
    txFilterBtn->setPosition(SIDEBAR_W + 260.f, 65.f); txFilterBtn->setSize(180.f, 32.f);

    auto txAllBtn = add<Button>(owner, tabTransactions, regular, "Show All",
        [&bank, txData, rebuildTxList] { *txData = bank.allTransactions(); rebuildTxList(); },
        ui::theme.textDim, ui::theme.textLight);
    txAllBtn->setPosition(SIDEBAR_W + 450.f, 65.f); txAllBtn->setSize(110.f, 32.f);

    // ==================================================================
    // TAB 4: Reset PIN
    // ==================================================================
    std::vector<Widget*> tabResetPin;
    float rx = SIDEBAR_W + 40.f;
    auto rpAcc = add<TextBox>(owner, tabResetPin, regular, "Account number", true);
    rpAcc->setPosition(rx, 95.f); rpAcc->setSize(360.f, 34.f);
    auto rpCnic = add<TextBox>(owner, tabResetPin, regular, "CNIC on file (identity verification)");
    rpCnic->setPosition(rx, 155.f); rpCnic->setSize(360.f, 34.f);
    auto rpNewPin = add<TextBox>(owner, tabResetPin, regular, "New 4-digit PIN", true);
    rpNewPin->setPosition(rx, 215.f); rpNewPin->setSize(360.f, 34.f);

    auto rpMsg = add<Label>(owner, tabResetPin, regular, "", 14, ui::theme.success);
    rpMsg->setPosition(rx, 320.f);

    auto rpBtn = add<Button>(owner, tabResetPin, regular, "Reset PIN",
        [&bank, rpAcc, rpCnic, rpNewPin, rpMsg] {
            long long accNo = 0;
            try { accNo = std::stoll(rpAcc->value()); } catch (...) { rpMsg->setText("Invalid account number."); return; }
            auto res = bank.resetPin(accNo, rpCnic->value(), rpNewPin->value());
            rpMsg->setText(res.message);
            if (res.ok) { rpAcc->clear(); rpCnic->clear(); rpNewPin->clear(); }
        },
        ui::theme.accent, ui::theme.textLight);
    rpBtn->setPosition(rx, 265.f); rpBtn->setSize(180.f, 36.f);

    // ==================================================================
    // TAB 5: Delete & Report
    // ==================================================================
    std::vector<Widget*> tabDelete;
    float dx = SIDEBAR_W + 40.f;
    auto delAcc = add<TextBox>(owner, tabDelete, regular, "Account number (must be Inactive)", true);
    delAcc->setPosition(dx, 95.f); delAcc->setSize(360.f, 34.f);

    auto delMsg = add<Label>(owner, tabDelete, regular, "", 14, ui::theme.success);
    delMsg->setPosition(dx, 175.f);

    auto delBtn = add<Button>(owner, tabDelete, regular, "Delete Closed Account",
        [&bank, delAcc, delMsg] {
            long long accNo = 0;
            try { accNo = std::stoll(delAcc->value()); } catch (...) { delMsg->setText("Invalid account number."); return; }
            auto res = bank.deleteClosedAccount(accNo);
            delMsg->setText(res.message);
            if (res.ok) delAcc->clear();
        },
        ui::theme.danger, ui::theme.textLight);
    delBtn->setPosition(dx, 140.f); delBtn->setSize(220.f, 32.f);

    // ------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------
    std::vector<std::vector<Widget*>*> tabs = {&tabDashboard, &tabNewAccount, &tabAccounts,
                                                &tabTransactions, &tabResetPin, &tabDelete};

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();

            for (auto* btn : navButtons) btn->handleEvent(event, window);
            for (auto* w : *tabs[currentTab]) w->handleEvent(event, window);
        }

        window.clear(ui::theme.contentBg);

        // Sidebar
        {
            sf::RectangleShape sidebar({SIDEBAR_W, WIN_H});
            sidebar.setFillColor(ui::theme.sidebarBg);
            window.draw(sidebar);

            sf::Text title("ADMIN DASHBOARD", bold, 18);
            title.setFillColor(ui::theme.textLight);
            title.setPosition(15.f, 30.f);
            window.draw(title);

            for (size_t i = 0; i < navButtons.size(); ++i) {
                auto* btn = navButtons[i];
                if (static_cast<int>(i) == currentTab) {
                    sf::RectangleShape hi(sf::Vector2f(btn->bounds().width, btn->bounds().height));
                    hi.setPosition(btn->bounds().left, btn->bounds().top);
                    hi.setFillColor(ui::theme.sidebarSelected);
                    window.draw(hi);
                }
                sf::Text label(tabNames[i], regular, 15);
                label.setFillColor(ui::theme.textLight);
                label.setPosition(btn->bounds().left + 14.f, btn->bounds().top + 9.f);
                window.draw(label);
                // Re-run hover detection (draw() on Button expects to draw its own bg;
                // we already drew the highlight above, so just draw label — skip Button::draw).
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                (void)mouse;
            }

            sf::Text quit("Esc or window close to quit", regular, 11);
            quit.setFillColor(sf::Color(200, 210, 230));
            quit.setPosition(15.f, WIN_H - 30.f);
            window.draw(quit);
        }

        // Content area title
        {
            sf::Text screenTitle(tabNames[currentTab], bold, 22);
            screenTitle.setFillColor(ui::theme.textDark);
            screenTitle.setPosition(SIDEBAR_W + 30.f, 20.f);
            window.draw(screenTitle);
        }

        if (currentTab == 0) {
            auto r = bank.generateSummaryReport();
            float cx = SIDEBAR_W + 30.f, cy = 70.f, cw = 270.f, ch = 90.f, gap = 20.f;
            ui::drawCard(window, regular, {cx, cy, cw, ch}, "Total Accounts", std::to_string(r.totalAccounts), ui::theme.accent);
            ui::drawCard(window, regular, {cx + cw + gap, cy, cw, ch}, "Total Balance", money(r.totalBalance), ui::theme.success);
            ui::drawCard(window, regular, {cx + 2 * (cw + gap), cy, cw, ch}, "Total Transactions", std::to_string(r.totalTransactions), sf::Color(150, 90, 200));
            ui::drawCard(window, regular, {cx, cy + ch + gap, cw, ch}, "Active", std::to_string(r.activeAccounts), ui::theme.success);
            ui::drawCard(window, regular, {cx + cw + gap, cy + ch + gap, cw, ch}, "Inactive", std::to_string(r.inactiveAccounts), ui::theme.textDim);
            ui::drawCard(window, regular, {cx + 2 * (cw + gap), cy + ch + gap, cw, ch}, "Locked", std::to_string(r.lockedAccounts), ui::theme.danger);
        } else if (currentTab == 2) {
            // Static labels for accounts tab (column header + selected detail box)
            sf::Text hdr("Account #    | Name                 | Status     | Balance", regular, 12);
            hdr.setFillColor(ui::theme.textDim);
            hdr.setPosition(ax, 95.f);
            window.draw(hdr);

            // Selected-account detail panel
            auto* c = currentSelected();
            ui::drawPanelBackground(window, {ax + 470.f, 65.f, WIN_W - (ax + 470.f) - 20.f, 150.f}, ui::theme.cardBg, ui::theme.border);
            if (c) {
                std::ostringstream d;
                d << "Selected: #" << c->accountNumber << "  " << c->name << "\n"
                  << "Status: " << c->status << "   Balance: " << money(c->balance) << "\n"
                  << "Phone: " << c->phone << "\n"
                  << "Address: " << c->address << "\n"
                  << "Created: " << c->createdDate;
                accDetail->setText(d.str());
            } else {
                accDetail->setText("Select an account from the list, then click\n'Load Selected Into Editor' to edit it.");
            }
            accDetail->setPosition(ax + 480.f, 75.f);
        } else if (currentTab == 5) {
            auto r = bank.generateSummaryReport();
            std::ostringstream rep;
            rep << "Summary Report\n\n"
                << "Total accounts:      " << r.totalAccounts << "\n"
                << "  Active:            " << r.activeAccounts << "\n"
                << "  Inactive:          " << r.inactiveAccounts << "\n"
                << "  Locked:            " << r.lockedAccounts << "\n"
                << "Total balance held:  " << money(r.totalBalance) << "\n"
                << "Total transactions:  " << r.totalTransactions;
            sf::Text repText(rep.str(), regular, 15);
            repText.setFillColor(ui::theme.textDark);
            repText.setPosition(dx, 230.f);
            window.draw(repText);
        }

        for (auto* w : *tabs[currentTab]) w->draw(window);

        window.display();
    }
}
