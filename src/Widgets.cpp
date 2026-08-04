#include "Widgets.h"
#include <cctype>
#include <algorithm>
#include <cmath>

namespace ui {

Theme theme{};

namespace {
constexpr float PI = 3.14159265358979323846f;

float lerp(float a, float b, float t) { return a + (b - a) * t; }

sf::Color lerpColor(sf::Color a, sf::Color b, float t) {
    return sf::Color(
        static_cast<sf::Uint8>(lerp(a.r, b.r, t)),
        static_cast<sf::Uint8>(lerp(a.g, b.g, t)),
        static_cast<sf::Uint8>(lerp(a.b, b.b, t)),
        static_cast<sf::Uint8>(lerp(a.a, b.a, t)));
}
}  // namespace

// ---------------------------------------------------------------------------
// Rounded-rect helpers
// ---------------------------------------------------------------------------
sf::ConvexShape makeRoundedRect(sf::Vector2f size, float radius, int cornerSegments) {
    radius = std::max(0.f, std::min({radius, size.x / 2.f, size.y / 2.f}));

    struct Corner { float cx, cy, startDeg; };
    Corner corners[4] = {
        { size.x - radius, radius,            -90.f },  // top-right
        { size.x - radius, size.y - radius,      0.f }, // bottom-right
        { radius,          size.y - radius,     90.f }, // bottom-left
        { radius,          radius,             180.f }, // top-left
    };

    std::vector<sf::Vector2f> points;
    points.reserve((cornerSegments + 1) * 4);
    for (auto& c : corners) {
        for (int i = 0; i <= cornerSegments; ++i) {
            float deg = c.startDeg + 90.f * static_cast<float>(i) / static_cast<float>(cornerSegments);
            float rad = deg * PI / 180.f;
            points.push_back({ c.cx + radius * std::cos(rad), c.cy + radius * std::sin(rad) });
        }
    }

    sf::ConvexShape shape;
    shape.setPointCount(points.size());
    for (size_t i = 0; i < points.size(); ++i) shape.setPoint(i, points[i]);
    return shape;
}

void drawRoundedRect(sf::RenderWindow& window, sf::FloatRect rect, float radius,
                      sf::Color fill, sf::Color outline, float outlineThickness) {
    auto shape = makeRoundedRect({rect.width, rect.height}, radius);
    shape.setPosition(rect.left, rect.top);
    shape.setFillColor(fill);
    if (outlineThickness > 0.f) {
        shape.setOutlineThickness(outlineThickness);
        shape.setOutlineColor(outline);
    }
    window.draw(shape);
}

void drawElevatedRoundedRect(sf::RenderWindow& window, sf::FloatRect rect, float radius,
                              sf::Color fill, float elevation) {
    // Soft shadow: a slightly larger, downward-offset, low-alpha rounded rect.
    sf::Color shadow(20, 24, 40, 55);
    sf::FloatRect shadowRect{rect.left, rect.top + elevation, rect.width, rect.height};
    drawRoundedRect(window, shadowRect, radius, shadow);
    drawRoundedRect(window, rect, radius, fill);
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
Label::Label(sf::Font& font, std::string text, unsigned size, sf::Color color, bool bold)
    : font_(font), text_(std::move(text)), size_(size), color_(color), bold_(bold) {}

void Label::draw(sf::RenderWindow& window) {
    sf::Text t(text_, font_, size_);
    t.setFillColor(color_);
    if (bold_) t.setStyle(sf::Text::Bold);
    t.setPosition(std::round(bounds_.left), std::round(bounds_.top));
    window.draw(t);
}

// ---------------------------------------------------------------------------
// Button — rounded, animated hover, soft shadow
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
    // Ease hoverT_ toward 0 or 1 each frame -> smooth lift/lighten instead of a hard snap.
    float target = hovered_ ? 1.f : 0.f;
    hoverT_ = lerp(hoverT_, target, 0.25f);
    if (std::fabs(hoverT_ - target) < 0.002f) hoverT_ = target;

    sf::Color lightened = bg_;
    lightened.r = static_cast<sf::Uint8>(std::min(255.f, lightened.r + 22.f));
    lightened.g = static_cast<sf::Uint8>(std::min(255.f, lightened.g + 22.f));
    lightened.b = static_cast<sf::Uint8>(std::min(255.f, lightened.b + 22.f));
    sf::Color fill = lerpColor(bg_, lightened, hoverT_);

    float lift = lerp(0.f, 2.f, hoverT_);
    drawElevatedRoundedRect(window, bounds_, theme.radiusSmall, fill, 2.f - lift * 0.4f);

    sf::Text t(label_, font_, 15);
    t.setStyle(sf::Text::Bold);
    t.setFillColor(fg_);
    auto tb = t.getLocalBounds();
    t.setPosition(std::round(bounds_.left + (bounds_.width - tb.width) / 2.f - tb.left),
                  std::round(bounds_.top + (bounds_.height - tb.height) / 2.f - tb.top - lift));
    window.draw(t);
}

// ---------------------------------------------------------------------------
// TextBox — rounded, animated focus ring
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
            focused_ = false;
        } else if (ch >= 32 && ch < 127) {
            char c = static_cast<char>(ch);
            if (digitsOnly_ && !std::isdigit(static_cast<unsigned char>(c))) return;
            if (text_.size() < 64) text_ += c;
        }
    }
}

