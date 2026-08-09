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
// Layout constants
// ---------------------------------------------------------------------------
constexpr float WIN_W = 1200.f;
constexpr float WIN_H = 740.f;
constexpr float SIDEBAR_W = 240.f;

constexpr float PAGE_PAD = 32.f;
constexpr float PAGE_TOP = 78.f;
constexpr float FIELD_W = 400.f;
constexpr float FIELD_H = 40.f;
constexpr float FIELD_GAP = 58.f;

std::string money(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
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
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                             "Admin Dashboard", sf::Style::Titlebar | sf::Style::Close, settings);
    window.setFramerateLimit(60);

    // Same font-swap note as before, now actually acted on: DejaVu is the
    // single biggest reason the admin screen read as "not the same app" as
    // the ATM. Drop InterRegular.ttf / InterBold.ttf into assets/ (free,
    // Google Fonts). Falls back to DejaVu automatically if they're missing
    // so this still compiles and runs without the new font files.
    sf::Font regular, bold;
    bool haveInter = true;
    try { regular = loadFontOrThrow("D:\\SummerSem\\FOCP\\Project\\banking_admin_sfml\\assets\\Inter28ptRegular.ttf.ttf"); }
    catch (...) { haveInter = false; regular = loadFontOrThrow("DejaVuSans.ttf"); }
    try { bold = loadFontOrThrow("D:\\SummerSem\\FOCP\\Project\\banking_admin_sfml\\assets\\Inter28ptBold.ttf.ttf"); }
    catch (...) { bold = loadFontOrThrow("DejaVuSans-Bold.ttf"); }
    (void)haveInter;

    std::vector<std::unique_ptr<Widget>> owner;
    int currentTab = 0;
    std::vector<std::string> tabNames = {"Dashboard", "New Account", "Accounts", "Transactions", "Reset PIN", "Delete & Report"};

    // ------------------------------------------------------------------
    // Sidebar navigation
    // ------------------------------------------------------------------
    std::vector<std::unique_ptr<Widget>> sidebarOwner;
    std::vector<Button*> navButtons;
    for (size_t i = 0; i < tabNames.size(); ++i) {
        auto btn = std::make_unique<Button>(regular, "", [&currentTab, i] { currentTab = static_cast<int>(i); },
                                             sf::Color::Transparent, ui::theme.textLight);
        btn->setPosition(14.f, 108.f + i * 50.f);
        btn->setSize(SIDEBAR_W - 28.f, 42.f);
        navButtons.push_back(btn.get());
        sidebarOwner.push_back(std::move(btn));
    }

    // ==================================================================
    // TAB 0: Dashboard
    // ==================================================================
    std::vector<Widget*> tabDashboard;
    // Quick-action shortcuts under the metric cards — jump straight into
    // the task instead of only ever landing on the summary.
    struct QuickAction { std::string label; int targetTab; sf::Color color; };
    std::vector<QuickAction> quickActions = {
        { "+ New Account", 1, ui::theme.accent },
        { "Reset a PIN",   4, ui::theme.warning },
        { "View Accounts", 2, sf::Color(90, 160, 250) },
    };
    {
        float qx = SIDEBAR_W + PAGE_PAD, qy = 470.f;
        for (size_t i = 0; i < quickActions.size(); ++i) {
            auto& qa = quickActions[i];
            auto* btn = add<Button>(owner, tabDashboard, regular, qa.label,
                [&currentTab, tab = qa.targetTab] { currentTab = tab; },
                qa.color, i == 0 ? ui::theme.textDark : ui::theme.textLight);
            btn->setPosition(qx + i * 236.f, qy);
            btn->setSize(220.f, 46.f);
        }
    }

    // ==================================================================
    // TAB 1: New Account
    // ==================================================================
    std::vector<Widget*> tabNewAccount;
    float x0 = SIDEBAR_W + PAGE_PAD + 24.f;
    float y = PAGE_TOP + 40.f;
    const float cardLeft = SIDEBAR_W + PAGE_PAD;
    const float cardW = 470.f;

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
    naMsg->setPosition(x0, y + FIELD_GAP * 6 + 52.f);

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
        ui::theme.accent, ui::theme.textDark);
    naCreateBtn->setPosition(x0, y + FIELD_GAP * 6);
    naCreateBtn->setSize(200.f, 44.f);

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
    accSearchBox->setPosition(ax + 18.f, 92.f); accSearchBox->setSize(280.f, 38.f);

    // STATUS column now renders as a colored pill (Table has native support
    // for this now) instead of plain black text.
    auto accTable = add<Table>(owner, tabAccounts, regular, bold,
        std::vector<TableColumn>{
            { "ACCOUNT #", 100.f, false, false },
            { "NAME",      170.f, false, false },
            { "STATUS",    110.f, false, true  },
            { "BALANCE",   110.f, true,  false },
        }, 36);
    accTable->setPosition(ax + 18.f, 142.f);
    accTable->setSize(490.f, 220.f);

    auto rebuildAccountList = [accountsData, accTable, buildAccountRow] {
        std::vector<std::vector<std::string>> rows;
        std::vector<sf::Color> accents;
        for (auto& c : *accountsData) {
            rows.push_back(buildAccountRow(c));
            accents.push_back(ui::statusColor(c.status));  // thin left accent bar per row
        }
        accTable->setRows(rows, accents);
    };
    rebuildAccountList();

    auto accSearchBtn = add<Button>(owner, tabAccounts, regular, "Search",
        [&bank, accSearchBox, accountsData, rebuildAccountList] {
            if (accSearchBox->value().empty()) *accountsData = bank.allAccounts();
            else *accountsData = bank.searchByName(accSearchBox->value());
            rebuildAccountList();
        },
        ui::theme.accent, ui::theme.textDark);
    accSearchBtn->setPosition(ax + 306.f, 92.f); accSearchBtn->setSize(130.f, 38.f);

    auto accDetail = add<Label>(owner, tabAccounts, regular, "Select an account above.", 14, ui::theme.textDark);
    accDetail->setPosition(ax + 552.f, 100.f);

    float ex = ax + 18.f;
    float ey = 410.f;
    auto accEditName = add<TextBox>(owner, tabAccounts, regular, "New name (blank = unchanged)");
    accEditName->setPosition(ex, ey); accEditName->setSize(300.f, 36.f);
    auto accEditPhone = add<TextBox>(owner, tabAccounts, regular, "New phone (blank = unchanged)");
    accEditPhone->setPosition(ex, ey + 48.f); accEditPhone->setSize(300.f, 36.f);
    auto accEditAddress = add<TextBox>(owner, tabAccounts, regular, "New address (blank = unchanged)");
    accEditAddress->setPosition(ex, ey + 96.f); accEditAddress->setSize(300.f, 36.f);

    auto accMsg = add<Label>(owner, tabAccounts, regular, "", 13, ui::theme.success);
    accMsg->setPosition(ex, ey + 258.f);

    auto currentSelected = [accountsData, accTable]() -> Customer* {
        int idx = accTable->selectedIndex();
        if (idx < 0 || idx >= static_cast<int>(accountsData->size())) return nullptr;
        return &(*accountsData)[idx];
    };

    auto accLoadBtn = add<Button>(owner, tabAccounts, regular, "Load Into Editor",
        [currentSelected, accEditName, accEditPhone, accEditAddress, accMsg] {
            auto* c = currentSelected();
            if (!c) { accMsg->setText("No account selected."); return; }
            accEditName->setValue(c->name);
            accEditPhone->setValue(c->phone);
            accEditAddress->setValue(c->address);
            accMsg->setText("Loaded account " + std::to_string(c->accountNumber) + " into editor.");
        },
        ui::theme.warning, ui::theme.textDark);
    accLoadBtn->setPosition(ax + 552.f, 268.f); accLoadBtn->setSize(220.f, 38.f);

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
        ui::theme.accent, ui::theme.textDark);
    accSaveBtn->setPosition(ex, ey + 144.f); accSaveBtn->setSize(150.f, 38.f);

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
    accActivateBtn->setPosition(ex, ey + 196.f); accActivateBtn->setSize(94.f, 36.f);
    auto accDeactivateBtn = makeStatusBtn("Deactivate", "Inactive", ui::theme.textDim);
    accDeactivateBtn->setPosition(ex + 104.f, ey + 196.f); accDeactivateBtn->setSize(104.f, 36.f);
    auto accLockBtn = makeStatusBtn("Lock", "Locked", ui::theme.danger);
    accLockBtn->setPosition(ex + 218.f, ey + 196.f); accLockBtn->setSize(78.f, 36.f);

    auto accDepositAmt = add<TextBox>(owner, tabAccounts, regular, "Amount");
    accDepositAmt->setPosition(ax + 552.f, ey); accDepositAmt->setSize(140.f, 36.f);
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
    accDepositBtn->setPosition(ax + 700.f, ey); accDepositBtn->setSize(110.f, 36.f);

    auto accWithdrawAmt = add<TextBox>(owner, tabAccounts, regular, "Amount");
    accWithdrawAmt->setPosition(ax + 552.f, ey + 48.f); accWithdrawAmt->setSize(140.f, 36.f);
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
        ui::theme.warning, ui::theme.textDark);
    accWithdrawBtn->setPosition(ax + 700.f, ey + 48.f); accWithdrawBtn->setSize(110.f, 36.f);

    // ==================================================================
    // TAB 3: Transactions
    // ==================================================================
    std::vector<Widget*> tabTransactions;
    auto txData = std::make_shared<std::vector<Transaction>>(bank.allTransactions());

    auto txTable = add<Table>(owner, tabTransactions, regular, bold,
        std::vector<TableColumn>{
            { "TIMESTAMP",  190.f, false, false },
            { "ACCOUNT #",  110.f, false, false },
            { "TYPE",       150.f, false, false },
            { "AMOUNT",     130.f, true,  false },
            { "BALANCE",    130.f, true,  false },
        }, 34);
    txTable->setPosition(SIDEBAR_W + PAGE_PAD, 132.f);
    txTable->setSize(WIN_W - SIDEBAR_W - PAGE_PAD * 2, WIN_H - 172.f);

    auto rebuildTxList = [txData, txTable] {
        std::vector<std::vector<std::string>> rows;
        std::vector<sf::Color> accents;
        for (auto& t : *txData) {
            rows.push_back({ t.timestamp, std::to_string(t.accountNumber), t.type, money(t.amount), money(t.balanceAfter) });
            // Deposit vs. withdrawal gets the same green/orange language the
            // ATM uses for the same actions, via the row's left accent bar.
            if (t.type == "Deposit") accents.push_back(ui::theme.success);
            else if (t.type == "Withdraw" || t.type == "Withdrawal") accents.push_back(ui::theme.warning);
            else accents.push_back(sf::Color::Transparent);
        }
        txTable->setRows(rows, accents);
    };
    rebuildTxList();

    auto txFilterBox = add<TextBox>(owner, tabTransactions, regular, "Account # (blank = all)", true);
    txFilterBox->setPosition(SIDEBAR_W + PAGE_PAD, 82.f); txFilterBox->setSize(220.f, 38.f);

    auto txFilterBtn = add<Button>(owner, tabTransactions, regular, "Filter",
        [&bank, txFilterBox, txData, rebuildTxList] {
            try {
                long long accNo = std::stoll(txFilterBox->value());
                *txData = bank.transactionsFor(accNo);
            } catch (...) { *txData = {}; }
            rebuildTxList();
        },
        ui::theme.accent, ui::theme.textDark);
    txFilterBtn->setPosition(SIDEBAR_W + PAGE_PAD + 236.f, 82.f); txFilterBtn->setSize(120.f, 38.f);

    auto txAllBtn = add<Button>(owner, tabTransactions, regular, "Show All",
        [&bank, txData, rebuildTxList] { *txData = bank.allTransactions(); rebuildTxList(); },
        ui::theme.textDim, ui::theme.textLight);
    txAllBtn->setPosition(SIDEBAR_W + PAGE_PAD + 372.f, 82.f); txAllBtn->setSize(120.f, 38.f);

    // ==================================================================
    // TAB 4: Reset PIN
    // ==================================================================
    std::vector<Widget*> tabResetPin;
    float rx = SIDEBAR_W + PAGE_PAD + 24.f;
    float ry = PAGE_TOP + 40.f;
    auto rpAcc = add<TextBox>(owner, tabResetPin, regular, "Account number", true);
    rpAcc->setPosition(rx, ry); rpAcc->setSize(380.f, 40.f);
    auto rpCnic = add<TextBox>(owner, tabResetPin, regular, "CNIC on file (identity verification)");
    rpCnic->setPosition(rx, ry + FIELD_GAP); rpCnic->setSize(380.f, 40.f);
    auto rpNewPin = add<TextBox>(owner, tabResetPin, regular, "New 4-digit PIN", true);
    rpNewPin->setPosition(rx, ry + FIELD_GAP * 2); rpNewPin->setSize(380.f, 40.f);

    auto rpMsg = add<Label>(owner, tabResetPin, regular, "", 14, ui::theme.success);
    rpMsg->setPosition(rx, ry + FIELD_GAP * 3 + 48.f);

    auto rpBtn = add<Button>(owner, tabResetPin, regular, "Reset PIN",
        [&bank, rpAcc, rpCnic, rpNewPin, rpMsg] {
            long long accNo = 0;
            try { accNo = std::stoll(rpAcc->value()); } catch (...) { rpMsg->setText("Invalid account number."); return; }
            auto res = bank.resetPin(accNo, rpCnic->value(), rpNewPin->value());
            rpMsg->setText(res.message);
            if (res.ok) { rpAcc->clear(); rpCnic->clear(); rpNewPin->clear(); }
        },
        ui::theme.warning, ui::theme.textDark);
    rpBtn->setPosition(rx, ry + FIELD_GAP * 3); rpBtn->setSize(200.f, 44.f);

    // ==================================================================
    // TAB 5: Delete & Report
    // ==================================================================
    std::vector<Widget*> tabDelete;
    float dx = SIDEBAR_W + PAGE_PAD + 24.f;
    float dy = PAGE_TOP + 40.f;
    auto delAcc = add<TextBox>(owner, tabDelete, regular, "Account number (must be Inactive)", true);
    delAcc->setPosition(dx, dy); delAcc->setSize(380.f, 40.f);

    auto delMsg = add<Label>(owner, tabDelete, regular, "", 14, ui::theme.success);
    delMsg->setPosition(dx, dy + FIELD_GAP + 48.f);

    auto delBtn = add<Button>(owner, tabDelete, regular, "Delete Closed Account",
        [&bank, delAcc, delMsg] {
            long long accNo = 0;
            try { accNo = std::stoll(delAcc->value()); } catch (...) { delMsg->setText("Invalid account number."); return; }
            auto res = bank.deleteClosedAccount(accNo);
            delMsg->setText(res.message);
            if (res.ok) delAcc->clear();
        },
        ui::theme.danger, ui::theme.textLight);
    delBtn->setPosition(dx, dy + FIELD_GAP); delBtn->setSize(240.f, 42.f);

    auto reportTable = add<Table>(owner, tabDelete, regular, bold,
        std::vector<TableColumn>{
            { "METRIC", 220.f, false, false },
            { "VALUE",  140.f, true,  false },
        }, 34);
    reportTable->setPosition(SIDEBAR_W + PAGE_PAD * 2 + 420.f, PAGE_TOP + 40.f);
    reportTable->setSize(360.f, 34.f * 6 + 34.f);

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

            // Gold plaque header, echoing the ATM's "BANK ATM" plaque so the
            // sidebar reads as the same hardware family instead of a
            // generic dashboard chrome.
            sf::FloatRect plaque(16.f, 24.f, SIDEBAR_W - 32.f, 46.f);
            ui::drawRoundedRect(window, plaque, 12.f, ui::theme.accent);
            sf::Text title("ADMIN", bold, 18);
            title.setStyle(sf::Text::Bold);
            title.setFillColor(sf::Color(20, 30, 55));
            auto tb = title.getLocalBounds();
            title.setPosition(std::round(plaque.left + (plaque.width - tb.width) / 2.f - tb.left),
                               std::round(plaque.top + (plaque.height - tb.height) / 2.f - tb.top - 2.f));
            window.draw(title);

            sf::Text subtitle("BANK ATM SYSTEM", regular, 10);
            subtitle.setFillColor(sf::Color(150, 170, 205));
            subtitle.setStyle(sf::Text::Bold);
            auto sb = subtitle.getLocalBounds();
            subtitle.setPosition(std::round((SIDEBAR_W - sb.width) / 2.f), 78.f);
            window.draw(subtitle);

            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            for (size_t i = 0; i < navButtons.size(); ++i) {
                auto* btn = navButtons[i];
                bool hovered = btn->bounds().contains(mouse);
                bool selected = static_cast<int>(i) == currentTab;

                if (selected) {
                    sf::FloatRect glow = btn->bounds();
                    glow.left -= 3.f; glow.top -= 3.f; glow.width += 6.f; glow.height += 6.f;
                    ui::drawRoundedRect(window, glow, 11.f, sf::Color(250, 196, 45, 30));
                    ui::drawRoundedRect(window, btn->bounds(), 9.f, ui::theme.sidebarSelected,
                                         ui::theme.accent, 1.4f);
                } else if (hovered) {
                    ui::drawRoundedRect(window, btn->bounds(), 9.f, sf::Color(255, 255, 255, 16));
                }

                sf::Color iconColor = selected ? ui::theme.accent : sf::Color(175, 190, 218);
                ui::drawNavIcon(window,
                                 {btn->bounds().left + 26.f, btn->bounds().top + btn->bounds().height / 2.f},
                                 static_cast<int>(i), iconColor);

                sf::Text label(tabNames[i], regular, 14);
                if (selected) { label.setStyle(sf::Text::Bold); label.setFillColor(ui::theme.textLight); }
                else label.setFillColor(sf::Color(190, 202, 222));
                label.setPosition(std::round(btn->bounds().left + 48.f),
                                   std::round(btn->bounds().top + (btn->bounds().height - 16.f) / 2.f));
                window.draw(label);
            }

            sf::Text quit("Esc or window close to quit", regular, 11);
            quit.setFillColor(sf::Color(140, 155, 185));
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

            sf::RectangleShape underline({36.f, 3.f});
            underline.setPosition(SIDEBAR_W + PAGE_PAD + 1.f, 56.f);
            underline.setFillColor(ui::theme.accent);
            window.draw(underline);
        }

        if (currentTab == 0) {
            auto r = bank.generateSummaryReport();
            float cx = SIDEBAR_W + PAGE_PAD, cy = 82.f, cw = 280.f, ch = 108.f, gap = 22.f;
            ui::drawCard(window, regular, {cx, cy, cw, ch}, "TOTAL ACCOUNTS", std::to_string(r.totalAccounts), ui::theme.accentDim, true);
            ui::drawCard(window, regular, {cx + cw + gap, cy, cw, ch}, "TOTAL BALANCE", money(r.totalBalance), ui::theme.success, true);
            ui::drawCard(window, regular, {cx + 2 * (cw + gap), cy, cw, ch}, "TOTAL TRANSACTIONS", std::to_string(r.totalTransactions), sf::Color(150, 90, 200), true);
            ui::drawCard(window, regular, {cx, cy + ch + gap, cw, ch}, "ACTIVE", std::to_string(r.activeAccounts), ui::theme.success, true);
            ui::drawCard(window, regular, {cx + cw + gap, cy + ch + gap, cw, ch}, "INACTIVE", std::to_string(r.inactiveAccounts), ui::theme.textDim, false);
            ui::drawCard(window, regular, {cx + 2 * (cw + gap), cy + ch + gap, cw, ch}, "LOCKED", std::to_string(r.lockedAccounts), ui::theme.danger, false);

            sf::Text qaLabel("QUICK ACTIONS", bold, 12);
            qaLabel.setStyle(sf::Text::Bold);
            qaLabel.setFillColor(ui::theme.textDim);
            qaLabel.setPosition(cx, cy + 2.f * (ch + gap) + 18.f);
            window.draw(qaLabel);
        } else if (currentTab == 1) {
            ui::drawSectionCard(window, bold, {cardLeft, PAGE_TOP, cardW, FIELD_GAP * 6.f + 96.f},
                                 "New Customer Details", ui::theme.accent);
        } else if (currentTab == 2) {
            ui::drawPanelBackground(window, {ax, 62.f, 526.f, 322.f}, ui::theme.cardBg, ui::theme.border);
            ui::drawSectionCard(window, bold, {ax, 62.f, 526.f, 322.f}, "Accounts", ui::theme.accent);

            ui::drawSectionCard(window, bold, {ax + 540.f, 62.f, WIN_W - (ax + 540.f) - PAGE_PAD, 210.f},
                                 "Account Detail", sf::Color(90, 160, 250));
            auto* c = currentSelected();
            if (c) {
                sf::Color pillColor = ui::statusColor(c->status);
                (void)pillColor;
                float py = 108.f;
                float pw = ui::drawStatusPill(window, regular, {ax + 558.f, py}, c->status);

                sf::Text head("#" + std::to_string(c->accountNumber) + "  " + c->name, bold, 15);
                head.setStyle(sf::Text::Bold);
                head.setFillColor(ui::theme.textDark);
                head.setPosition(ax + 558.f + pw + 12.f, py + 2.f);
                window.draw(head);

                std::ostringstream d;
                d << money(c->balance) << "\n\n"
                  << c->phone << "\n"
                  << c->address << "\n"
                  << "Opened " << c->createdDate;
                accDetail->setText(d.str());
                accDetail->setPosition(ax + 558.f, py + 34.f);
            } else {
                accDetail->setText("Select an account from the table, then click\n'Load Into Editor' to edit it.");
                accDetail->setPosition(ax + 558.f, 108.f);
            }

            ui::drawSectionCard(window, bold, {ax + 540.f, 284.f, WIN_W - (ax + 540.f) - PAGE_PAD, 100.f},
                                 "Quick Transaction", ui::theme.success);
            ui::drawSectionCard(window, bold, {ex - 18.f, ey - 40.f, 526.f, 300.f},
                                 "Edit & Status", ui::theme.warning);
        } else if (currentTab == 3) {
            ui::drawSectionCard(window, bold, {SIDEBAR_W + PAGE_PAD, 62.f, WIN_W - SIDEBAR_W - PAGE_PAD * 2, 56.f},
                                 "Filter", ui::theme.accent);
        } else if (currentTab == 4) {
            ui::drawSectionCard(window, bold, {cardLeft, PAGE_TOP, 470.f, FIELD_GAP * 3.f + 96.f},
                                 "Verify & Reset", ui::theme.warning);
        } else if (currentTab == 5) {
            ui::drawSectionCard(window, bold, {cardLeft, PAGE_TOP, 420.f, FIELD_GAP + 92.f},
                                 "Delete Closed Account", ui::theme.danger);
            ui::drawSectionCard(window, bold,
                                 {SIDEBAR_W + PAGE_PAD * 2.f + 420.f, PAGE_TOP, 360.f, 34.f * 7.f + 40.f},
                                 "Summary Report", ui::theme.accent);

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