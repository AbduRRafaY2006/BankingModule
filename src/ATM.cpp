#include "ATM.h"
#include "Widgets.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <cctype>
#include <thread>

using ui::Button;
using ui::Label;
using ui::TextBox;
using ui::Widget;
using ui::CashDispenser;

namespace {

constexpr float WIN_W = 680.f;
constexpr float WIN_H = 640.f;

// Reference-inspired ATM theme
const sf::Color ATM_BG(245, 245, 245);
const sf::Color ATM_BODY(27, 57, 101);
const sf::Color ATM_BODY_DARK(14, 36, 70);
const sf::Color ATM_SCREEN(2, 12, 3);
const sf::Color ATM_GREEN(36, 255, 82);
const sf::Color ATM_ACCENT(250, 196, 45);
const sf::Color ATM_ACCENT_DIM(190, 145, 28);
const sf::Color ATM_TEXT(235, 240, 245);
const sf::Color ATM_TEXT_DIM(165, 195, 225);
const sf::Color ATM_SUCCESS(46, 204, 113);
const sf::Color ATM_DANGER(231, 76, 60);
const sf::Color ATM_CARD_BG(220, 220, 235);
const sf::Color ATM_BTN_BG(50, 70, 110);
const sf::Color ATM_BTN_HOVER(70, 95, 150);

sf::Color accentFor(const std::string& title) {
    if (title == "DEPOSIT") return sf::Color(46, 204, 113);
    if (title == "WITHDRAW") return sf::Color(230, 160, 60);
    if (title == "BALANCE INQUIRY") return sf::Color(90, 160, 250);
    if (title == "CHANGE PIN") return sf::Color(180, 130, 240);
    if (title == "MINI STATEMENT") return sf::Color(90, 200, 210);
    return ATM_ACCENT;
}

// Small circular badge with an arrow (up/down) or a glyph — used as a
// per-screen icon next to titles and mini-statement rows.
void drawBadge(sf::RenderWindow& window, sf::Vector2f center, float r, sf::Color color, bool up) {
    sf::CircleShape c(r);
    c.setOrigin(r, r);
    c.setPosition(center);
    c.setFillColor(sf::Color(color.r, color.g, color.b, 60));
    c.setOutlineThickness(2.f);
    c.setOutlineColor(color);
    window.draw(c);
    sf::ConvexShape arrow;
    arrow.setPointCount(3);
    float a = r * 0.5f;
    if (up) {
        arrow.setPoint(0, {center.x, center.y - a});
        arrow.setPoint(1, {center.x - a, center.y + a * 0.6f});
        arrow.setPoint(2, {center.x + a, center.y + a * 0.6f});
    } else {
        arrow.setPoint(0, {center.x, center.y + a});
        arrow.setPoint(1, {center.x - a, center.y - a * 0.6f});
        arrow.setPoint(2, {center.x + a, center.y - a * 0.6f});
    }
    arrow.setFillColor(color);
    window.draw(arrow);
}

sf::Font loadFontOrThrow(const std::string& file) {
    sf::Font font;
    for (const std::string& base : {"assets/", "../assets/", "./"}) {
        if (font.loadFromFile(base + file)) return font;
    }
    throw std::runtime_error("Could not load font: " + file);
}

std::string money(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

// sf::Text's std::string constructor decodes as ANSI/Latin-1, not UTF-8.
// Any literal with a multi-byte character (₨ ▼ ✓ ✗ ⌫ ↵ ─ etc.) must be
// passed through this helper first, or it renders as garbled glyphs with
// the wrong measured width (which is what was throwing off centering and
// causing text to overlap).
sf::String utf8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

std::string toUpperAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// ----------------------------------------------------------------------------
// Sound effects — lazily loaded, small round-robin pool per sound so rapid
// repeated plays (e.g. fast keypad taps) don't cut each other off. This file
// draws its own buttons directly (not via ui::Button), so it owns its own
// sound triggering rather than relying on the shared widget layer.
// ----------------------------------------------------------------------------
namespace sound {
sf::SoundBuffer& bufferFor(const std::string& filename) {
    static std::vector<std::unique_ptr<sf::SoundBuffer>> cache;
    static std::vector<std::string> names;
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == filename) return *cache[i];
    }
    auto buf = std::make_unique<sf::SoundBuffer>();
    bool ok = false;
    for (const std::string& base : {"assets/", "../assets/", "./"}) {
        if (buf->loadFromFile(base + filename)) { ok = true; break; }
    }
    if (!ok) {
        static sf::SoundBuffer empty;
        return empty;  // silent failure — sound is a nice-to-have
    }
    names.push_back(filename);
    cache.push_back(std::move(buf));
    return *cache.back();
}

template <size_t N>
struct Pool {
    sf::Sound sounds[N];
    size_t next = 0;
    void play(sf::SoundBuffer& buf) {
        sounds[next].setBuffer(buf);
        sounds[next].play();
        next = (next + 1) % N;
    }
};

void click() {
    static Pool<4> pool;
    pool.play(bufferFor("click.wav"));
}
void cardInsert() {
    static Pool<2> pool;
    pool.play(bufferFor("card_insert.wav"));
}
void success() {
    static Pool<2> pool;
    pool.play(bufferFor("success_chime.wav"));
}
}  // namespace sound

// ============================================================================
// Reference-style ATM frame helpers
// ============================================================================
void drawATMBody(sf::RenderWindow& window) {
    // Reference proportions: a compact, almost-square ATM body with generous
    // white space around it.
    sf::FloatRect shadow(51.f, 22.f, 590.f, 585.f);
    ui::drawRoundedRect(window, shadow, 34.f, sf::Color(0, 0, 0, 35));

    sf::FloatRect body(45.f, 14.f, 590.f, 585.f);
    ui::drawRoundedRect(window, body, 34.f, ATM_BODY, sf::Color(9, 29, 57), 5.f);

    sf::FloatRect highlight(51.f, 20.f, 578.f, 7.f);
    ui::drawRoundedRect(window, highlight, 4.f, sf::Color(255,255,255,35));
}

void drawATMHeader(sf::RenderWindow& window, sf::Font& font, const std::string& contextLabel, const std::string& slotLabel) {
    // Small context subtitle in the top margin, above the plaque — shows
    // which screen is currently active so the header isn't identical/static
    // across every screen.
    if (!contextLabel.empty()) {
        sf::Text ctxText(contextLabel, font, 13);
        ctxText.setStyle(sf::Text::Bold);
        ctxText.setFillColor(sf::Color(160, 180, 210));
        ctxText.setPosition((WIN_W - ctxText.getLocalBounds().width) / 2.f, 22.f);
        window.draw(ctxText);
    }

    // Yellow BANK ATM plaque, matching the reference.
    sf::FloatRect titleRect(154.f, 45.f, 373.f, 49.f);
    ui::drawRoundedRect(window, titleRect, 17.f, ATM_ACCENT);

    sf::Text title("B A N K  A T M", font, 24);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(ATM_BODY_DARK);
    title.setPosition(titleRect.left + (titleRect.width - title.getLocalBounds().width)/2.f,
                      titleRect.top + 8.f);
    window.draw(title);

    sf::CircleShape light(8.f);
    light.setPosition(588.f, 42.f);
    light.setFillColor(ATM_BODY_DARK);
    window.draw(light);

    // Card slot + yellow card lip.
    sf::FloatRect slot(212.f, 108.f, 256.f, 23.f);
    ui::drawRoundedRect(window, slot, 7.f, ATM_BODY_DARK);
    sf::FloatRect slotLip(272.f, 88.f, 136.f, 29.f);
    ui::drawRoundedRect(window, slotLip, 5.f, ATM_ACCENT);

    // Shows "INSERT CARD" only while no one is authenticated; once logged
    // in, this shows the customer's name instead — previously this always
    // said "INSERT CARD" even mid-session, which was simply incorrect.
    sf::Text insert(slotLabel, font, 17);
    insert.setStyle(sf::Text::Bold);
    insert.setFillColor(ATM_TEXT_DIM);
    insert.setPosition((WIN_W - insert.getLocalBounds().width)/2.f, 137.f);
    window.draw(insert);
}