void TextBox::draw(sf::RenderWindow& window) {
    focusT_ = lerp(focusT_, focused_ ? 1.f : 0.f, 0.3f);

    drawRoundedRect(window, bounds_, theme.radiusSmall, theme.inputBg,
                     lerpColor(theme.border, theme.inputFocused, focusT_),
                     lerp(1.f, 2.f, focusT_));

    std::string display = text_;
    if (maskChars_ && !display.empty()) display = std::string(display.size(), '*');

    bool showPlaceholder = text_.empty() && !focused_;
    sf::Text t(showPlaceholder ? placeholder_ : display, font_, 15);
    t.setFillColor(showPlaceholder ? theme.textDim : theme.textDark);
    t.setPosition(std::round(bounds_.left + 12.f), std::round(bounds_.top + (bounds_.height - 18.f) / 2.f));
    window.draw(t);

    if (focused_) {
        cursorBlink_ += 0.016f;
        if (std::fmod(cursorBlink_, 1.0f) < 0.5f) {
            float textWidth = t.findCharacterPos(display.size()).x - t.getPosition().x;
            sf::RectangleShape cursor({1.5f, 18.f});
            cursor.setPosition(bounds_.left + 12.f + (showPlaceholder ? 0.f : textWidth),
                                bounds_.top + (bounds_.height - 18.f) / 2.f);
            cursor.setFillColor(theme.inputFocused);
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
    drawRoundedRect(window, bounds_, theme.radiusSmall, theme.cardBg, theme.border, 1.f);

    sf::View oldView = window.getView();
    sf::Vector2u winSize = window.getSize();
    sf::View clipView(sf::FloatRect(bounds_.left, bounds_.top, bounds_.width, bounds_.height));
    clipView.setViewport(sf::FloatRect(bounds_.left / winSize.x, bounds_.top / winSize.y,
                                        bounds_.width / winSize.x, bounds_.height / winSize.y));
    window.setView(clipView);

    int visibleRows = static_cast<int>(bounds_.height / rowHeight_) + 1;
    for (int i = 0; i < visibleRows; ++i) {
        int idx = scrollOffset_ + i;
        if (idx >= static_cast<int>(items_.size())) break;
        float y = bounds_.top + i * rowHeight_;
        if (idx == selected_) {
            sf::RectangleShape hi({bounds_.width, static_cast<float>(rowHeight_)});
            hi.setPosition(bounds_.left, y);
            hi.setFillColor(theme.rowSelected);
            window.draw(hi);
        } else if (idx % 2 == 1) {
            sf::RectangleShape stripe({bounds_.width, static_cast<float>(rowHeight_)});
            stripe.setPosition(bounds_.left, y);
            stripe.setFillColor(theme.rowAlt);
            window.draw(stripe);
        }
        sf::Text t(items_[idx], font_, 14);
        t.setFillColor(theme.textDark);
        t.setPosition(bounds_.left + 10.f, y + (rowHeight_ - 16.f) / 2.f);
        window.draw(t);
    }
    window.setView(oldView);
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------
Table::Table(sf::Font& font, sf::Font& headerFont, std::vector<TableColumn> columns, unsigned rowHeight)
    : font_(font), headerFont_(headerFont), columns_(std::move(columns)), rowHeight_(rowHeight) {}

void Table::setRows(std::vector<std::vector<std::string>> rows, std::vector<sf::Color> rowAccents) {
    rows_ = std::move(rows);
    rowAccents_ = std::move(rowAccents);
    if (selected_ >= static_cast<int>(rows_.size())) selected_ = rows_.empty() ? -1 : 0;
    scrollOffset_ = 0;
}

float Table::totalWidth() const {
    float w = 0.f;
    for (auto& c : columns_) w += c.width;
    return w;
}

void Table::handleEvent(const sf::Event& e, sf::RenderWindow& window) {
    constexpr float headerH = 34.f;
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    sf::FloatRect body{bounds_.left, bounds_.top + headerH, bounds_.width, bounds_.height - headerH};

    if (e.type == sf::Event::MouseWheelScrolled && body.contains(mouse)) {
        scrollOffset_ -= static_cast<int>(e.mouseWheelScroll.delta);
        int maxOffset = std::max(0, static_cast<int>(rows_.size()) - static_cast<int>(body.height / rowHeight_));
        scrollOffset_ = std::clamp(scrollOffset_, 0, maxOffset);
    }
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        if (body.contains(mouse)) {
            int row = static_cast<int>((mouse.y - body.top) / rowHeight_) + scrollOffset_;
            if (row >= 0 && row < static_cast<int>(rows_.size())) selected_ = row;
        }
    }
}

void Table::draw(sf::RenderWindow& window) {
    constexpr float headerH = 34.f;
    drawRoundedRect(window, bounds_, theme.radiusSmall, theme.cardBg, theme.border, 1.f);

    // Header row
    {
        sf::RectangleShape hdrBg({bounds_.width, headerH});
        hdrBg.setPosition(bounds_.left, bounds_.top);
        hdrBg.setFillColor(sf::Color(240, 242, 248));
        window.draw(hdrBg);

        float cx = bounds_.left + 10.f;
        for (auto& col : columns_) {
            sf::Text h(col.header, headerFont_, 12);
            h.setStyle(sf::Text::Bold);
            h.setFillColor(theme.textDim);
            float tx = cx;
            if (col.alignRight) {
                auto tb = h.getLocalBounds();
                tx = cx + col.width - tb.width - 12.f;
            }
            h.setPosition(std::round(tx), std::round(bounds_.top + (headerH - 14.f) / 2.f));
            window.draw(h);
            cx += col.width;
        }
        sf::RectangleShape divider({bounds_.width, 1.f});
        divider.setPosition(bounds_.left, bounds_.top + headerH);
        divider.setFillColor(theme.border);
        window.draw(divider);
    }

    // Body (clipped)
    sf::FloatRect body{bounds_.left, bounds_.top + headerH, bounds_.width, bounds_.height - headerH};
    sf::View oldView = window.getView();
    sf::Vector2u winSize = window.getSize();
    sf::View clipView(body);
    clipView.setViewport(sf::FloatRect(body.left / winSize.x, body.top / winSize.y,
                                        body.width / winSize.x, body.height / winSize.y));
    window.setView(clipView);

    int visibleRows = static_cast<int>(body.height / rowHeight_) + 1;
    for (int i = 0; i < visibleRows; ++i) {
        int idx = scrollOffset_ + i;
        if (idx >= static_cast<int>(rows_.size())) break;
        float y = body.top + i * rowHeight_;

        if (idx == selected_) {
            sf::RectangleShape hi({bounds_.width, static_cast<float>(rowHeight_)});
            hi.setPosition(bounds_.left, y);
            hi.setFillColor(theme.rowSelected);
            window.draw(hi);
        } else if (idx % 2 == 1) {
            sf::RectangleShape stripe({bounds_.width, static_cast<float>(rowHeight_)});
            stripe.setPosition(bounds_.left, y);
            stripe.setFillColor(theme.rowAlt);
            window.draw(stripe);
        }

        sf::Color rowColor = theme.textDark;
        if (idx < static_cast<int>(rowAccents_.size()) && rowAccents_[idx] != sf::Color::Transparent) {
            rowColor = rowAccents_[idx];
        }

        float cx = bounds_.left + 10.f;
        auto& rowCells = rows_[idx];
        for (size_t c = 0; c < columns_.size() && c < rowCells.size(); ++c) {
            sf::Text t(rowCells[c], font_, 14);
            t.setFillColor(rowColor);
            float tx = cx;
            if (columns_[c].alignRight) {
                auto tb = t.getLocalBounds();
                tx = cx + columns_[c].width - tb.width - 12.f;
            }
            t.setPosition(std::round(tx), std::round(y + (rowHeight_ - 16.f) / 2.f));
            window.draw(t);
            cx += columns_[c].width;
        }
    }
    window.setView(oldView);
}

// ---------------------------------------------------------------------------
// Static drawing helpers
// ---------------------------------------------------------------------------
void drawPanelBackground(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color border) {
    drawRoundedRect(window, rect, theme.radius, fill, border, 1.f);
}

void drawCard(sf::RenderWindow& window, sf::Font& font, sf::FloatRect rect,
              const std::string& title, const std::string& value, sf::Color valueColor) {
    drawElevatedRoundedRect(window, rect, theme.radius, theme.cardBg, 3.f);

    sf::Text titleText(title, font, 13);
    titleText.setFillColor(theme.textDim);
    titleText.setStyle(sf::Text::Bold);
    titleText.setPosition(rect.left + 16.f, rect.top + 14.f);
    window.draw(titleText);

    sf::Text valueText(value, font, 28);
    valueText.setStyle(sf::Text::Bold);
    valueText.setFillColor(valueColor);
    valueText.setPosition(rect.left + 16.f, rect.top + rect.height / 2.f + 2.f);
    window.draw(valueText);
}

}  // namespace ui