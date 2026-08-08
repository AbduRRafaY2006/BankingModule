#include "ATM.h"
#include "Widgets.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <memory>
#include <vector>
#include <sstream>
#include <stdexcept>

using ui::Button;
using ui::Label;
using ui::TextBox;
using ui::Widget;

namespace {
// Layout constants tuned for an ATM-sized window
constexpr float WIN_W = 760.f;
constexpr float WIN_H = 480.f;

sf::Font loadFontOrThrow(const std::string& file) {
    sf::Font font;
    for (const std::string& base : {"assets/", "../assets/", "./"}) {
        if (font.loadFromFile(base + file)) return font;
    }
    throw std::runtime_error("Could not load font: " + file);
}

template <typename T, typename... Args>
T* add(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab, Args&&... args) {
    auto w = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = w.get();
    tab.push_back(raw);
    owner.push_back(std::move(w));
    return raw;
}

std::string money(double v) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed); oss.precision(2);
    oss << v;
    return oss.str();
}

enum class Screen {
    Login,
    Dashboard,
    Balance,
    Deposit,
    Withdraw,
    MiniStatement,
    ChangePin,
};

} // namespace

// Forward declarations for builders (each screen implemented separately)
static void BuildLoginScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                             sf::Font& regular, sf::Font& bold, Bank& bank,
                             Screen& current, long long& loggedIn, bool& shouldClose, Label*& outMsg);
static void BuildDashboard(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                           sf::Font& regular, sf::Font& bold, Bank& bank,
                           Screen& current, long long& loggedIn, Label*& welcomeLabel);
static void BuildBalanceScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                               sf::Font& regular, sf::Font& bold, Bank& bank,
                               Screen& current, long long& loggedIn, Label*& balanceLabel);
static void BuildDepositScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                               sf::Font& regular, sf::Font& bold, Bank& bank,
                               Screen& current, long long& loggedIn, Label*& resLabel);
static void BuildWithdrawScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                                sf::Font& regular, sf::Font& bold, Bank& bank,
                                Screen& current, long long& loggedIn, Label*& resLabel);
static void BuildMiniStatement(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                               sf::Font& regular, sf::Font& bold, Bank& bank,
                               Screen& current, long long& loggedIn);
static void BuildChangePinScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                                 sf::Font& regular, sf::Font& bold, Bank& bank,
                                 Screen& current, long long& loggedIn, Label*& resLabel);

void RunATMUI(Bank& bank) {
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                             "ATM", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Font regular = loadFontOrThrow("DejaVuSans.ttf");
    sf::Font bold = loadFontOrThrow("DejaVuSans-Bold.ttf");

    std::vector<std::unique_ptr<Widget>> owner;
    Screen current = Screen::Login;
    long long loggedInAccount = 0;
    bool shouldClose = false;

    // per-screen widget groups
    std::vector<Widget*> tabLogin, tabDashboard, tabBalance, tabDeposit, tabWithdraw, tabMini, tabChange;

    // message / dynamic labels we update from actions
    Label* loginMsg = nullptr;
    Label* welcomeLabel = nullptr;
    Label* balanceLabel = nullptr;
    Label* depositRes = nullptr;
    Label* withdrawRes = nullptr;
    Label* changePinRes = nullptr;

    // Build screens (each function wires callbacks and fills owner/tab vectors)
    BuildLoginScreen(owner, tabLogin, regular, bold, bank, current, loggedInAccount, shouldClose, loginMsg);
    BuildDashboard(owner, tabDashboard, regular, bold, bank, current, loggedInAccount, welcomeLabel);
    BuildBalanceScreen(owner, tabBalance, regular, bold, bank, current, loggedInAccount, balanceLabel);
    BuildDepositScreen(owner, tabDeposit, regular, bold, bank, current, loggedInAccount, depositRes);
    BuildWithdrawScreen(owner, tabWithdraw, regular, bold, bank, current, loggedInAccount, withdrawRes);
    BuildMiniStatement(owner, tabMini, regular, bold, bank, current, loggedInAccount);
    BuildChangePinScreen(owner, tabChange, regular, bold, bank, current, loggedInAccount, changePinRes);

    std::vector<std::vector<Widget*>*> tabs = {&tabLogin, &tabDashboard, &tabBalance,
                                               &tabDeposit, &tabWithdraw, &tabMini, &tabChange};

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();

            for (auto* w : *tabs[static_cast<int>(current)]) w->handleEvent(event, window);
        }

        if (shouldClose) {
            window.close();
            continue;
        }

        // Some screens need to refresh dynamic content when shown: update them here
        if (current == Screen::Dashboard && welcomeLabel) {
            if (loggedInAccount != 0) {
                if (auto c = bank.findByAccountNumber(loggedInAccount)) {
                    welcomeLabel->setText(std::string("Welcome ") + c->name);
                }
            }
        } else if (current == Screen::Balance && balanceLabel) {
            if (loggedInAccount != 0) {
                if (auto c = bank.findByAccountNumber(loggedInAccount)) {
                    balanceLabel->setText(std::string("Balance: ") + money(c->balance));
                }
            }
        }

        window.clear(ui::theme.contentBg);

        // Draw header
        sf::Text title("ATM", bold, 20);
        title.setFillColor(ui::theme.textDark);
        title.setPosition(18.f, 12.f);
        window.draw(title);

        for (auto* w : *tabs[static_cast<int>(current)]) w->draw(window);

        window.display();
    }
}