void drawCashReceiptSlot(sf::RenderWindow& window, sf::Font& font) {
    sf::FloatRect slot(235.f, 520.f, 209.f, 23.f);
    ui::drawRoundedRect(window, slot, 7.f, ATM_BODY_DARK);

    sf::Text label("CASH / RECEIPT", font, 16);
    label.setStyle(sf::Text::Bold);
    label.setFillColor(ATM_TEXT_DIM);
    label.setPosition((WIN_W - label.getLocalBounds().width)/2.f, 550.f);
    window.draw(label);

    // Two small lower openings seen in the reference.
    sf::FloatRect leftFoot(259.f, 581.f, 47.f, 15.f);
    sf::FloatRect rightFoot(375.f, 581.f, 47.f, 15.f);
    ui::drawRoundedRect(window, leftFoot, 4.f, ATM_BODY_DARK);
    ui::drawRoundedRect(window, rightFoot, 4.f, ATM_BODY_DARK);
}

void drawSideButton(sf::RenderWindow& window, sf::FloatRect rect, bool left, bool hovered) {
    // Recessed housing + physical yellow button. The hover state is rendered
    // here so the side controls visibly respond to the mouse.
    sf::FloatRect housing(rect.left - 5.f, rect.top - 5.f,
                          rect.width + 10.f, rect.height + 10.f);
    ui::drawRoundedRect(window, housing, 13.f, ATM_BODY_DARK);

    sf::Color buttonFill = hovered ? sf::Color(255, 210, 70) : ATM_ACCENT;
    sf::Color outline = hovered ? sf::Color(255, 230, 120) : ATM_ACCENT_DIM;
    ui::drawRoundedRect(window, rect, 11.f, buttonFill, outline, 2.f);

    sf::ConvexShape arrow;
    arrow.setPointCount(3);
    const float cx = rect.left + rect.width * 0.5f;
    const float cy = rect.top + rect.height * 0.5f;
    const float halfH = 9.f;
    const float tip = 8.f;
    const float base = 7.f;
    if (left) {
        arrow.setPoint(0, {cx + tip, cy});
        arrow.setPoint(1, {cx - base, cy - halfH});
        arrow.setPoint(2, {cx - base, cy + halfH});
    } else {
        arrow.setPoint(0, {cx - tip, cy});
        arrow.setPoint(1, {cx + base, cy - halfH});
        arrow.setPoint(2, {cx + base, cy + halfH});
    }
    arrow.setFillColor(ATM_BODY_DARK);
    window.draw(arrow);
}

void drawATMFrame(sf::RenderWindow& window, sf::Font& font, const std::string& contextLabel, const std::string& slotLabel) {
    drawATMBody(window);
    drawATMHeader(window, font, contextLabel, slotLabel);
    drawCashReceiptSlot(window, font);
}

// ============================================================================
// ATM State Machine
// ============================================================================
enum class ATMState {
    Idle,           // Card insertion screen
    EnteringPIN,    // PIN entry
    MainMenu,       // Main menu
    BalanceInquiry,
    Deposit,
    Withdraw,
    MiniStatement,
    ChangePIN,
    Processing,     // Showing transaction animation
    TransactionSuccess,
    TransactionFailed,
    Logout
};

// Short subtitle shown in the top margin of the header for whichever screen
// is currently active — keeps the header from looking identical on every
// screen. Empty string means no subtitle (card-insert / idle state).
std::string screenTitleFor(ATMState s) {
    switch (s) {
        case ATMState::EnteringPIN:       return "ENTER PIN";
        case ATMState::MainMenu:          return "MAIN MENU";
        case ATMState::BalanceInquiry:    return "BALANCE INQUIRY";
        case ATMState::Deposit:           return "DEPOSIT";
        case ATMState::Withdraw:          return "WITHDRAWAL";
        case ATMState::MiniStatement:     return "MINI STATEMENT";
        case ATMState::ChangePIN:         return "";
        case ATMState::Processing:        return "PROCESSING";
        case ATMState::TransactionSuccess:
        case ATMState::TransactionFailed: return "TRANSACTION RESULT";
        default:                          return "";
    }
}

ATMState escapeTargetFor(ATMState s) {
    switch (s) {
        case ATMState::EnteringPIN:       return ATMState::Logout;
        case ATMState::MainMenu:          return ATMState::EnteringPIN;
        case ATMState::BalanceInquiry:
        case ATMState::Deposit:
        case ATMState::Withdraw:
        case ATMState::MiniStatement:
        case ATMState::ChangePIN:
        case ATMState::TransactionSuccess:
        case ATMState::TransactionFailed: return ATMState::MainMenu;
        default:                          return s;
    }
}

bool isActiveAccount(const Bank& bank, long long accNo) {
    auto customer = bank.findByAccountNumber(accNo);
    return customer.has_value() && customer->status == "Active";
}

struct ATMContext {
    Bank* bank = nullptr;
    long long accountNumber = 0;
    std::string pin;
    Customer currentCustomer;
    double transactionAmount = 0.0;
    std::string transactionMessage;
    bool isSuccess = false;
    ATMState nextState = ATMState::Idle;
    std::vector<Transaction> miniStatementTxns;
    float animationProgress = 0.f;
    bool animationComplete = false;
};

// ============================================================================
// ATM Screen Classes
// ============================================================================

class ATMScreen {
public:
    virtual ~ATMScreen() = default;
    virtual void handleEvent(const sf::Event& e, sf::RenderWindow& window) = 0;
    virtual void draw(sf::RenderWindow& window) = 0;
    virtual void update(float dt) {}
    virtual ATMState nextState() const { return ATMState::Idle; }
    virtual bool isDone() const { return false; }
};

// ----------------------------------------------------------------------------
// Card Insertion Screen
// ----------------------------------------------------------------------------
class CardInsertScreen : public ATMScreen {
public:
    CardInsertScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {}

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code == sf::Keyboard::Enter || e.key.code == sf::Keyboard::Space) {
                ctx_.nextState = ATMState::EnteringPIN;
            }
        }
        if (e.type == sf::Event::MouseButtonPressed) {
            if (insertBtn_.contains(e.mouseButton.x, e.mouseButton.y)) {
                ctx_.nextState = ATMState::EnteringPIN;
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        // Common ATM body is drawn by RunATMUI.

        // Central display area
        sf::FloatRect screen(109.f, 178.f, 290.f, 300.f);
        ui::drawRoundedRect(window, screen, 16.f, ATM_SCREEN, ATM_GREEN, 3.f);

        sf::Text prompt("WELCOME", font_, 25);
        prompt.setStyle(sf::Text::Bold);
        prompt.setFillColor(ATM_GREEN);
        prompt.setPosition((WIN_W - prompt.getLocalBounds().width)/2.f, 215.f);
        window.draw(prompt);

        sf::Text line(utf8("────────────────────"), font_, 14);
        line.setFillColor(ATM_GREEN);
        line.setPosition((WIN_W - line.getLocalBounds().width)/2.f, 255.f);
        window.draw(line);

        sf::Text msg("INSERT CARD", font_, 22);
        msg.setStyle(sf::Text::Bold);
        msg.setFillColor(ATM_GREEN);
        msg.setPosition((WIN_W - msg.getLocalBounds().width)/2.f, 310.f);
        window.draw(msg);

        float bounce = std::sin(clock_.getElapsedTime().asSeconds() * 2.f) * 4.f;
        sf::Text arrow(utf8("▼"), font_, 27);
        arrow.setFillColor(ATM_GREEN);
        arrow.setPosition((WIN_W - arrow.getLocalBounds().width)/2.f, 360.f + bounce);
        window.draw(arrow);

        sf::Text hint("PRESS ENTER OR CLICK", font_, 13);
        hint.setFillColor(ATM_TEXT_DIM);
        hint.setPosition((WIN_W - hint.getLocalBounds().width)/2.f, 420.f);
        window.draw(hint);

        // The whole display is clickable as before.
        insertBtn_ = screen;
        insertBtn_.height += 50.f;
    }

private:
    sf::Font& font_;
    ATMContext& ctx_;
    sf::Clock clock_;
    sf::FloatRect insertBtn_{0.f, 0.f, WIN_W, WIN_H};
};

