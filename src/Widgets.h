#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <functional>

// ---------------------------------------------------------------------------
// A tiny hand-rolled widget set. FTXUI gave us Input/Button/Menu/layout for
// free; SFML gives us none of that, so this file is the "cost" of switching:
// every widget below (focus, cursor, click hit-testing, scrolling) has to be
// built and maintained by hand.
// ---------------------------------------------------------------------------

namespace ui {

struct Theme {
    sf::Color sidebarBg{37, 66, 150};
    sf::Color sidebarSelected{60, 96, 200};
    sf::Color contentBg{245, 247, 250};
    sf::Color cardBg{255, 255, 255};
    sf::Color border{210, 214, 222};
    sf::Color textDark{40, 44, 52};
    sf::Color textLight{255, 255, 255};
    sf::Color textDim{130, 138, 150};
    sf::Color accent{53, 122, 217};
    sf::Color success{46, 160, 90};
    sf::Color danger{210, 70, 70};
    sf::Color warning{220, 150, 40};
    sf::Color inputBg{255, 255, 255};
    sf::Color inputFocused{53, 122, 217};
};

extern Theme theme;

// A widget occupies a fixed rectangle set by the screen that owns it.
class Widget {
public:
    virtual ~Widget() = default;
    virtual void setPosition(float x, float y) { bounds_.left = x; bounds_.top = y; }
    virtual void setSize(float w, float h) { bounds_.width = w; bounds_.height = h; }
    sf::FloatRect bounds() const { return bounds_; }
    virtual void handleEvent(const sf::Event& e, sf::RenderWindow& window) = 0;
    virtual void draw(sf::RenderWindow& window) = 0;

protected:
    sf::FloatRect bounds_;
};

class Label : public Widget {
public:
    Label(sf::Font& font, std::string text, unsigned size = 16, sf::Color color = sf::Color::Black, bool bold = false);
    void setText(const std::string& t) { text_ = t; }
    void handleEvent(const sf::Event&, sf::RenderWindow&) override {}
    void draw(sf::RenderWindow& window) override;

private:
    sf::Font& font_;
    std::string text_;
    unsigned size_;
    sf::Color color_;
    bool bold_;
};

class Button : public Widget {
public:
    Button(sf::Font& font, std::string label, std::function<void()> onClick,
           sf::Color bg = theme.accent, sf::Color fg = theme.textLight);
    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

private:
    sf::Font& font_;
    std::string label_;
    std::function<void()> onClick_;
    sf::Color bg_, fg_;
    bool hovered_ = false;
};

// Single-line editable text field. Optional maskDigitsOnly restricts input
// to digits (used for PIN / amount / account-number fields).
class TextBox : public Widget {
public:
    TextBox(sf::Font& font, std::string placeholder, bool digitsOnly = false, bool maskChars = false);
    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

    const std::string& value() const { return text_; }
    void setValue(const std::string& v) { text_ = v; }
    void clear() { text_.clear(); }

private:
    sf::Font& font_;
    std::string placeholder_;
    std::string text_;
    bool digitsOnly_;
    bool maskChars_;
    bool focused_ = false;
    float cursorBlink_ = 0.f;
};

// Scrollable, click-to-select list of strings, rendered as fixed-width rows.
class ListBox : public Widget {
public:
    explicit ListBox(sf::Font& font, unsigned rowHeight = 26);
    void setItems(std::vector<std::string> items);
    int selectedIndex() const { return selected_; }
    void handleEvent(const sf::Event& e, sf::RenderWindow& window) override;
    void draw(sf::RenderWindow& window) override;

private:
    sf::Font& font_;
    std::vector<std::string> items_;
    int selected_ = -1;
    int scrollOffset_ = 0;
    unsigned rowHeight_;
};

// Draws a bordered card with a title and returns the inner content origin,
// used for dashboard summary tiles. Not a Widget (static, non-interactive).
void drawCard(sf::RenderWindow& window, sf::Font& font, sf::FloatRect rect,
              const std::string& title, const std::string& value, sf::Color valueColor);

void drawPanelBackground(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color border);

}  // namespace ui
