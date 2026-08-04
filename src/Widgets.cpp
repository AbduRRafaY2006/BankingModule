#include "Widgets.h"
#include <cctype>
#include <algorithm>
#include <cmath>

namespace ui {

Theme theme{};

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
Label::Label(sf::Font& font, std::string text, unsigned size, sf::Color color, bool bold)
    : font_(font), text_(std::move(text)), size_(size), color_(color), bold_(bold) {}

void Label::draw(sf::RenderWindow& window) {
    sf::Text t(text_, font_, size_);
    t.setFillColor(color_);
    if (bold_) t.setStyle(sf::Text::Bold);
    t.setPosition(bounds_.left, bounds_.top);
    window.draw(t);
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
Button::Button(sf::Font& font, std::string label, std::function<void()> onClick, sf::Color bg, sf::Color fg)
    : font_(font), label_(std::move(label)), onClick_(std::move(onClick)), bg_(bg), fg_(fg) {}

void Button::handleEvent(const sf::Event& e, sf::RenderWindow& window) {
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    hovered_ = bounds_.contains(mouse);
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        if (hovered_ && onClick_) onClick_();
    }
}

void Button::draw(sf::RenderWindow& window) {
    sf::RectangleShape rect({bounds_.width, bounds_.height});
    rect.setPosition(bounds_.left, bounds_.top);
    sf::Color fill = bg_;
    if (hovered_) {
        fill.r = static_cast<sf::Uint8>(std::min(255, fill.r + 20));
        fill.g = static_cast<sf::Uint8>(std::min(255, fill.g + 20));
        fill.b = static_cast<sf::Uint8>(std::min(255, fill.b + 20));
    }
    rect.setFillColor(fill);
    window.draw(rect);

    sf::Text t(label_, font_, 15);
    t.setFillColor(fg_);
    auto tb = t.getLocalBounds();
    t.setPosition(bounds_.left + (bounds_.width - tb.width) / 2.f - tb.left,
                  bounds_.top + (bounds_.height - tb.height) / 2.f - tb.top);
    window.draw(t);
}

// ---------------------------------------------------------------------------
// TextBox
// ---------------------------------------------------------------------------
TextBox::TextBox(sf::Font& font, std::string placeholder, bool digitsOnly, bool maskChars)
    : font_(font), placeholder_(std::move(placeholder)), digitsOnly_(digitsOnly), maskChars_(maskChars) {}

void TextBox::handleEvent(const sf::Event& e, sf::RenderWindow& window) {
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        focused_ = bounds_.contains(mouse);
    }
    if (!focused_) return;

    if (e.type == sf::Event::TextEntered) {
        auto ch = e.text.unicode;
        if (ch == 8) {  // backspace
            if (!text_.empty()) text_.pop_back();
        } else if (ch == 13 || ch == 9) {
            // Enter/Tab: release focus, don't insert.
            focused_ = false;
        } else if (ch >= 32 && ch < 127) {
            char c = static_cast<char>(ch);
            if (digitsOnly_ && !std::isdigit(static_cast<unsigned char>(c))) return;
            if (text_.size() < 64) text_ += c;
        }
    }
}

void TextBox::draw(sf::RenderWindow& window) {
    sf::RectangleShape rect({bounds_.width, bounds_.height});
    rect.setPosition(bounds_.left, bounds_.top);
    rect.setFillColor(theme.inputBg);
    rect.setOutlineThickness(focused_ ? 2.f : 1.f);
    rect.setOutlineColor(focused_ ? theme.inputFocused : theme.border);
    window.draw(rect);

    std::string display = text_;
    if (maskChars_ && !display.empty()) display = std::string(display.size(), '*');

    bool showPlaceholder = text_.empty() && !focused_;
    sf::Text t(showPlaceholder ? placeholder_ : display, font_, 15);
    t.setFillColor(showPlaceholder ? theme.textDim : theme.textDark);
    t.setPosition(bounds_.left + 8.f, bounds_.top + (bounds_.height - 18.f) / 2.f);
    window.draw(t);

    if (focused_) {
        cursorBlink_ += 0.016f;  // approx per-frame at ~60fps; good enough for a blink
        if (std::fmod(cursorBlink_, 1.0f) < 0.5f) {
            float textWidth = t.findCharacterPos(display.size()).x - t.getPosition().x;
            sf::RectangleShape cursor({1.5f, 18.f});
            cursor.setPosition(bounds_.left + 8.f + (showPlaceholder ? 0.f : textWidth),
                                bounds_.top + (bounds_.height - 18.f) / 2.f);
            cursor.setFillColor(theme.textDark);
            window.draw(cursor);
        }
    }
}