// ----------------------------------------------------------------------------
// PIN Entry Screen
// ----------------------------------------------------------------------------
class PinEntryScreen : public ATMScreen {
public:
    PinEntryScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {
        pin_.reserve(4);
    }

private:
    void registerFailedAttempt() {
        ++failedAttempts_;
        pin_.clear();
        if (failedAttempts_ >= 3) {
            error_ = "Too many failed attempts. Exiting ATM.";
            ctx_.nextState = ATMState::Logout;
            return;
        }
        error_ = "Invalid PIN. Please try again.";
        shaking_ = true;
        shakeClock_.restart();
    }

public:

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code >= sf::Keyboard::Num0 && e.key.code <= sf::Keyboard::Num9) {
                int digit = e.key.code - sf::Keyboard::Num0;
                if (pin_.size() < 4) { pin_ += ('0' + digit); sound::click(); }
            } else if (e.key.code >= sf::Keyboard::Numpad0 && e.key.code <= sf::Keyboard::Numpad9) {
                int digit = e.key.code - sf::Keyboard::Numpad0;
                if (pin_.size() < 4) { pin_ += ('0' + digit); sound::click(); }
            } else if (e.key.code == sf::Keyboard::BackSpace) {
                if (!pin_.empty()) pin_.pop_back();
            } else if (e.key.code == sf::Keyboard::Enter) {
                if (pin_.size() == 4) {
                    sound::click();
                    auto result = ctx_.bank->authenticate(ctx_.accountNumber, pin_);
                    if (result.has_value()) {
                        ctx_.currentCustomer = result.value();
                        ctx_.nextState = ATMState::MainMenu;
                        pin_.clear();
                        failedAttempts_ = 0;
                    } else {
                        registerFailedAttempt();
                    }
                }
            }
        }

        // Virtual keypad
        if (e.type == sf::Event::MouseButtonPressed) {
            for (const auto& [rect, digit] : keypadButtons_) {
                if (rect.contains(e.mouseButton.x, e.mouseButton.y)) {
                    sound::click();
                    if (digit == -1) { // Backspace
                        if (!pin_.empty()) pin_.pop_back();
                    } else if (digit == -2) { // Enter
                        if (pin_.size() == 4) {
                            auto result = ctx_.bank->authenticate(ctx_.accountNumber, pin_);
                            if (result.has_value()) {
                                ctx_.currentCustomer = result.value();
                                ctx_.nextState = ATMState::MainMenu;
                                pin_.clear();
                                failedAttempts_ = 0;
                                error_.clear();
                            } else {
                                registerFailedAttempt();
                            }
                        }
                    } else if (pin_.size() < 4) {
                        pin_ += ('0' + digit);
                        error_.clear();
                    }
                    break;
                }
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        // Shake decays over ~0.35s after a wrong PIN.
        float shakeOffset = 0.f;
        if (shaking_) {
            float t = shakeClock_.getElapsedTime().asSeconds();
            const float duration = 0.35f;
            if (t >= duration) {
                shaking_ = false;
            } else {
                float decay = 1.f - (t / duration);
                shakeOffset = std::sin(t * 40.f) * 8.f * decay;
            }
        }

        // Title — moved below the physical header/card-slot chrome (was at
        // y=40, colliding directly with the "BANK ATM" plaque).
        sf::Text title("ENTER PIN", font_, 22);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ATM_TEXT);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 168.f);
        window.draw(title);

        // Account display
        sf::Text acc("Account: " + std::to_string(ctx_.accountNumber), font_, 14);
        acc.setFillColor(ATM_TEXT_DIM);
        acc.setPosition((WIN_W - acc.getLocalBounds().width) / 2.f, 198.f);
        window.draw(acc);

        // PIN dots — shake horizontally on a wrong attempt.
        float dotY = 218.f;
        float dotsWidth = 3 * 50.f + 20.f;
        float dotsStartX = (WIN_W - dotsWidth) / 2.f + shakeOffset;
        for (int i = 0; i < 4; ++i) {
            sf::CircleShape dot(10.f);
            dot.setPosition(dotsStartX + i * 50.f, dotY);
            if (i < static_cast<int>(pin_.size())) {
                dot.setFillColor(ATM_ACCENT);
                dot.setOutlineColor(ATM_ACCENT_DIM);
                dot.setOutlineThickness(2.f);
            } else {
                dot.setFillColor(sf::Color(60, 80, 120));
                dot.setOutlineColor(sf::Color(40, 60, 90));
                dot.setOutlineThickness(2.f);
            }
            window.draw(dot);
        }

        // Error message
        if (!error_.empty()) {
            sf::Text err(error_, font_, 14);
            err.setFillColor(ATM_DANGER);
            err.setPosition((WIN_W - err.getLocalBounds().width) / 2.f + shakeOffset, 242.f);
            window.draw(err);
        }

        // Virtual keypad — shrunk and re-centered (was off-center and its
        // last row overlapped the CASH/RECEIPT slot at the bottom).
        const float ks = 56.f, kg = 8.f;
        const float kx = (WIN_W - (3 * ks + 2 * kg)) / 2.f, ky = 262.f;
        keypadButtons_.clear();

        std::vector<std::pair<std::string, int>> keys = {
            {"1", 1}, {"2", 2}, {"3", 3},
            {"4", 4}, {"5", 5}, {"6", 6},
            {"7", 7}, {"8", 8}, {"9", 9},
            {"⌫", -1}, {"0", 0}, {"↵", -2}
        };

        for (size_t i = 0; i < keys.size(); ++i) {
            float x = kx + (i % 3) * (ks + kg);
            float y = ky + (i / 3) * (ks + kg);
            sf::FloatRect rect(x, y, ks, ks);
            keypadButtons_.push_back({rect, keys[i].second});

            // Button background
            sf::Color bg = ATM_BTN_BG;
            bool isSpecial = (keys[i].second < 0);
            if (isSpecial) {
                bg = (keys[i].second == -2) ? ATM_ACCENT : sf::Color(60, 60, 80);
            }
            
            // Hover
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (rect.contains(mouse)) {
                bg = isSpecial ? sf::Color(220, 180, 60) : ATM_BTN_HOVER;
            }

            // Use drawRoundedRect for button
            ui::drawRoundedRect(window, rect, 12.f, bg);

            // Label
            sf::Text label(utf8(keys[i].first), font_, 19);
            label.setStyle(sf::Text::Bold);
            label.setFillColor(isSpecial ? ATM_TEXT : ATM_TEXT);
            label.setPosition(
                x + (ks - label.getLocalBounds().width) / 2.f,
                y + (ks - label.getLocalBounds().height) / 2.f - 4.f
            );
            window.draw(label);
        }
    }

private:
    sf::Font& font_;
    ATMContext& ctx_;
    std::string pin_;
    std::string error_;
    int failedAttempts_ = 0;
    std::vector<std::pair<sf::FloatRect, int>> keypadButtons_;
    bool shaking_ = false;
    sf::Clock shakeClock_;
};

