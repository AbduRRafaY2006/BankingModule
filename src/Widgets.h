#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <vector>
#include <functional>

// ============================================================================
// Widgets.h
//
// REDESIGN NOTE (this pass):
//   Theme colors are now pulled directly from the ATM module's palette
//   (navy body / gold accent / green success / warm-orange warning) so the
//   admin dashboard and the ATM screen read as one product instead of two.
//   Added: drawBadge() (the ATM's circular up/down badge, now shared),
//   drawStatusPill(), drawNavIcon(), drawSectionCard() — all small,
//   composable helpers used to give every admin tab the same "designed"
//   quality the ATM screen has, instead of flat boxes of text.
//   TableColumn gained `statusPill` — when set, that column renders as a
//   colored pill (Active/Inactive/Locked) instead of plain text.
//   Table also renders a thin colored accent bar on the left edge of a row
//   when rowAccents_[i] is non-transparent, instead of recoloring the
//   entire row's text (restrained accent, not a wall of color).
//
// Everything else (constructors, setPosition/setSize/bounds, value(),
// setValue(), clear(), selectedIndex(), setItems()) is unchanged, so
// ATM.cpp / UI.cpp keep compiling against the same public surface.
// ============================================================================

namespace ui {

struct Theme {
    // Frame — matches ATM_BODY_DARK / ATM_BODY / ATM_BG
    sf::Color sidebarBg       {  14,  36,  70 };
    sf::Color sidebarSelected {  27,  57, 101 };
    sf::Color contentBg       { 245, 245, 245 };
    sf::Color cardBg          { 255, 255, 255 };
    sf::Color border          { 226, 230, 238 };

    // Inputs
    sf::Color inputBg         { 248, 249, 252 };
    sf::Color inputFocused    {  27,  57, 101 };

    // Text
    sf::Color textDark        {  27,  35,  55 };
    sf::Color textDim         { 130, 140, 158 };
    sf::Color textLight       { 235, 240, 245 };

    // Accents / status — matches ATM_ACCENT / ATM_SUCCESS / ATM_DANGER
    sf::Color accent          { 250, 196,  45 };   // gold
    sf::Color accentHover     { 255, 214,  90 };
    sf::Color accentDim       { 190, 145,  28 };
    sf::Color success         {  46, 204, 113 };
    sf::Color danger          { 231,  76,  60 };
    sf::Color warning         { 230, 160,  60 };

    // Table striping
    sf::Color rowAlt          { 248, 249, 252 };
    sf::Color rowSelected     { 250, 196,  45, 35 };

    float radius              { 14.f };
    float radiusSmall         { 10.f };
};

extern Theme theme;

// ---------------------------------------------------------------------------
// Sound effects
// ---------------------------------------------------------------------------
void playCashSound();

// ---------------------------------------------------------------------------
// Rounded-rect drawing helpers
// ---------------------------------------------------------------------------
sf::ConvexShape makeRoundedRect(sf::Vector2f size, float radius, int cornerSegments = 8);

void drawRoundedRect(sf::RenderWindow& window, sf::FloatRect rect, float radius,
                      sf::Color fill, sf::Color outline = sf::Color::Transparent,
                      float outlineThickness = 0.f);

void drawElevatedRoundedRect(sf::RenderWindow& window, sf::FloatRect rect, float radius,
                              sf::Color fill, float elevation = 2.f);

// ---------------------------------------------------------------------------
// Shared visual language (ported from ATM.cpp so both modules use the
// same drawing code instead of two copies drifting apart)
// ---------------------------------------------------------------------------

// Small circular badge with an up/down arrow — the ATM's per-screen icon,
// reused here on dashboard cards and table rows.
void drawBadge(sf::RenderWindow& window, sf::Vector2f center, float r, sf::Color color, bool up);

// Maps a customer status string to its theme color. Shared by Table's
// status-pill rendering and any code (accDetail, etc.) that needs the
// same color without duplicating the if/else chain.
sf::Color statusColor(const std::string& status);

// Rounded pill with colored outline + tinted fill, sized to its text.
// Returns the pill's width so callers can lay out things after it.
float drawStatusPill(sf::RenderWindow& window, sf::Font& font, sf::Vector2f pos, const std::string& status);

// Cheap glyph-style icon for sidebar nav items — no image assets required.
void drawNavIcon(sf::RenderWindow& window, sf::Vector2f center, int tabIndex, sf::Color color);

// A titled card panel: rounded background + small gold accent tick + label.
// Use to visually group a cluster of fields/buttons instead of leaving them
// floating on the bare content background.
void drawSectionCard(sf::RenderWindow& window, sf::Font& boldFont, sf::FloatRect rect,
                      const std::string& title, sf::Color accent);

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
    float hoverT_ = 0.f;
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
// ListBox
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
// Table
// ---------------------------------------------------------------------------
struct TableColumn {
    std::string header;
    float width;
    bool alignRight = false;
    bool statusPill = false;   // render this column's cells as a status pill
};

class Table : public Widget {
public:
    Table(sf::Font& font, sf::Font& headerFont,
          std::vector<TableColumn> columns, unsigned rowHeight = 34);

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
// CashDispenser (unchanged — ATM-only widget, kept for interface parity)
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
    float t_ = 0.f;
    float idleT_ = 0.f;
};

// ---------------------------------------------------------------------------
// Static drawing helpers
// ---------------------------------------------------------------------------
void drawPanelBackground(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color border);

// trendUp picks the badge arrow direction next to the metric title.
void drawCard(sf::RenderWindow& window, sf::Font& font, sf::FloatRect rect,
              const std::string& title, const std::string& value, sf::Color valueColor,
              bool trendUp = true);

}  // namespace ui