// ---------------------------------------------------------------------------
// Screen builders
// Each builds widgets into the provided owner/tab vectors and wires callbacks
// ---------------------------------------------------------------------------

static void BuildLoginScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                             sf::Font& regular, sf::Font& bold, Bank& bank,
                             Screen& current, long long& loggedIn, bool& shouldClose, Label*& outMsg) {
    float x = 40.f, y = 80.f;
    auto accBox = add<TextBox>(owner, tab, regular, "Account number", true);
    accBox->setPosition(x, y); accBox->setSize(320.f, 40.f);

    auto pinBox = add<TextBox>(owner, tab, regular, "PIN", true, true);
    pinBox->setPosition(x, y + 56.f); pinBox->setSize(320.f, 40.f);

    outMsg = add<Label>(owner, tab, regular, "", 14, ui::theme.danger);
    outMsg->setPosition(x, y + 140.f);

    auto loginBtn = add<Button>(owner, tab, regular, "Enter",
        [accBox, pinBox, outMsg, &bank, &current, &loggedIn] {
            long long acc = 0;
            try { acc = std::stoll(accBox->value()); } catch (...) { outMsg->setText("Invalid account number."); return; }
            auto c = bank.findByAccountNumber(acc);
            if (!c) { outMsg->setText("Account not found."); return; }
            if (c->pin != pinBox->value()) { outMsg->setText("Incorrect PIN."); return; }
            if (c->status != "Active") { outMsg->setText("Account is not active."); return; }
            // success
            loggedIn = acc;
            accBox->clear(); pinBox->clear(); outMsg->setText("");
            current = Screen::Dashboard;
        }, ui::theme.accent, ui::theme.textLight);
    loginBtn->setPosition(x, y + 96.f); loginBtn->setSize(160.f, 42.f);

    auto quitBtn = add<Button>(owner, tab, regular, "Quit",
        [&shouldClose] { shouldClose = true; }, ui::theme.textDim, ui::theme.textLight);
    quitBtn->setPosition(x + 180.f, y + 96.f); quitBtn->setSize(120.f, 42.f);
}

static void BuildDashboard(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                           sf::Font& regular, sf::Font& bold, Bank& bank,
                           Screen& current, long long& loggedIn, Label*& welcomeLabel) {
    float x = 40.f, y = 80.f, gap = 56.f;
    welcomeLabel = add<Label>(owner, tab, regular, "Welcome", 16, ui::theme.textDark, true);
    welcomeLabel->setPosition(x, y - 36.f);

    auto balBtn = add<Button>(owner, tab, regular, "Balance Inquiry",
        [&current] { current = Screen::Balance; }, ui::theme.accent, ui::theme.textLight);
    balBtn->setPosition(x, y); balBtn->setSize(260.f, 44.f);

    auto depBtn = add<Button>(owner, tab, regular, "Deposit",
        [&current] { current = Screen::Deposit; }, ui::theme.success, ui::theme.textLight);
    depBtn->setPosition(x, y + gap); depBtn->setSize(260.f, 44.f);

    auto wdrBtn = add<Button>(owner, tab, regular, "Withdraw",
        [&current] { current = Screen::Withdraw; }, ui::theme.danger, ui::theme.textLight);
    wdrBtn->setPosition(x, y + gap * 2); wdrBtn->setSize(260.f, 44.f);

    auto miniBtn = add<Button>(owner, tab, regular, "Mini Statement",
        [&current] { current = Screen::MiniStatement; }, ui::theme.accent, ui::theme.textLight);
    miniBtn->setPosition(x, y + gap * 3); miniBtn->setSize(260.f, 44.f);

    auto pinBtn = add<Button>(owner, tab, regular, "Change PIN",
        [&current] { current = Screen::ChangePin; }, ui::theme.warning, ui::theme.textDark);
    pinBtn->setPosition(x + 320.f, y); pinBtn->setSize(220.f, 44.f);

    auto logoutBtn = add<Button>(owner, tab, regular, "Logout",
        [&loggedIn, &current] { loggedIn = 0; current = Screen::Login; }, ui::theme.textDim, ui::theme.textLight);
    logoutBtn->setPosition(x + 320.f, y + gap); logoutBtn->setSize(220.f, 44.f);
}