// ----------------------------------------------------------------------------
// Main Menu Screen
// ----------------------------------------------------------------------------
class MainMenuScreen : public ATMScreen {
public:
    MainMenuScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {
        menuItems_ = {
            {"1", "BALANCE INQUIRY", ATMState::BalanceInquiry},
            {"2", "DEPOSIT", ATMState::Deposit},
            {"3", "WITHDRAW", ATMState::Withdraw},
            {"4", "MINI STATEMENT", ATMState::MiniStatement},
            {"5", "CHANGE PIN", ATMState::ChangePIN},
            {"6", "LOGOUT", ATMState::Logout}
        };
    }

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code >= sf::Keyboard::Num1 && e.key.code <= sf::Keyboard::Num6) {
                int idx = e.key.code - sf::Keyboard::Num1;
                if (idx < static_cast<int>(menuItems_.size())) {
                    sound::click();
                    ctx_.nextState = menuItems_[idx].state;
                }
            }
        }

        if (e.type == sf::Event::MouseButtonPressed) {
            for (size_t i = 0; i < menuRects_.size(); ++i) {
                if (menuRects_[i].contains(static_cast<float>(e.mouseButton.x),
                                           static_cast<float>(e.mouseButton.y))) {
                    sound::click();
                    ctx_.nextState = menuItems_[i].state;
                    break;
                }
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        // Central terminal display.  The reference has a clear physical gap
        // between the yellow buttons and the green screen border.
        const sf::FloatRect screen(177.f, 177.f, 327.f, 327.f);
        ui::drawRoundedRect(window, screen, 16.f, ATM_SCREEN, ATM_GREEN, 3.f);

        sf::Text heading("MAIN MENU", font_, 17);
        heading.setStyle(sf::Text::Bold);
        heading.setFillColor(ATM_GREEN);
        heading.setPosition(screen.left + (screen.width - heading.getLocalBounds().width)/2.f, 202.f);
        window.draw(heading);

        sf::RectangleShape divider({245.f, 1.f});
        divider.setPosition(193.f, 231.f);
        divider.setFillColor(ATM_GREEN);
        window.draw(divider);

        menuRects_.clear();

        // Text positions intentionally leave a large gap between the screen
        // edge and the physical side buttons.
        // Menu text is kept comfortably inside the display.
        const float leftX = 203.f;
        const float rightX = 389.f;
        const float firstY = 260.f;
        const float gapY = 56.f;

        // IMPORTANT: these are OUTSIDE the green display.  There is a visible
        // gap between every button and the screen, matching the reference.
        const float sideXLeft = 119.f;
        const float sideXRight = 531.f;
        const float sideY[] = {244.f, 300.f, 356.f, 412.f};
        const float sideW = 34.f;
        const float sideH = 39.f;

        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        // Hover state per menu item (0-3 = left buttons, 4-5 = right buttons),
        // computed once and used for both the physical button AND its
        // matching text line on screen, so hovering a button visibly lights
        // up the option it corresponds to.
        bool hovered[6];
        for (int i = 0; i < 4; ++i) {
            sf::FloatRect r(sideXLeft - 5.f, sideY[i] - 5.f, sideW + 10.f, sideH + 10.f);
            hovered[i] = r.contains(mouse);
            drawSideButton(window, {sideXLeft, sideY[i], sideW, sideH}, true, hovered[i]);
        }
        {
            sf::FloatRect r(sideXRight - 5.f, sideY[0] - 5.f, sideW + 10.f, sideH + 10.f);
            hovered[4] = r.contains(mouse);
            drawSideButton(window, {sideXRight, sideY[0], sideW, sideH}, false, hovered[4]);
        }
        {
            sf::FloatRect r(sideXRight - 5.f, sideY[1] - 5.f, sideW + 10.f, sideH + 10.f);
            hovered[5] = r.contains(mouse);
            drawSideButton(window, {sideXRight, sideY[1], sideW, sideH}, false, hovered[5]);
        }

        // Hitboxes follow the physical buttons exactly.
        for (int i = 0; i < 4; ++i)
            menuRects_.push_back({sideXLeft - 5.f, sideY[i] - 5.f, sideW + 10.f, sideH + 10.f});
        menuRects_.push_back({sideXRight - 5.f, sideY[0] - 5.f, sideW + 10.f, sideH + 10.f});
        menuRects_.push_back({sideXRight - 5.f, sideY[1] - 5.f, sideW + 10.f, sideH + 10.f});

        auto drawOption = [&](int index, const std::string& text, float x, float y) {
            sf::Color c = hovered[index] ? sf::Color(150, 255, 170) : ATM_GREEN;

            sf::Text num(std::to_string(index + 1), font_, 15);
            num.setFillColor(c);
            num.setPosition(x, y);
            window.draw(num);

            sf::Text label(text, font_, 14);
            label.setStyle(hovered[index] ? sf::Text::Bold : sf::Text::Regular);
            label.setFillColor(c);
            label.setPosition(x + 22.f, y);
            window.draw(label);
        };

        drawOption(0, "BALANCE INQ", leftX, firstY);
        drawOption(1, "DEPOSIT", leftX, firstY + gapY);
        drawOption(2, "WITHDRAW", leftX, firstY + gapY * 2.f);
        drawOption(3, "MINI STMT", leftX, firstY + gapY * 3.f);
        drawOption(4, "CHANGE PIN", rightX, firstY);
        drawOption(5, "LOGOUT", rightX, firstY + gapY);

        // Small green terminal cursor at the bottom-left of the display.
        sf::RectangleShape cursor({10.f, 19.f});
        cursor.setPosition(203.f, 459.f);
        cursor.setFillColor(ATM_GREEN);
        window.draw(cursor);
    }

private:
    struct MenuItem {
        std::string number;
        std::string label;
        ATMState state;
    };
    sf::Font& font_;
    ATMContext& ctx_;
    std::vector<MenuItem> menuItems_;
    std::vector<sf::FloatRect> menuRects_;
};

// ----------------------------------------------------------------------------
// Base Transaction Screen
// ----------------------------------------------------------------------------
class TransactionScreen : public ATMScreen {
public:
    TransactionScreen(sf::Font& font, ATMContext& ctx, const std::string& title, 
                      bool showAmount = true) 
        : font_(font), ctx_(ctx), title_(title), showAmount_(showAmount) {}

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (showAmount_) {
                if (e.key.code >= sf::Keyboard::Num0 && e.key.code <= sf::Keyboard::Num9) {
                    if (amountStr_.size() < 10) amountStr_ += ('0' + (e.key.code - sf::Keyboard::Num0));
                } else if (e.key.code == sf::Keyboard::Period) {
                    if (amountStr_.find('.') == std::string::npos && !amountStr_.empty()) {
                        amountStr_ += '.';
                    }
                } else if (e.key.code == sf::Keyboard::BackSpace) {
                    if (!amountStr_.empty()) amountStr_.pop_back();
                } else if (e.key.code == sf::Keyboard::Enter) {
                    confirm();
                }
            }
            if (e.key.code == sf::Keyboard::Escape) {
                ctx_.nextState = ATMState::MainMenu;
            }
        }

        if (e.type == sf::Event::MouseButtonPressed) {
            if (confirmBtn_.contains(e.mouseButton.x, e.mouseButton.y)) {
                sound::click();
                justPressed_ = true;
                pressClock_.restart();
                confirm();
            }
            if (cancelBtn_.contains(e.mouseButton.x, e.mouseButton.y)) {
                sound::click();
                ctx_.nextState = ATMState::MainMenu;
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        // Keep transaction controls below the physical ATM header/card slot.
        // The common ATM frame is drawn first, so this prevents the transaction
        // UI from overlapping "BANK ATM", the card slot, and "INSERT CARD".
        sf::Color accent = accentFor(title_);
        drawBadge(window, {WIN_W / 2.f - 90.f, 240.f}, 16.f, accent, title_ == "DEPOSIT");

        sf::Text title(title_, font_, 22);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ATM_TEXT);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 227.f);
        window.draw(title);

        if (showAmount_) {
            // Amount display
            sf::Text label("Enter amount (Rs.)", font_, 14);
            label.setFillColor(ATM_TEXT_DIM);
            label.setPosition((WIN_W - label.getLocalBounds().width) / 2.f, 261.f);
            window.draw(label);

            sf::Text amount(amountStr_.empty() ? "0.00" : amountStr_, font_, 32);
            amount.setStyle(sf::Text::Bold);
            amount.setFillColor(ATM_ACCENT);
            amount.setPosition((WIN_W - amount.getLocalBounds().width) / 2.f, 284.f);
            window.draw(amount);

            // Live "balance after" preview — updates as the amount is typed,
            // rather than only being checked on Confirm.
            if (!amountStr_.empty()) {
                try {
                    double amt = std::stod(amountStr_);
                    auto bal = ctx_.bank->getBalance(ctx_.accountNumber);
                    if (bal.has_value()) {
                        double after = (title_ == "WITHDRAW") ? bal.value() - amt : bal.value() + amt;
                        std::string preview = "Balance after: Rs " + money(after);
                        sf::Text bt(preview, font_, 12);
                        bt.setFillColor(after < 0.0 ? ATM_DANGER : ATM_TEXT_DIM);
                        bt.setPosition((WIN_W - bt.getLocalBounds().width) / 2.f, 322.f);
                        window.draw(bt);
                    }
                } catch (...) {
                    // Incomplete input (e.g. just "."); skip the preview this frame.
                }
            }

            // Quick amount buttons
            std::vector<double> quickAmounts = {100, 500, 1000, 2000, 5000};
            const float qx = 119.f, qy = 344.f, qw = 82.f, qh = 42.f, qg = 8.f;
            quickRects_.clear();

            for (size_t i = 0; i < quickAmounts.size(); ++i) {
                float x = qx + i * (qw + qg);
                sf::FloatRect rect(x, qy, qw, qh);
                quickRects_.push_back(rect);

                sf::Color bg(accent.r / 4, accent.g / 4, accent.b / 4);
                sf::Color outline(accent.r, accent.g, accent.b, 140);
                sf::Vector2f mouse =
                    window.mapPixelToCoords(sf::Mouse::getPosition(window));
                if (rect.contains(mouse)) { bg = sf::Color(accent.r / 2, accent.g / 2, accent.b / 2); outline = accent; }

                ui::drawRoundedRect(window, rect, 8.f, bg, outline, 1.5f);

                // Use ASCII "Rs" so the currency label cannot render as garbled
                // characters with the current SFML/font setup.
                sf::Text quickLabel(
                    "Rs " + std::to_string((int)quickAmounts[i]), font_, 13);
                quickLabel.setFillColor(ATM_TEXT);
                quickLabel.setPosition(
                    x + (qw - quickLabel.getLocalBounds().width) / 2.f,
                    qy + (qh - quickLabel.getLocalBounds().height) / 2.f - 2.f
                );
                window.draw(quickLabel);
            }

            // Action buttons
            const float btnY = 414.f;
            sf::FloatRect confirmRect(155.f, btnY, 145.f, 48.f);
            sf::FloatRect cancelRect(325.f, btnY, 145.f, 48.f);
            confirmBtn_ = confirmRect;
            cancelBtn_ = cancelRect;

            // Confirm — brief scale-pulse right after being pressed.
            float pressT = pressClock_.getElapsedTime().asSeconds();
            float pulse = (justPressed_ && pressT < 0.15f) ? (1.f - pressT / 0.15f) * 6.f : 0.f;
            sf::FloatRect confirmDraw(confirmRect.left - pulse / 2.f, confirmRect.top - pulse / 2.f,
                                       confirmRect.width + pulse, confirmRect.height + pulse);
            sf::Color confirmBg = ATM_SUCCESS;
            sf::Vector2f mouse =
                window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (confirmRect.contains(mouse))
                confirmBg = sf::Color(60, 220, 130);

            ui::drawRoundedRect(window, confirmDraw, 10.f, confirmBg);

            sf::Text confirmText("Confirm", font_, 16);
            confirmText.setStyle(sf::Text::Bold);
            confirmText.setFillColor(ATM_TEXT);
            confirmText.setPosition(
                confirmRect.left +
                    (confirmRect.width - confirmText.getLocalBounds().width) / 2.f,
                confirmRect.top +
                    (confirmRect.height - confirmText.getLocalBounds().height) / 2.f - 2.f
            );
            window.draw(confirmText);

            // Cancel
            sf::Color cancelBg = sf::Color(60, 40, 40);
            if (cancelRect.contains(mouse))
                cancelBg = sf::Color(80, 50, 50);

            ui::drawRoundedRect(window, cancelRect, 10.f, cancelBg);

            sf::Text cancelText("Cancel", font_, 16);
            cancelText.setStyle(sf::Text::Bold);
            cancelText.setFillColor(ATM_TEXT);
            cancelText.setPosition(
                cancelRect.left +
                    (cancelRect.width - cancelText.getLocalBounds().width) / 2.f,
                cancelRect.top +
                    (cancelRect.height - cancelText.getLocalBounds().height) / 2.f - 2.f
            );
            window.draw(cancelText);

            // Error
            if (!error_.empty()) {
                sf::Text err(error_, font_, 14);
                err.setFillColor(ATM_DANGER);
                err.setPosition(
                    (WIN_W - err.getLocalBounds().width) / 2.f, 479.f);
                window.draw(err);
            }
        }
    }

