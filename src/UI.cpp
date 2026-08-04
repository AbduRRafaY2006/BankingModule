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
using ui::Table;
using ui::TableColumn;
using ui::TextBox;
using ui::Widget;

namespace {

// ---------------------------------------------------------------------------
// Layout constants — one place to tune spacing instead of magic numbers
// scattered through the file.
// ---------------------------------------------------------------------------
constexpr float WIN_W = 1200.f;
constexpr float WIN_H = 740.f;
constexpr float SIDEBAR_W = 240.f;

constexpr float PAGE_PAD = 32.f;      // left margin of the content area
constexpr float PAGE_TOP = 78.f;      // where content starts, under the screen title
constexpr float FIELD_W = 400.f;
constexpr float FIELD_H = 40.f;
constexpr float FIELD_GAP = 58.f;     // vertical distance between stacked fields

std::string money(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

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

    // NOTE ON FONTS: DejaVu Sans is what's giving this the "not like a real
    // website" look — it's a solid open font but it's not what any modern
    // product UI actually ships. Drop a nicer variable/static font (e.g.
    // "Inter", "Poppins", or "Manrope" — all free, all on Google Fonts) into
    // assets/ as InterRegular.ttf / InterBold.ttf and just change the two
    // filenames below. No other code needs to change.
    sf::Font regular = loadFontOrThrow("DejaVuSans.ttf");
    sf::Font bold = loadFontOrThrow("DejaVuSans-Bold.ttf");

    std::vector<std::unique_ptr<Widget>> owner;
    int currentTab = 0;
    std::vector<std::string> tabNames = {"Dashboard", "New Account", "Accounts", "Transactions", "Reset PIN", "Delete & Report"};

    // ------------------------------------------------------------------
    // Sidebar navigation — rounded selection pill + hover highlight
    // ------------------------------------------------------------------
    std::vector<std::unique_ptr<Widget>> sidebarOwner;
    std::vector<Button*> navButtons;
    for (size_t i = 0; i < tabNames.size(); ++i) {
        auto btn = std::make_unique<Button>(regular, tabNames[i], [&currentTab, i] { currentTab = static_cast<int>(i); },
                                             ui::theme.sidebarBg, ui::theme.textLight);
        btn->setPosition(16.f, 96.f + i * 48.f);
        btn->setSize(SIDEBAR_W - 32.f, 40.f);
        navButtons.push_back(btn.get());
        sidebarOwner.push_back(std::move(btn));
    }

    // ==================================================================
    // TAB 0: Dashboard
    // ==================================================================
    std::vector<Widget*> tabDashboard;

    // ==================================================================
    // TAB 1: New Account
    // ==================================================================
    std::vector<Widget*> tabNewAccount;
    float x0 = SIDEBAR_W + PAGE_PAD;
    float y = PAGE_TOP;

    auto naName = add<TextBox>(owner, tabNewAccount, regular, "Full name");
    naName->setPosition(x0, y); naName->setSize(FIELD_W, FIELD_H);
    auto naCnic = add<TextBox>(owner, tabNewAccount, regular, "CNIC / National ID");
    naCnic->setPosition(x0, y + FIELD_GAP); naCnic->setSize(FIELD_W, FIELD_H);
    auto naPhone = add<TextBox>(owner, tabNewAccount, regular, "Phone number");
    naPhone->setPosition(x0, y + FIELD_GAP * 2); naPhone->setSize(FIELD_W, FIELD_H);
    auto naAddress = add<TextBox>(owner, tabNewAccount, regular, "Address");
    naAddress->setPosition(x0, y + FIELD_GAP * 3); naAddress->setSize(FIELD_W, FIELD_H);
    auto naDeposit = add<TextBox>(owner, tabNewAccount, regular, "Initial deposit e.g. 500.00");
    naDeposit->setPosition(x0, y + FIELD_GAP * 4); naDeposit->setSize(FIELD_W, FIELD_H);
    auto naPin = add<TextBox>(owner, tabNewAccount, regular, "4-digit PIN", /*digitsOnly=*/true);
    naPin->setPosition(x0, y + FIELD_GAP * 5); naPin->setSize(FIELD_W, FIELD_H);

    auto naMsg = add<Label>(owner, tabNewAccount, regular, "", 14, ui::theme.success);
    naMsg->setPosition(x0, y + FIELD_GAP * 6 + 50.f);

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
    naCreateBtn->setPosition(x0, y + FIELD_GAP * 6);
    naCreateBtn->setSize(190.f, 42.f);

    // ==================================================================
    // TAB 2: Accounts (search / list / update / status / deposit-withdraw)
    // ==================================================================
    std::vector<Widget*> tabAccounts;
    auto accountsData = std::make_shared<std::vector<Customer>>(bank.allAccounts());

    auto buildAccountRow = [](const Customer& c) -> std::vector<std::string> {
        return { std::to_string(c.accountNumber), c.name, c.status, money(c.balance) };
    };

    float ax = SIDEBAR_W + PAGE_PAD;
    auto accSearchBox = add<TextBox>(owner, tabAccounts, regular, "Search by name (blank = all)");
    accSearchBox->setPosition(ax, 68.f); accSearchBox->setSize(300.f, 38.f);

    auto accTable = add<Table>(owner, tabAccounts, regular, bold,
        std::vector<TableColumn>{
            { "ACCOUNT #", 100.f, false },
            { "NAME",      190.f, false },
            { "STATUS",    100.f, false },
            { "BALANCE",   110.f, true  },
        }, 34);
    accTable->setPosition(ax, 116.f);
    accTable->setSize(500.f, 200.f);

    auto rebuildAccountList = [accountsData, accTable, buildAccountRow] {
        std::vector<std::vector<std::string>> rows;
        std::vector<sf::Color> accents;
        for (auto& c : *accountsData) {
            rows.push_back(buildAccountRow(c));
            accents.push_back(sf::Color::Transparent);
        }
        accTable->setRows(rows, accents);
    };
    rebuildAccountList();

    auto accSearchBtn = add<Button>(owner, tabAccounts, regular, "Search / Refresh",
        [&bank, accSearchBox, accountsData, rebuildAccountList] {
            if (accSearchBox->value().empty()) *accountsData = bank.allAccounts();
            else *accountsData = bank.searchByName(accSearchBox->value());
            rebuildAccountList();
        },
        ui::theme.accent, ui::theme.textLight);
    accSearchBtn->setPosition(ax + 316.f, 68.f); accSearchBtn->setSize(160.f, 38.f);

    auto accDetail = add<Label>(owner, tabAccounts, regular, "Select an account above.", 14, ui::theme.textDark);
    accDetail->setPosition(ax + 540.f, 78.f);

    float ex = ax;
    float ey = 350.f;
    auto accEditName = add<TextBox>(owner, tabAccounts, regular, "New name (blank = unchanged)");
    accEditName->setPosition(ex, ey); accEditName->setSize(320.f, 36.f);
    auto accEditPhone = add<TextBox>(owner, tabAccounts, regular, "New phone (blank = unchanged)");
    accEditPhone->setPosition(ex, ey + 48.f); accEditPhone->setSize(320.f, 36.f);
    auto accEditAddress = add<TextBox>(owner, tabAccounts, regular, "New address (blank = unchanged)");
    accEditAddress->setPosition(ex, ey + 96.f); accEditAddress->setSize(320.f, 36.f);

    auto accMsg = add<Label>(owner, tabAccounts, regular, "", 13, ui::theme.success);
    accMsg->setPosition(ex, ey + 260.f);

    auto currentSelected = [accountsData, accTable]() -> Customer* {
        int idx = accTable->selectedIndex();
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
    accLoadBtn->setPosition(ax + 540.f, 250.f); accLoadBtn->setSize(240.f, 38.f);

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
    accSaveBtn->setPosition(ex, ey + 144.f); accSaveBtn->setSize(160.f, 38.f);

    auto makeStatusBtn = [&](const std::string& label, const std::string& status, sf::Color color) {
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
    auto accActivateBtn = makeStatusBtn("Activate", "Active", ui::theme.success);
    accActivateBtn->setPosition(ex, ey + 196.f); accActivateBtn->setSize(100.f, 36.f);
    auto accDeactivateBtn = makeStatusBtn("Deactivate", "Inactive", ui::theme.textDim);
    accDeactivateBtn->setPosition(ex + 112.f, ey + 196.f); accDeactivateBtn->setSize(112.f, 36.f);
    auto accLockBtn = makeStatusBtn("Lock", "Locked", ui::theme.danger);
    accLockBtn->setPosition(ex + 236.f, ey + 196.f); accLockBtn->setSize(84.f, 36.f);

    auto accDepositAmt = add<TextBox>(owner, tabAccounts, regular, "Amount");
    accDepositAmt->setPosition(ax + 540.f, ey); accDepositAmt->setSize(150.f, 36.f);
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
    accDepositBtn->setPosition(ax + 700.f, ey); accDepositBtn->setSize(120.f, 36.f);

    auto accWithdrawAmt = add<TextBox>(owner, tabAccounts, regular, "Amount");
    accWithdrawAmt->setPosition(ax + 540.f, ey + 48.f); accWithdrawAmt->setSize(150.f, 36.f);
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
    accWithdrawBtn->setPosition(ax + 700.f, ey + 48.f); accWithdrawBtn->setSize(120.f, 36.f);

    // ==================================================================
    // TAB 3: Transactions
    // ==================================================================
    std::vector<Widget*> tabTransactions;
    auto txData = std::make_shared<std::vector<Transaction>>(bank.allTransactions());

    auto txTable = add<Table>(owner, tabTransactions, regular, bold,
        std::vector<TableColumn>{
            { "TIMESTAMP",  190.f, false },
            { "ACCOUNT #",  110.f, false },
            { "TYPE",       150.f, false },
            { "AMOUNT",     130.f, true  },
            { "BALANCE",    130.f, true  },
        }, 32);
    txTable->setPosition(SIDEBAR_W + PAGE_PAD, 120.f);
    txTable->setSize(WIN_W - SIDEBAR_W - PAGE_PAD * 2, WIN_H - 160.f);

    auto rebuildTxList = [txData, txTable] {
        std::vector<std::vector<std::string>> rows;
        for (auto& t : *txData) {
            rows.push_back({ t.timestamp, std::to_string(t.accountNumber), t.type, money(t.amount), money(t.balanceAfter) });
        }
        txTable->setRows(rows);
    };
    rebuildTxList();

    auto txFilterBox = add<TextBox>(owner, tabTransactions, regular, "Account # (blank = all)", true);
    txFilterBox->setPosition(SIDEBAR_W + PAGE_PAD, 68.f); txFilterBox->setSize(220.f, 38.f);

    auto txFilterBtn = add<Button>(owner, tabTransactions, regular, "Filter by Account #",
        [&bank, txFilterBox, txData, rebuildTxList] {
            try {
                long long accNo = std::stoll(txFilterBox->value());
                *txData = bank.transactionsFor(accNo);
            } catch (...) { *txData = {}; }
            rebuildTxList();
        },
        ui::theme.accent, ui::theme.textLight);
    txFilterBtn->setPosition(SIDEBAR_W + PAGE_PAD + 236.f, 68.f); txFilterBtn->setSize(190.f, 38.f);

    auto txAllBtn = add<Button>(owner, tabTransactions, regular, "Show All",
        [&bank, txData, rebuildTxList] { *txData = bank.allTransactions(); rebuildTxList(); },
        ui::theme.textDim, ui::theme.textLight);
    txAllBtn->setPosition(SIDEBAR_W + PAGE_PAD + 442.f, 68.f); txAllBtn->setSize(120.f, 38.f);

    // ==================================================================
    // TAB 4: Reset PIN
    // ==================================================================
    std::vector<Widget*> tabResetPin;
    float rx = SIDEBAR_W + PAGE_PAD;
    auto rpAcc = add<TextBox>(owner, tabResetPin, regular, "Account number", true);
    rpAcc->setPosition(rx, PAGE_TOP); rpAcc->setSize(380.f, 40.f);
    auto rpCnic = add<TextBox>(owner, tabResetPin, regular, "CNIC on file (identity verification)");
    rpCnic->setPosition(rx, PAGE_TOP + FIELD_GAP); rpCnic->setSize(380.f, 40.f);
    auto rpNewPin = add<TextBox>(owner, tabResetPin, regular, "New 4-digit PIN", true);
    rpNewPin->setPosition(rx, PAGE_TOP + FIELD_GAP * 2); rpNewPin->setSize(380.f, 40.f);

    auto rpMsg = add<Label>(owner, tabResetPin, regular, "", 14, ui::theme.success);
    rpMsg->setPosition(rx, PAGE_TOP + FIELD_GAP * 3 + 46.f);

    auto rpBtn = add<Button>(owner, tabResetPin, regular, "Reset PIN",
        [&bank, rpAcc, rpCnic, rpNewPin, rpMsg] {
            long long accNo = 0;
            try { accNo = std::stoll(rpAcc->value()); } catch (...) { rpMsg->setText("Invalid account number."); return; }
            auto res = bank.resetPin(accNo, rpCnic->value(), rpNewPin->value());
            rpMsg->setText(res.message);
            if (res.ok) { rpAcc->clear(); rpCnic->clear(); rpNewPin->clear(); }
        },
        ui::theme.accent, ui::theme.textLight);
    rpBtn->setPosition(rx, PAGE_TOP + FIELD_GAP * 3); rpBtn->setSize(190.f, 42.f);

    // ==================================================================
    // TAB 5: Delete & Report
    // ==================================================================
    std::vector<Widget*> tabDelete;
    float dx = SIDEBAR_W + PAGE_PAD;
    auto delAcc = add<TextBox>(owner, tabDelete, regular, "Account number (must be Inactive)", true);
    delAcc->setPosition(dx, PAGE_TOP); delAcc->setSize(380.f, 40.f);

    auto delMsg = add<Label>(owner, tabDelete, regular, "", 14, ui::theme.success);
    delMsg->setPosition(dx, PAGE_TOP + FIELD_GAP + 46.f);

    auto delBtn = add<Button>(owner, tabDelete, regular, "Delete Closed Account",
        [&bank, delAcc, delMsg] {
            long long accNo = 0;
            try { accNo = std::stoll(delAcc->value()); } catch (...) { delMsg->setText("Invalid account number."); return; }
            auto res = bank.deleteClosedAccount(accNo);
            delMsg->setText(res.message);
            if (res.ok) delAcc->clear();
        },
        ui::theme.danger, ui::theme.textLight);
    delBtn->setPosition(dx, PAGE_TOP + FIELD_GAP); delBtn->setSize(240.f, 40.f);

    // Report table (replaces the old plain-text ostringstream dump)
    auto reportTable = add<Table>(owner, tabDelete, regular, bold,
        std::vector<TableColumn>{
            { "METRIC", 220.f, false },
            { "VALUE",  140.f, true  },
        }, 34);
    reportTable->setPosition(dx, PAGE_TOP + FIELD_GAP * 2 + 20.f);
    reportTable->setSize(360.f, 34.f * 6 + 34.f);  // header + 6 rows

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

        // ---- Sidebar ----
        {
            sf::RectangleShape sidebar({SIDEBAR_W, WIN_H});
            sidebar.setFillColor(ui::theme.sidebarBg);
            window.draw(sidebar);

            sf::Text title("ADMIN DASHBOARD", bold, 18);
            title.setFillColor(ui::theme.textLight);
            title.setStyle(sf::Text::Bold);
            title.setPosition(20.f, 34.f);
            window.draw(title);

            sf::RectangleShape rule({SIDEBAR_W - 40.f, 1.f});
            rule.setPosition(20.f, 70.f);
            rule.setFillColor(sf::Color(255, 255, 255, 40));
            window.draw(rule);

            for (size_t i = 0; i < navButtons.size(); ++i) {
                auto* btn = navButtons[i];
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                bool hovered = btn->bounds().contains(mouse);
                bool selected = static_cast<int>(i) == currentTab;

                if (selected || hovered) {
                    sf::Color pill = selected ? ui::theme.sidebarSelected : sf::Color(255, 255, 255, 22);
                    ui::drawRoundedRect(window, btn->bounds(), 8.f, pill);
                }
                if (selected) {
                    sf::RectangleShape accent({3.f, btn->bounds().height - 12.f});
                    accent.setPosition(btn->bounds().left - 0.f, btn->bounds().top + 6.f);
                    accent.setFillColor(ui::theme.textLight);
                    window.draw(accent);
                }

                sf::Text label(tabNames[i], regular, 15);
                if (selected) label.setStyle(sf::Text::Bold);
                label.setFillColor(ui::theme.textLight);
                label.setPosition(std::round(btn->bounds().left + 18.f),
                                   std::round(btn->bounds().top + (btn->bounds().height - 18.f) / 2.f));
                window.draw(label);
            }

            sf::Text quit("Esc or window close to quit", regular, 11);
            quit.setFillColor(sf::Color(200, 210, 230));
            quit.setPosition(20.f, WIN_H - 28.f);
            window.draw(quit);
        }

        // ---- Content title ----
        {
            sf::Text screenTitle(tabNames[currentTab], bold, 24);
            screenTitle.setStyle(sf::Text::Bold);
            screenTitle.setFillColor(ui::theme.textDark);
            screenTitle.setPosition(SIDEBAR_W + PAGE_PAD, 24.f);
            window.draw(screenTitle);
        }

        if (currentTab == 0) {
            auto r = bank.generateSummaryReport();
            float cx = SIDEBAR_W + PAGE_PAD, cy = 78.f, cw = 280.f, ch = 100.f, gap = 22.f;
            ui::drawCard(window, regular, {cx, cy, cw, ch}, "TOTAL ACCOUNTS", std::to_string(r.totalAccounts), ui::theme.accent);
            ui::drawCard(window, regular, {cx + cw + gap, cy, cw, ch}, "TOTAL BALANCE", money(r.totalBalance), ui::theme.success);
            ui::drawCard(window, regular, {cx + 2 * (cw + gap), cy, cw, ch}, "TOTAL TRANSACTIONS", std::to_string(r.totalTransactions), sf::Color(150, 90, 200));
            ui::drawCard(window, regular, {cx, cy + ch + gap, cw, ch}, "ACTIVE", std::to_string(r.activeAccounts), ui::theme.success);
            ui::drawCard(window, regular, {cx + cw + gap, cy + ch + gap, cw, ch}, "INACTIVE", std::to_string(r.inactiveAccounts), ui::theme.textDim);
            ui::drawCard(window, regular, {cx + 2 * (cw + gap), cy + ch + gap, cw, ch}, "LOCKED", std::to_string(r.lockedAccounts), ui::theme.danger);
        } else if (currentTab == 2) {
            auto* c = currentSelected();
            ui::drawPanelBackground(window, {ax + 540.f, 68.f, WIN_W - (ax + 540.f) - PAGE_PAD, 160.f}, ui::theme.cardBg, ui::theme.border);
            if (c) {
                sf::Color pillColor = statusColor(c->status);
                std::ostringstream d;
                d << "#" << c->accountNumber << "   " << c->name << "\n\n"
                  << c->status << "   ·   " << money(c->balance) << "\n"
                  << c->phone << "\n"
                  << c->address << "\n"
                  << "Opened " << c->createdDate;
                accDetail->setText(d.str());
                accDetail->setPosition(ax + 556.f, 84.f);
            } else {
                accDetail->setText("Select an account from the table, then click\n'Load Selected Into Editor' to edit it.");
                accDetail->setPosition(ax + 556.f, 84.f);
            }
        } else if (currentTab == 5) {
            auto r = bank.generateSummaryReport();
            std::vector<std::vector<std::string>> rows = {
                { "Total accounts",     std::to_string(r.totalAccounts) },
                { "  Active",           std::to_string(r.activeAccounts) },
                { "  Inactive",         std::to_string(r.inactiveAccounts) },
                { "  Locked",           std::to_string(r.lockedAccounts) },
                { "Total balance held", money(r.totalBalance) },
                { "Total transactions", std::to_string(r.totalTransactions) },
            };
            std::vector<sf::Color> accents = {
                sf::Color::Transparent, ui::theme.success, ui::theme.textDim, ui::theme.danger,
                sf::Color::Transparent, sf::Color::Transparent,
            };
            reportTable->setRows(rows, accents);
        }

        for (auto* w : *tabs[currentTab]) w->draw(window);

        window.display();
    }
}