static void BuildBalanceScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                               sf::Font& regular, sf::Font& bold, Bank& bank,
                               Screen& current, long long& loggedIn, Label*& balanceLabel) {
    float x = 40.f, y = 120.f;
    balanceLabel = add<Label>(owner, tab, regular, "Balance: ", 18, ui::theme.textDark, true);
    balanceLabel->setPosition(x, y);

    auto back = add<Button>(owner, tab, regular, "Back",
        [&current] { current = Screen::Dashboard; }, ui::theme.textDim, ui::theme.textLight);
    back->setPosition(40.f, 360.f); back->setSize(120.f, 40.f);
}

static void BuildDepositScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                               sf::Font& regular, sf::Font& bold, Bank& bank,
                               Screen& current, long long& loggedIn, Label*& resLabel) {
    float x = 40.f, y = 100.f;
    auto amt = add<TextBox>(owner, tab, regular, "Amount e.g. 50.00");
    amt->setPosition(x, y); amt->setSize(220.f, 40.f);

    resLabel = add<Label>(owner, tab, regular, "", 14, ui::theme.success);
    resLabel->setPosition(x, y + 100.f);

    auto doDep = add<Button>(owner, tab, regular, "Deposit",
        [amt, resLabel, &loggedIn, &bank] {
            if (loggedIn == 0) { resLabel->setText("Not logged in."); return; }
            double v = 0;
            try { v = std::stod(amt->value()); } catch (...) { resLabel->setText("Invalid amount."); return; }
            if (v <= 0) { resLabel->setText("Amount must be positive."); return; }
            auto r = bank.deposit(loggedIn, v);
            resLabel->setText(r.message);
            if (r.ok) amt->clear();
        }, ui::theme.success, ui::theme.textLight);
    doDep->setPosition(x + 260.f, y); doDep->setSize(140.f, 40.f);

    auto back = add<Button>(owner, tab, regular, "Back",
        [&current] { current = Screen::Dashboard; }, ui::theme.textDim, ui::theme.textLight);
    back->setPosition(40.f, 360.f); back->setSize(120.f, 40.f);
}

static void BuildWithdrawScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                                sf::Font& regular, sf::Font& bold, Bank& bank,
                                Screen& current, long long& loggedIn, Label*& resLabel) {
    float x = 40.f, y = 100.f;
    auto amt = add<TextBox>(owner, tab, regular, "Amount e.g. 20.00");
    amt->setPosition(x, y); amt->setSize(220.f, 40.f);

    resLabel = add<Label>(owner, tab, regular, "", 14, ui::theme.danger);
    resLabel->setPosition(x, y + 100.f);

    auto doW = add<Button>(owner, tab, regular, "Withdraw",
        [amt, resLabel, &loggedIn, &bank] {
            if (loggedIn == 0) { resLabel->setText("Not logged in."); return; }
            double v = 0;
            try { v = std::stod(amt->value()); } catch (...) { resLabel->setText("Invalid amount."); return; }
            if (v <= 0) { resLabel->setText("Amount must be positive."); return; }
            auto r = bank.withdraw(loggedIn, v);
            resLabel->setText(r.message);
            if (r.ok) amt->clear();
        }, ui::theme.danger, ui::theme.textLight);
    doW->setPosition(x + 260.f, y); doW->setSize(140.f, 40.f);

    auto back = add<Button>(owner, tab, regular, "Back",
        [&current] { current = Screen::Dashboard; }, ui::theme.textDim, ui::theme.textLight);
    back->setPosition(40.f, 360.f); back->setSize(120.f, 40.f);
}