protected:
    virtual void confirm() {
        if (amountStr_.empty()) {
            error_ = "Please enter an amount";
            return;
        }
        try {
            ctx_.transactionAmount = std::stod(amountStr_);
            if (ctx_.transactionAmount <= 0) {
                error_ = "Amount must be positive";
                return;
            }
            // Will be overridden by derived classes
        } catch (...) {
            error_ = "Invalid amount";
        }
    }

    sf::Font& font_;
    ATMContext& ctx_;
    std::string title_;
    bool showAmount_;
    std::string amountStr_;
    std::string error_;
    sf::FloatRect confirmBtn_, cancelBtn_;
    std::vector<sf::FloatRect> quickRects_;
    bool justPressed_ = false;
    sf::Clock pressClock_;
};

// ----------------------------------------------------------------------------
// Balance Inquiry Screen
// ----------------------------------------------------------------------------
class BalanceInquiryScreen : public ATMScreen {
public:
    BalanceInquiryScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {}

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code == sf::Keyboard::Enter || e.key.code == sf::Keyboard::Escape) {
                ctx_.nextState = ATMState::MainMenu;
            }
        }
        if (e.type == sf::Event::MouseButtonPressed) {
            if (doneBtn_.contains(e.mouseButton.x, e.mouseButton.y)) {
            sound::click();
                ctx_.nextState = ATMState::MainMenu;
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        drawBadge(window, {WIN_W / 2.f - 140.f, 178.f}, 15.f, accentFor("BALANCE INQUIRY"), true);

        sf::Text title("BALANCE INQUIRY", font_, 22);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ATM_TEXT);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 168.f);
        window.draw(title);

        // Balance display card — starts below the title, same clear-zone
        // convention TransactionScreen uses, so it can't collide with the
        // physical header/card-slot chrome drawn by drawATMFrame.
        sf::FloatRect cardRect(60.f, 210.f, WIN_W - 120.f, 190.f);
        ui::drawRoundedRect(window, cardRect, 16.f, sf::Color(40, 60, 100));

        sf::Text label("Available Balance", font_, 16);
        label.setFillColor(ATM_TEXT_DIM);
        label.setPosition((WIN_W - label.getLocalBounds().width) / 2.f, 240.f);
        window.draw(label);

        auto balance = ctx_.bank->getBalance(ctx_.accountNumber);
        std::string balStr = balance.has_value() ? money(balance.value()) : "0.00";
        sf::Text amount(utf8("₨ " + balStr), font_, 44);
        amount.setStyle(sf::Text::Bold);
        amount.setFillColor(ATM_ACCENT);
        amount.setPosition((WIN_W - amount.getLocalBounds().width) / 2.f, 285.f);
        window.draw(amount);

        // Done button — moved down to clear the taller card, and correctly
        // centered on the window (was off-center by 100px before).
        sf::FloatRect doneRect((WIN_W - 160.f) / 2.f, 430.f, 160.f, 50.f);
        doneBtn_ = doneRect;
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::Color bg = ATM_BTN_BG;
        if (doneRect.contains(mouse)) bg = ATM_BTN_HOVER;
        ui::drawRoundedRect(window, doneRect, 10.f, bg);
        sf::Text doneText("OK", font_, 18);
        doneText.setStyle(sf::Text::Bold);
        doneText.setFillColor(ATM_TEXT);
        doneText.setPosition(
            doneRect.left + (doneRect.width - doneText.getLocalBounds().width) / 2.f,
            doneRect.top + (doneRect.height - doneText.getLocalBounds().height) / 2.f - 2.f
        );
        window.draw(doneText);
    }

private:
    sf::Font& font_;
    ATMContext& ctx_;
    sf::FloatRect doneBtn_;
};

// ----------------------------------------------------------------------------
// Deposit Screen
// ----------------------------------------------------------------------------
class DepositScreen : public TransactionScreen {
public:
    DepositScreen(sf::Font& font, ATMContext& ctx) 
        : TransactionScreen(font, ctx, "DEPOSIT", true) {}

protected:
    void confirm() override {
        TransactionScreen::confirm();
        if (!error_.empty()) return;
        
        auto result = ctx_.bank->deposit(ctx_.accountNumber, ctx_.transactionAmount);
        if (result.ok) {
            ctx_.transactionMessage = "Deposit successful!\n₨ " + money(ctx_.transactionAmount) + " added";
            ctx_.isSuccess = true;
        } else {
            ctx_.transactionMessage = "Deposit failed:\n" + result.message;
            ctx_.isSuccess = false;
        }
        ctx_.nextState = ATMState::Processing;
    }
};

// ----------------------------------------------------------------------------
// Withdraw Screen
// ----------------------------------------------------------------------------
class WithdrawScreen : public TransactionScreen {
public:
    WithdrawScreen(sf::Font& font, ATMContext& ctx) 
        : TransactionScreen(font, ctx, "WITHDRAW", true) {}

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        // Handle quick amount clicks
        if (e.type == sf::Event::MouseButtonPressed) {
            std::vector<double> quickAmounts = {100, 500, 1000, 2000, 5000};
            for (size_t i = 0; i < quickRects_.size() && i < quickAmounts.size(); ++i) {
                if (quickRects_[i].contains(e.mouseButton.x, e.mouseButton.y)) {
                sound::click();
                    amountStr_ = std::to_string((int)quickAmounts[i]);
                    // Auto-confirm for quick amounts
                    confirm();
                    return;
                }
            }
        }
        TransactionScreen::handleEvent(e, window);
    }

protected:
    void confirm() override {
        TransactionScreen::confirm();
        if (!error_.empty()) return;

        auto balance = ctx_.bank->getBalance(ctx_.accountNumber);
        if (balance.has_value() && ctx_.transactionAmount > balance.value()) {
            error_ = "Insufficient balance";
            return;
        }

        auto result = ctx_.bank->withdraw(ctx_.accountNumber, ctx_.transactionAmount);
        if (result.ok) {
            ctx_.transactionMessage = "Withdrawal successful!\n₨ " + money(ctx_.transactionAmount) + " dispensed";
            ctx_.isSuccess = true;
        } else {
            ctx_.transactionMessage = "Withdrawal failed:\n" + result.message;
            ctx_.isSuccess = false;
        }
        ctx_.nextState = ATMState::Processing;
    }
};