// ---------------------------------------------------------------------------
// ListBox
// ---------------------------------------------------------------------------
ListBox::ListBox(sf::Font& font, unsigned rowHeight) : font_(font), rowHeight_(rowHeight) {}

void ListBox::setItems(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_ >= static_cast<int>(items_.size())) selected_ = items_.empty() ? -1 : 0;
    scrollOffset_ = 0;
}

void ListBox::handleEvent(const sf::Event& e, sf::RenderWindow& window) {
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    if (e.type == sf::Event::MouseWheelScrolled && bounds_.contains(mouse)) {
        scrollOffset_ -= static_cast<int>(e.mouseWheelScroll.delta);
        int maxOffset = std::max(0, static_cast<int>(items_.size()) - static_cast<int>(bounds_.height / rowHeight_));
        scrollOffset_ = std::clamp(scrollOffset_, 0, maxOffset);
    }
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        if (bounds_.contains(mouse)) {
            int row = static_cast<int>((mouse.y - bounds_.top) / rowHeight_) + scrollOffset_;
            if (row >= 0 && row < static_cast<int>(items_.size())) selected_ = row;
        }
    }
}

void ListBox::draw(sf::RenderWindow& window) {
    drawPanelBackground(window, bounds_, theme.cardBg, theme.border);

    sf::View oldView = window.getView();
    sf::FloatRect vp = bounds_;
    // Clip to the list box area using a scissor-style view.
    sf::Vector2u winSize = window.getSize();
    sf::View clipView(sf::FloatRect(vp.left, vp.top, vp.width, vp.height));
    clipView.setViewport(sf::FloatRect(vp.left / winSize.x, vp.top / winSize.y,
                                        vp.width / winSize.x, vp.height / winSize.y));
    window.setView(clipView);

    int visibleRows = static_cast<int>(bounds_.height / rowHeight_) + 1;
    for (int i = 0; i < visibleRows; ++i) {
        int idx = scrollOffset_ + i;
        if (idx >= static_cast<int>(items_.size())) break;
        float y = bounds_.top + i * rowHeight_;
        if (idx == selected_) {
            sf::RectangleShape hi({bounds_.width, static_cast<float>(rowHeight_)});
            hi.setPosition(bounds_.left, y);
            hi.setFillColor(sf::Color(220, 232, 250));
            window.draw(hi);
        }
        sf::Text t(items_[idx], font_, 14);
        t.setFillColor(theme.textDark);
        t.setPosition(bounds_.left + 8.f, y + (rowHeight_ - 16.f) / 2.f);
        window.draw(t);
    }
    window.setView(oldView);
}

// ---------------------------------------------------------------------------
// Static drawing helpers
// ---------------------------------------------------------------------------
void drawPanelBackground(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color border) {
    sf::RectangleShape r({rect.width, rect.height});
    r.setPosition(rect.left, rect.top);
    r.setFillColor(fill);
    r.setOutlineThickness(1.f);
    r.setOutlineColor(border);
    window.draw(r);
}

void drawCard(sf::RenderWindow& window, sf::Font& font, sf::FloatRect rect,
              const std::string& title, const std::string& value, sf::Color valueColor) {
    drawPanelBackground(window, rect, theme.cardBg, theme.border);

    sf::Text titleText(title, font, 13);
    titleText.setFillColor(theme.textDim);
    titleText.setPosition(rect.left + 10.f, rect.top + 8.f);
    window.draw(titleText);

    sf::Text valueText(value, font, 26);
    valueText.setStyle(sf::Text::Bold);
    valueText.setFillColor(valueColor);
    auto vb = valueText.getLocalBounds();
    valueText.setPosition(rect.left + (rect.width - vb.width) / 2.f - vb.left,
                           rect.top + rect.height / 2.f - vb.height / 2.f + 4.f);
    window.draw(valueText);
}

}  // namespace ui
