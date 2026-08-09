#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <vector>
#include <functional>

// ============================================================================
// Widgets.h
//
// CHANGES vs the previous version:
//   - Theme gained a `radius` field (corner radius used everywhere) and an
//     `accentHover` color for consistent hover states.
//   - Added drawRoundedRect() / makeRoundedRect() — SFML has no built-in
//     rounded-rect shape, so this builds one out of arced corner points.
//   - Added the Table widget: a proper column-aligned, header-row table with
//     row striping and selection, used anywhere we used to just print a
//     formatted string into a ListBox (Accounts, Transactions, Reports).
//   - Button / TextBox now carry small animation state (hoverT_ / focusT_)
//     so hover/focus transitions are smooth instead of instant.
//
// NOTE: This header is a best-effort reconstruction of the original
// Widgets.h based on how Widgets.cpp / UI.cpp use it. If your real header
// has extra members other files depend on, port those over — the public
// API surface here (constructors, setPosition/setSize/bounds, value(),
// setValue(), clear(), selectedIndex(), setItems()) matches what UI.cpp
// already calls, so it should drop in cleanly.
// ============================================================================

namespace ui {

struct Theme {
    // Frame
    sf::Color sidebarBg       {  38,  52, 110 };
    sf::Color sidebarSelected {  62,  82, 158 };
    sf::Color contentBg       { 244, 246, 250 };
    sf::Color cardBg          { 255, 255, 255 };
    sf::Color border          { 226, 230, 238 };

    // Inputs
    sf::Color inputBg         { 255, 255, 255 };
    sf::Color inputFocused    {  86, 120, 236 };

    // Text
    sf::Color textDark        {  28,  32,  44 };
    sf::Color textDim         { 133, 141, 158 };
    sf::Color textLight       { 255, 255, 255 };

    // Accents / status
    sf::Color accent          {  64, 108, 238 };
    sf::Color accentHover     {  92, 132, 245 };
    sf::Color success         {  39, 174,  96 };
    sf::Color danger          { 224,  69,  69 };
    sf::Color warning         { 232, 160,  40 };

    // Table striping
    sf::Color rowAlt          { 247, 249, 252 };
    sf::Color rowSelected     { 222, 232, 253 };

    float radius              { 10.f };   // default corner radius for panels/buttons
    float radiusSmall         {  6.f };   // corner radius for inputs / rows
};

extern Theme theme;

// ---------------------------------------------------------------------------
// Sound effects — lazily loaded from assets/cash_dispense.wav on first use.
// Keeps a small round-robin pool of sf::Sound instances so rapid repeated
// plays don't cut each other off.
// ---------------------------------------------------------------------------
void playCashSound();

// ---------------------------------------------------------------------------
// Rounded-rect drawing helpers
// ---------------------------------------------------------------------------
sf::ConvexShape makeRoundedRect(sf::Vector2f size, float radius, int cornerSegments = 8);

void drawRoundedRect(sf::RenderWindow& window, sf::FloatRect rect, float radius,
                      sf::Color fill, sf::Color outline = sf::Color::Transparent,
                      float outlineThickness = 0.f);

// Soft drop shadow + rounded fill, used for cards/buttons that should "lift".
void drawElevatedRoundedRect(sf::RenderWindow& window, sf::FloatRect rect, float radius,
                              sf::Color fill, float elevation = 2.f);

// ---------------------------------------------------------------------------
// Base widget
// ---------------------------------------------------------------------------
class Widget {
public:
    virtual ~Widget() = default;
    virtual void handleEvent(const sf::Event&, sf::RenderWindow&) {}
    virtual void draw(sf::RenderWindow&) = 0;

    void setPosition(float x, float y) { bounds_.left = x; bounds_.top = y; }
    void setSize(float w, float h)     { bounds_.width = w; bounds_.height = h; }
    sf::FloatRect bounds() const       { return bounds_; }

protected:
    sf::FloatRect bounds_{};
};

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
class Label : public Widget {
public:
    Label(sf::Font& font, std::string text, unsigned size = 14,
          sf::Color color = sf::Color::Black, bool bold = false);

    void setText(const std::string& t) { text_ = t; }
    const std::string& text() const    { return text_; }