// ----------------------------------------------------------------------------
// Mini Statement Screen
// ----------------------------------------------------------------------------
class MiniStatementScreen : public ATMScreen {
public:
    MiniStatementScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {
        ctx_.miniStatementTxns = ctx_.bank->miniStatement(ctx_.accountNumber, 5);
    }

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code == sf::Keyboard::Enter || e.key.code == sf::Keyboard::Escape) {
                ctx_.nextState = ATMState::MainMenu;
            }
        }
        if (e.type == sf::Event::MouseButtonPressed) {
            if (doneBtn_.contains(e.mouseButton.x, e.mouseButton.y)) {
            sound::click();
                ctx_.nextState = ATMState::MainMenu;
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        // Content here must stay below the physical header (BANK ATM plaque,
        // card slot, "INSERT CARD") drawn by drawATMFrame — same clear zone
        // every other screen respects.
        sf::Text title("MINI STATEMENT", font_, 22);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ATM_TEXT);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 175.f);
        window.draw(title);

        // Table header
        float y = 215.f;
        float cols[] = {30.f, 100.f, 130.f, 100.f};
        std::string headers[] = {"#", "Date", "Type", "Amount"};

        sf::FloatRect headerRect(45.f, y, 560.f, 30.f);
        ui::drawRoundedRect(window, headerRect, 6.f, sf::Color(40, 60, 100));

        float x = 55.f;
        for (int i = 0; i < 4; ++i) {
            sf::Text h(headers[i], font_, 12);
            h.setStyle(sf::Text::Bold);
            h.setFillColor(ATM_TEXT_DIM);
            h.setPosition(x, y + 6.f);
            window.draw(h);
            x += cols[i];
        }

        // Transactions
        y += 40.f;
        int count = 0;
        for (const auto& txn : ctx_.miniStatementTxns) {
            if (count >= 5) break;
            sf::Color bg = (count % 2 == 0) ? sf::Color(30, 45, 75) : sf::Color(35, 55, 90);
            sf::FloatRect rowRect(45.f, y + count * 36.f, 560.f, 32.f);
            ui::drawRoundedRect(window, rowRect, 4.f, bg);

            x = 55.f;
            sf::Text num(std::to_string(count + 1), font_, 12);
            num.setFillColor(ATM_TEXT_DIM);
            num.setPosition(x, y + count * 36.f + 8.f);
            window.draw(num);
            x += cols[0];

            sf::Text date(txn.timestamp.substr(5, 11), font_, 12);
            date.setFillColor(ATM_TEXT);
            date.setPosition(x, y + count * 36.f + 8.f);
            window.draw(date);
            x += cols[1];

            bool isDeposit = (txn.type == "Deposit");
            drawBadge(window, {x + 8.f, y + count * 36.f + 16.f}, 8.f,
                      isDeposit ? sf::Color(46, 204, 113) : sf::Color(230, 160, 60), isDeposit);

            sf::Text type(txn.type, font_, 12);
            type.setFillColor(ATM_TEXT);
            type.setPosition(x + 22.f, y + count * 36.f + 8.f);
            window.draw(type);
            x += cols[2];

            std::string amtStr = (txn.type == "Deposit" ? "+" : "") + money(txn.amount);
            sf::Text amt(amtStr, font_, 12);
            amt.setFillColor(txn.type == "Deposit" ? ATM_SUCCESS : ATM_TEXT);
            amt.setPosition(x, y + count * 36.f + 8.f);
            window.draw(amt);

            count++;
        }

        if (ctx_.miniStatementTxns.empty()) {
            sf::Text empty("No transactions found", font_, 16);
            empty.setFillColor(ATM_TEXT_DIM);
            empty.setPosition((WIN_W - empty.getLocalBounds().width) / 2.f, y + 60.f);
            window.draw(empty);
        }

        // Done button
        float doneY = y + 190.f;
        sf::FloatRect doneRect(160.f, doneY, 160.f, 50.f);
        doneBtn_ = doneRect;
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::Color bg = ATM_BTN_BG;
        if (doneRect.contains(mouse)) bg = ATM_BTN_HOVER;
        ui::drawRoundedRect(window, doneRect, 10.f, bg);
        sf::Text doneText("OK", font_, 18);
        doneText.setStyle(sf::Text::Bold);
        doneText.setFillColor(ATM_TEXT);
        doneText.setPosition(
            doneRect.left + (doneRect.width - doneText.getLocalBounds().width) / 2.f,
            doneRect.top + (doneRect.height - doneText.getLocalBounds().height) / 2.f - 2.f
        );
        window.draw(doneText);
    }

private:
    sf::Font& font_;
    ATMContext& ctx_;
    sf::FloatRect doneBtn_;
};

// ----------------------------------------------------------------------------
// Change PIN Screen
// ----------------------------------------------------------------------------
class ChangePINScreen : public ATMScreen {
public:
    ChangePINScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {}

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code >= sf::Keyboard::Num0 && e.key.code <= sf::Keyboard::Num9) {
                int digit = e.key.code - sf::Keyboard::Num0;
                if (state_ == 0 && oldPin_.size() < 4) oldPin_ += ('0' + digit);
                else if (state_ == 1 && newPin_.size() < 4) newPin_ += ('0' + digit);
                else if (state_ == 2 && confirmPin_.size() < 4) confirmPin_ += ('0' + digit);
            } else if (e.key.code == sf::Keyboard::BackSpace) {
                if (state_ == 0 && !oldPin_.empty()) oldPin_.pop_back();
                else if (state_ == 1 && !newPin_.empty()) newPin_.pop_back();
                else if (state_ == 2 && !confirmPin_.empty()) confirmPin_.pop_back();
            } else if (e.key.code == sf::Keyboard::Enter) {
                if (state_ == 0 && oldPin_.size() == 4) {
                    state_ = 1;
                    error_.clear();
                } else if (state_ == 1 && newPin_.size() == 4) {
                    state_ = 2;
                    error_.clear();
                } else if (state_ == 2 && confirmPin_.size() == 4) {
                    confirm();
                }
            } else if (e.key.code == sf::Keyboard::Escape) {
                ctx_.nextState = ATMState::MainMenu;
            }
        }

        if (e.type == sf::Event::MouseButtonPressed) {
            if (cancelBtn_.contains(e.mouseButton.x, e.mouseButton.y)) {
            sound::click();
                ctx_.nextState = ATMState::MainMenu;
            }
        }
    }

    void draw(sf::RenderWindow& window) override {
        sf::Text title("CHANGE PIN", font_, 22);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ATM_TEXT);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 168.f);
        window.draw(title);

        std::vector<std::string> labels = {"Enter current PIN", "Enter new PIN", "Confirm new PIN"};
        std::string* pins[] = {&oldPin_, &newPin_, &confirmPin_};
        const float startY = 214.f;
        const float rowGap = 76.f;

        for (int i = 0; i < 3; ++i) {
            float y = startY + i * rowGap;
            bool active = (i == state_);

            sf::Text label(labels[i], font_, 14);
            label.setFillColor(active ? ATM_ACCENT : ATM_TEXT_DIM);
            label.setPosition((WIN_W - label.getLocalBounds().width) / 2.f, y);
            window.draw(label);

            // PIN dots
            for (int j = 0; j < 4; ++j) {
                sf::CircleShape dot(8.f);
                dot.setPosition(WIN_W / 2.f - 80.f + j * 44.f, y + 30.f);
                if (j < static_cast<int>(pins[i]->size())) {
                    dot.setFillColor(active ? ATM_ACCENT : ATM_TEXT_DIM);
                } else {
                    dot.setFillColor(sf::Color(60, 80, 120));
                    dot.setOutlineColor(active ? sf::Color(100, 140, 200) : sf::Color(40, 60, 90));
                    dot.setOutlineThickness(2.f);
                }
                window.draw(dot);
            }
        }

        // Error
        if (!error_.empty()) {
            sf::Text err(error_, font_, 14);
            err.setFillColor(ATM_DANGER);
            err.setPosition((WIN_W - err.getLocalBounds().width) / 2.f, 430.f);
            window.draw(err);
        }

        // Cancel button
        sf::FloatRect cancelRect(145.f, 474.f, 190.f, 46.f);
        cancelBtn_ = cancelRect;
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        sf::Color bg = sf::Color(60, 40, 40);
        if (cancelRect.contains(mouse)) bg = sf::Color(80, 50, 50);
        ui::drawRoundedRect(window, cancelRect, 10.f, bg);
        sf::Text cancelText("Cancel", font_, 16);
        cancelText.setStyle(sf::Text::Bold);
        cancelText.setFillColor(ATM_TEXT);
        cancelText.setPosition(
            cancelRect.left + (cancelRect.width - cancelText.getLocalBounds().width) / 2.f,
            cancelRect.top + (cancelRect.height - cancelText.getLocalBounds().height) / 2.f - 2.f
        );
        window.draw(cancelText);
    }