static void BuildMiniStatement(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                               sf::Font& regular, sf::Font& bold, Bank& bank,
                               Screen& current, long long& loggedIn) {
    using ui::Table; using ui::TableColumn;
    float x = 24.f, y = 64.f;
    auto tbl = std::make_unique<Table>(regular, bold, std::vector<TableColumn>{
        {"TIME", 180.f, false}, {"TYPE", 120.f, false}, {"AMOUNT", 120.f, true}, {"BALANCE", 120.f, true}
    }, 28);
    Table* tblRaw = tbl.get();
    tblRaw->setPosition(x, y);
    tblRaw->setSize(700.f, 320.f);
    tab.push_back(tblRaw);
    owner.push_back(std::move(tbl));

    auto refresh = add<Button>(owner, tab, regular, "Refresh",
        [tblRaw, &loggedIn, &bank] {
            if (loggedIn == 0) return;
            auto tx = bank.transactionsFor(loggedIn);
            std::vector<std::vector<std::string>> rows;
            for (auto& t : tx) {
                rows.push_back({ t.timestamp, t.type, money(t.amount), money(t.balanceAfter) });
            }
            tblRaw->setRows(rows);
        }, ui::theme.accent, ui::theme.textLight);
    refresh->setPosition(24.f, 400.f); refresh->setSize(120.f, 36.f);

    auto back = add<Button>(owner, tab, regular, "Back",
        [&current] { current = Screen::Dashboard; }, ui::theme.textDim, ui::theme.textLight);
    back->setPosition(160.f, 400.f); back->setSize(120.f, 36.f);
}

static void BuildChangePinScreen(std::vector<std::unique_ptr<Widget>>& owner, std::vector<Widget*>& tab,
                                 sf::Font& regular, sf::Font& bold, Bank& bank,
                                 Screen& current, long long& loggedIn, Label*& resLabel) {
    float x = 40.f, y = 100.f, gap = 52.f;
    auto curBox = add<TextBox>(owner, tab, regular, "Current PIN", true, true);
    curBox->setPosition(x, y); curBox->setSize(260.f, 40.f);

    auto newBox = add<TextBox>(owner, tab, regular, "New PIN (4 digits)", true, true);
    newBox->setPosition(x, y + gap); newBox->setSize(260.f, 40.f);

    auto confBox = add<TextBox>(owner, tab, regular, "Confirm PIN", true, true);
    confBox->setPosition(x, y + gap * 2); confBox->setSize(260.f, 40.f);

    resLabel = add<Label>(owner, tab, regular, "", 14, ui::theme.success);
    resLabel->setPosition(x, y + gap * 3 + 10.f);

    auto doChange = add<Button>(owner, tab, regular, "Change PIN",
        [curBox, newBox, confBox, resLabel, &loggedIn, &bank] {
            if (loggedIn == 0) { resLabel->setText("Not logged in."); return; }
            auto c = bank.findByAccountNumber(loggedIn);
            if (!c) { resLabel->setText("Account not found."); return; }
            if (c->pin != curBox->value()) { resLabel->setText("Current PIN incorrect."); return; }
            std::string np = newBox->value();
            std::string cp = confBox->value();
            if (np.size() != 4 || !std::all_of(np.begin(), np.end(), [](unsigned char ch){ return std::isdigit(ch); })) {
                resLabel->setText("New PIN must be exactly 4 digits."); return;
            }
            if (np != cp) { resLabel->setText("PIN confirmation does not match."); return; }
            // Bank::resetPin requires cnic verification; use the stored CNIC after current-PIN verification
            auto r = bank.resetPin(loggedIn, c->cnic, np);
            resLabel->setText(r.message);
            if (r.ok) { curBox->clear(); newBox->clear(); confBox->clear(); }
        }, ui::theme.accent, ui::theme.textLight);
    doChange->setPosition(x + 300.f, y); doChange->setSize(140.f, 40.f);

    auto back = add<Button>(owner, tab, regular, "Back",
        [&current] { current = Screen::Dashboard; }, ui::theme.textDim, ui::theme.textLight);
    back->setPosition(40.f, 400.f); back->setSize(120.f, 36.f);
}