    void draw(sf::RenderWindow& window) override;

private:
    sf::Font& font_;
    std::string text_;
    unsigned size_;
    sf::Color color_;
    bool bold_;
};

// ---------------------------------------------------------------------------
// Button (rounded, animated hover)
// ---------------------------------------------------------------------------
class Button : public Widget {
public:
    Button(sf::Font& font, std::string label, std::function<void()> onClick,
           sf::Color bg, sf::Color fg);

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

private:
    sf::Font& font_;
    std::string label_;
    std::function<void()> onClick_;
    sf::Color bg_, fg_;
    bool hovered_ = false;
    float hoverT_ = 0.f;   // 0..1, eased toward hovered_ each frame
};

// ---------------------------------------------------------------------------
// TextBox (rounded, animated focus ring)
// ---------------------------------------------------------------------------
class TextBox : public Widget {
public:
    TextBox(sf::Font& font, std::string placeholder,
            bool digitsOnly = false, bool maskChars = false);

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

    std::string value() const     { return text_; }
    void setValue(const std::string& v) { text_ = v; }
    void clear()                  { text_.clear(); }

private:
    sf::Font& font_;
    std::string placeholder_;
    std::string text_;
    bool digitsOnly_;
    bool maskChars_;
    bool focused_ = false;
    float cursorBlink_ = 0.f;
    float focusT_ = 0.f;
};

// ---------------------------------------------------------------------------
// ListBox (kept for anything that's a plain single-column list)
// ---------------------------------------------------------------------------
class ListBox : public Widget {
public:
    ListBox(sf::Font& font, unsigned rowHeight);

    void setItems(std::vector<std::string> items);
    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

    int selectedIndex() const { return selected_; }

private:
    sf::Font& font_;
    unsigned rowHeight_;
    std::vector<std::string> items_;
    int selected_ = -1;
    int scrollOffset_ = 0;
};

// ---------------------------------------------------------------------------
// Table — the real replacement for hand-formatted text rows.
// Use this anywhere you were building a "col1 | col2 | col3" string.
// ---------------------------------------------------------------------------
struct TableColumn {
    std::string header;
    float width;            // px
    bool alignRight = false;
};

class Table : public Widget {
public:
    Table(sf::Font& font, sf::Font& headerFont,
          std::vector<TableColumn> columns, unsigned rowHeight = 34);

    // Each row is a vector of cell strings, one per column.
    // Optional per-row accent color (e.g. status pill color); pass
    // sf::Color::Transparent to use default text color.
    void setRows(std::vector<std::vector<std::string>> rows,
                 std::vector<sf::Color> rowAccents = {});

    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

    int selectedIndex() const { return selected_; }

private:
    sf::Font& font_;
    sf::Font& headerFont_;
    std::vector<TableColumn> columns_;
    unsigned rowHeight_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<sf::Color> rowAccents_;
    int selected_ = -1;
    int scrollOffset_ = 0;

    float totalWidth() const;
};

// ---------------------------------------------------------------------------
// CashDispenser — an illustrated ATM character (rounded body, blinking
// smiley screen, keypad) with a bill that hangs out of a slot and retracts
// automatically. Purely visual; advances its own timers inside draw() the
// same way Button/TextBox ease their hover/focus/cursor state each frame.
//
// Usage: call trigger() once a withdrawal succeeds. The widget then runs
// through Processing -> Dispensing -> Resting -> Retracting -> Idle on its
// own; nothing else needs to poll or update it. The face keeps blinking
// continuously regardless of state.
// ---------------------------------------------------------------------------
class CashDispenser : public Widget {
public:
    explicit CashDispenser(sf::Font& font);

    void trigger();
    void draw(sf::RenderWindow& window) override;

private:
    enum class State { Idle, Processing, Dispensing, Resting, Retracting };

    sf::Font& font_;
    State state_ = State::Idle;
    float t_ = 0.f;        // seconds elapsed in current state
    float idleT_ = 0.f;    // free-running clock, drives the blink regardless of state
};

// ---------------------------------------------------------------------------
// Static drawing helpers
// ---------------------------------------------------------------------------
void drawPanelBackground(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color border);

void drawCard(sf::RenderWindow& window, sf::Font& font, sf::FloatRect rect,
              const std::string& title, const std::string& value, sf::Color valueColor);

}  // namespace ui