private:
    void confirm() {
        if (newPin_ != confirmPin_) {
            error_ = "New PINs don't match";
            state_ = 1;
            newPin_.clear();
            confirmPin_.clear();
            return;
        }
        if (oldPin_ == newPin_) {
            error_ = "New PIN must be different";
            state_ = 1;
            newPin_.clear();
            confirmPin_.clear();
            return;
        }

        auto result = ctx_.bank->changePin(ctx_.accountNumber, oldPin_, newPin_);
        if (result.ok) {
            ctx_.transactionMessage = "PIN changed successfully!";
            ctx_.isSuccess = true;
        } else {
            ctx_.transactionMessage = "PIN change failed:\n" + result.message;
            ctx_.isSuccess = false;
        }
        ctx_.nextState = ATMState::Processing;
    }

    sf::Font& font_;
    ATMContext& ctx_;
    std::string oldPin_, newPin_, confirmPin_;
    int state_ = 0;
    std::string error_;
    sf::FloatRect cancelBtn_;
};

// ----------------------------------------------------------------------------
// Processing Screen — a brief "working" beat between confirming a
// transaction and showing its result. ctx_.isSuccess / ctx_.transactionMessage
// are already set by whichever screen triggered this (Deposit/Withdraw/
// ChangePIN); this screen just waits, then hands off to the result screen.
// Intentionally ignores input — a real ATM doesn't let you cancel mid-process.
// ----------------------------------------------------------------------------
class ProcessingScreen : public ATMScreen {
public:
    ProcessingScreen(sf::Font& font, ATMContext& ctx) : font_(font), ctx_(ctx) {}

    void update(float dt) override {
        timer_ += dt;
        if (timer_ >= kDuration) {
            ctx_.nextState = ctx_.isSuccess ? ATMState::TransactionSuccess : ATMState::TransactionFailed;
        }
    }

    void handleEvent(const sf::Event&, sf::RenderWindow&) override {}

    void draw(sf::RenderWindow& window) override {
        sf::Text title("PROCESSING", font_, 22);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ATM_TEXT);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 168.f);
        window.draw(title);

        sf::Text sub("Please wait...", font_, 14);
        sub.setFillColor(ATM_TEXT_DIM);
        sub.setPosition((WIN_W - sub.getLocalBounds().width) / 2.f, 202.f);
        window.draw(sub);

        // Three pulsing dots.
        float startX = WIN_W / 2.f - 24.f;
        for (int i = 0; i < 3; ++i) {
            float phase = timer_ * 6.f - static_cast<float>(i) * 0.6f;
            float pulse = 0.5f + 0.5f * std::sin(phase);
            sf::CircleShape dot(4.f + pulse * 2.5f);
            dot.setFillColor(ATM_ACCENT);
            dot.setPosition(startX + i * 24.f, 250.f);
            window.draw(dot);
        }
    }

private:
    sf::Font& font_;
    ATMContext& ctx_;
    float timer_ = 0.f;
    static constexpr float kDuration = 1.0f;
};

// ----------------------------------------------------------------------------
// Transaction Result Screen (Success/Failure)
// ----------------------------------------------------------------------------
class TransactionResultScreen : public ATMScreen {
public:
    TransactionResultScreen(sf::Font& font, ATMContext& ctx, CashDispenser& dispenser)
        : font_(font), ctx_(ctx), dispenser_(dispenser) {
        if (ctx_.isSuccess) {
            dispenser_.trigger();
            sound::success();
        }
        timer_ = 0.f;
        showCash_ = ctx_.isSuccess;
    }

    void update(float dt) override {
        timer_ += dt;
        if (timer_ > 3.0f && !done_) {
            done_ = true;
        }
    }

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override {
        if (done_ && (e.type == sf::Event::KeyPressed || e.type == sf::Event::MouseButtonPressed)) {
            ctx_.nextState = ATMState::MainMenu;
        }
    }

    void draw(sf::RenderWindow& window) override {
        // Content here must stay below the physical header (BANK ATM plaque,
        // card slot, "INSERT CARD") drawn by drawATMFrame, and above the
        // CASH/RECEIPT slot near the bottom — same clear zone every other
        // screen (MainMenuScreen, TransactionScreen, etc.) already respects.

        // Icon
        sf::CircleShape icon(40.f);
        icon.setPosition((WIN_W - 80.f) / 2.f, 175.f);
        icon.setFillColor(ctx_.isSuccess ? ATM_SUCCESS : ATM_DANGER);
        window.draw(icon);

        sf::Text symbol(utf8(ctx_.isSuccess ? "✓" : "✗"), font_, 50);
        symbol.setStyle(sf::Text::Bold);
        symbol.setFillColor(ATM_TEXT);
        symbol.setPosition((WIN_W - symbol.getLocalBounds().width) / 2.f, 170.f);
        window.draw(symbol);

        // Title
        sf::Text title(ctx_.isSuccess ? "SUCCESS" : "FAILED", font_, 24);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(ctx_.isSuccess ? ATM_SUCCESS : ATM_DANGER);
        title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 265.f);
        window.draw(title);

        // Message
        sf::Text msg(utf8(ctx_.transactionMessage), font_, 16);
        msg.setFillColor(ATM_TEXT);
        msg.setPosition((WIN_W - msg.getLocalBounds().width) / 2.f, 300.f);
        window.draw(msg);

        // Cash dispenser animation (for successful withdrawals)
        if (ctx_.isSuccess && ctx_.nextState != ATMState::MainMenu) {
            dispenser_.setPosition(90.f, 345.f);
            dispenser_.setSize(300.f, 150.f);
            dispenser_.draw(window);
        }

        // Continue hint
        if (done_) {
            float alpha = std::sin(timer_ * 3.f) * 0.3f + 0.7f;
            sf::Text cont("Press any key to continue", font_, 14);
            cont.setFillColor(sf::Color(180, 190, 210, static_cast<sf::Uint8>(alpha * 255)));
            cont.setPosition((WIN_W - cont.getLocalBounds().width) / 2.f, 505.f);
            window.draw(cont);
        }
    }

    bool isDone() const override { return done_; }

private:
    sf::Font& font_;
    ATMContext& ctx_;
    CashDispenser& dispenser_;
    float timer_ = 0.f;
    bool done_ = false;
    bool showCash_ = true;
};

} // namespace

// ============================================================================
// Main ATM UI Entry Point
// ============================================================================

void RunATMUI(Bank& bank) {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                            "ATM", sf::Style::Titlebar | sf::Style::Close, settings);
    window.setFramerateLimit(60);

    sf::Font font;
    try {
        font = loadFontOrThrow("DejaVuSans.ttf");
    } catch (...) {
        return;
    }

    ATMContext ctx;
    ctx.bank = &bank;

    // ----- DEFAULT ACCOUNT: pre-fills the account-number field as a demo hint -----
    const long long DEFAULT_ACCOUNT = 100000001;
    // -------------------------------------------------------------------------

    CashDispenser dispenser(font);

    // Every session starts at the login screen: account number first, then
    // PIN, before anything else is reachable. No auto-login — this is what
    // lets a different account be used after logout instead of always
    // landing back on the same one.
    std::unique_ptr<ATMScreen> currentScreen = std::make_unique<CardInsertScreen>(font, ctx);
    ATMState currentState = ATMState::Idle;  // tracked for the header subtitle
    bool enteringAccount = true;
    ctx.nextState = ATMState::Idle;

    sf::Clock clock;
    std::string accStr = std::to_string(DEFAULT_ACCOUNT);
    std::string accountEntryError;

    // Idle timeout: resets on any input while authenticated. Past kIdleWarn
    // seconds, a countdown banner appears; past kIdleLogout, the session
    // ends automatically — same as a real ATM returning to the card screen
    // after inactivity.
    sf::Clock idleClock;
    constexpr float kIdleWarn = 20.f;
    constexpr float kIdleLogout = 25.f;

    // Brief card-eject animation played on logout, before the account-entry
    // overlay takes over. Purely visual — doesn't block input.
    bool ejectingCard = false;
    sf::Clock ejectClock;
    constexpr float kEjectDuration = 0.6f;

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (!enteringAccount && (event.type == sf::Event::KeyPressed ||
                                      event.type == sf::Event::MouseButtonPressed)) {
                idleClock.restart();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape && !enteringAccount) {
                ATMState backState = escapeTargetFor(currentState);
                if (backState != currentState) {
                    ctx.nextState = backState;
                }
            }
            if (enteringAccount) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code >= sf::Keyboard::Num0 && event.key.code <= sf::Keyboard::Num9) {
                        if (accStr.size() < 12) accStr += ('0' + (event.key.code - sf::Keyboard::Num0));
                    } else if (event.key.code == sf::Keyboard::BackSpace) {
                        if (!accStr.empty()) accStr.pop_back();
                    } else if (event.key.code == sf::Keyboard::Enter) {
                        try {
                            long long enteredAccount = std::stoll(accStr);
                            // Try to load the customer
                            auto found = bank.findByAccountNumber(enteredAccount);
                            if (found.has_value()) {
                                if (found->status != "Active") {
                                    accountEntryError = "Account is not active.";
                                    accStr = std::to_string(DEFAULT_ACCOUNT);
                                } else {
                                    sound::cardInsert();
                                    idleClock.restart();
                                    ctx.accountNumber = enteredAccount;
                                    ctx.currentCustomer = found.value();
                                    accountEntryError.clear();
                                    // Account number alone isn't enough — hand off
                                    // to the PIN screen next, same as the normal
                                    // state machine does everywhere else.
                                    enteringAccount = false;
                                    ctx.nextState = ATMState::EnteringPIN;
                                }
                            } else {
                                // account not found – keep overlay
                                accountEntryError = "Account not found.";
                                accStr = std::to_string(DEFAULT_ACCOUNT);
                            }
                        } catch (...) {}
                    }
                }
                // While entering account, we still draw the overlay, so skip normal handling.
                // We'll still process events for the overlay.
                // But we don't want to pass events to currentScreen because it's not the active screen.
                // Instead we handle the overlay events above.
                // We'll skip the rest of the event loop for now.
                // (We'll handle it below)
            } else {
                // Normal screen event handling
                currentScreen->handleEvent(event, window);
            }
        }

        if (!enteringAccount && !isActiveAccount(bank, ctx.accountNumber)) {
            ctx.nextState = ATMState::Logout;
        }

        // State transitions (only when not in account entry).
        // The state machine itself is the completion signal: individual
        // screens set ctx.nextState when the user makes a valid choice.
        // Do NOT gate this on ATMScreen::isDone(), because most screens
        // intentionally inherit the base implementation (false).
        if (!enteringAccount && ctx.nextState != ATMState::Idle) {
            ATMState newState = ctx.nextState;
            ctx.nextState = ATMState::Idle;
            currentState = newState;

            switch (newState) {
                case ATMState::EnteringPIN:
                    currentScreen = std::make_unique<PinEntryScreen>(font, ctx);
                    break;
                case ATMState::MainMenu:
                    currentScreen = std::make_unique<MainMenuScreen>(font, ctx);
                    break;
                case ATMState::BalanceInquiry:
                    currentScreen = std::make_unique<BalanceInquiryScreen>(font, ctx);
                    break;
                case ATMState::Deposit:
                    currentScreen = std::make_unique<DepositScreen>(font, ctx);
                    break;
                case ATMState::Withdraw:
                    currentScreen = std::make_unique<WithdrawScreen>(font, ctx);
                    break;
                case ATMState::MiniStatement:
                    currentScreen = std::make_unique<MiniStatementScreen>(font, ctx);
                    break;
                case ATMState::ChangePIN:
                    currentScreen = std::make_unique<ChangePINScreen>(font, ctx);
                    break;
                case ATMState::Processing:
                    currentScreen = std::make_unique<ProcessingScreen>(font, ctx);
                    break;
                case ATMState::TransactionSuccess:
                case ATMState::TransactionFailed:
                    currentScreen = std::make_unique<TransactionResultScreen>(font, ctx, dispenser);
                    break;
                case ATMState::Logout:
                    enteringAccount = true;
                    currentState = ATMState::Idle;
                    accStr = std::to_string(DEFAULT_ACCOUNT);
                    currentScreen = std::make_unique<CardInsertScreen>(font, ctx);
                    ejectingCard = true;
                    ejectClock.restart();
                    sound::cardInsert();  // reused for the eject mechanism sound
                    break;
                default:
                    break;
            }
        }

        // Idle timeout: past kIdleLogout seconds with no input while
        // authenticated, end the session automatically — same as a real ATM.
        if (!enteringAccount && idleClock.getElapsedTime().asSeconds() >= kIdleLogout) {
            ctx.nextState = ATMState::Logout;
        }

        float dt = clock.restart().asSeconds();
        if (!enteringAccount) {
            currentScreen->update(dt);
        }

        window.clear(ATM_BG);

        // Reference-style physical ATM body and common hardware. Shows
        // "INSERT CARD" only pre-authentication; once logged in, shows the
        // customer's name instead — this used to say "INSERT CARD" even
        // mid-session, which was simply wrong.
        std::string slotLabel = enteringAccount ? "INSERT CARD" : toUpperAscii(ctx.currentCustomer.name);
        drawATMFrame(window, font, screenTitleFor(currentState), slotLabel);

        // Draw account entry overlay if needed
        if (enteringAccount) {
            sf::RectangleShape overlay({WIN_W, WIN_H});
            overlay.setFillColor(sf::Color(0, 0, 0, 180));
            window.draw(overlay);

            sf::Text title("ENTER ACCOUNT NUMBER", font, 22);
            title.setStyle(sf::Text::Bold);
            title.setFillColor(ATM_TEXT);
            title.setPosition((WIN_W - title.getLocalBounds().width) / 2.f, 200.f);
            window.draw(title);

            sf::Text acc(accStr, font, 36);
            acc.setStyle(sf::Text::Bold);
            acc.setFillColor(ATM_ACCENT);
            acc.setPosition((WIN_W - acc.getLocalBounds().width) / 2.f, 260.f);
            window.draw(acc);

            sf::Text hint("Enter account number then press ENTER", font, 14);
            hint.setFillColor(ATM_TEXT_DIM);
            hint.setPosition((WIN_W - hint.getLocalBounds().width) / 2.f, 330.f);
            window.draw(hint);

            sf::Text demo("Demo: " + std::to_string(DEFAULT_ACCOUNT), font, 12);
            demo.setFillColor(ATM_TEXT_DIM);
            demo.setPosition((WIN_W - demo.getLocalBounds().width) / 2.f, 360.f);
            window.draw(demo);

            if (!accountEntryError.empty()) {
                sf::Text err(accountEntryError, font, 14);
                err.setFillColor(ATM_DANGER);
                err.setPosition((WIN_W - err.getLocalBounds().width) / 2.f, 390.f);
                window.draw(err);
            }
        } else {
            currentScreen->draw(window);

            // Idle countdown warning — shown only once we're past kIdleWarn,
            // so normal use never sees it.
            float idleT = idleClock.getElapsedTime().asSeconds();
            if (idleT >= kIdleWarn) {
                int secondsLeft = static_cast<int>(std::ceil(kIdleLogout - idleT));
                if (secondsLeft < 0) secondsLeft = 0;
                std::string msg = "Session ending in " + std::to_string(secondsLeft) + "s - touch screen to continue";
                sf::Text warn(msg, font, 13);
                warn.setStyle(sf::Text::Bold);
                float pulse = 0.6f + 0.4f * std::sin(idleT * 5.f);
                warn.setFillColor(sf::Color(255, 200, 60, static_cast<sf::Uint8>(pulse * 255)));
                sf::FloatRect band(0.f, 600.f, WIN_W, 26.f);
                ui::drawRoundedRect(window, band, 0.f, sf::Color(0, 0, 0, 140));
                warn.setPosition((WIN_W - warn.getLocalBounds().width) / 2.f, 604.f);
                window.draw(warn);
            }
        }

        // Card-eject animation on logout — a yellow card sliding up out of
        // the slot and fading, mirroring the insert direction in reverse.
        if (ejectingCard) {
            float t = ejectClock.getElapsedTime().asSeconds();
            if (t >= kEjectDuration) {
                ejectingCard = false;
            } else {
                float p = t / kEjectDuration;
                float slideY = 100.f - p * 70.f;
                sf::Uint8 alpha = static_cast<sf::Uint8>((1.f - p) * 255.f);
                sf::FloatRect card(258.f, slideY, 136.f, 29.f);
                ui::drawRoundedRect(window, card, 5.f, sf::Color(ATM_ACCENT.r, ATM_ACCENT.g, ATM_ACCENT.b, alpha));
            }
        }

        window.display();
    }
}