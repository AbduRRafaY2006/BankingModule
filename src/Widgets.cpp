#include "Widgets.h"
#include <cctype>
#include <algorithm>
#include <cmath>
#include <memory>

namespace ui {

Theme theme{};

// ---------------------------------------------------------------------------
// Sound effects
// ---------------------------------------------------------------------------
namespace {
sf::SoundBuffer& loadBufferOnce(const std::string& filename) {
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
        // Silent failure: sound is a nice-to-have, never worth crashing the
        // app over a missing/misplaced wav file.
        static sf::SoundBuffer empty;
        return empty;
    }
    names.push_back(filename);
    cache.push_back(std::move(buf));
    return *cache.back();
}

// Round-robin pool of sf::Sound so a rapid second play doesn't cut the
// first one off (each sf::Sound can only play one instance at a time).
template <size_t N>
struct SoundPool {
    sf::Sound sounds[N];
    size_t next = 0;
    void play(sf::SoundBuffer& buf) {
        sounds[next].setBuffer(buf);
        sounds[next].play();
        next = (next + 1) % N;
    }
};
}  // namespace

void playCashSound() {
    static SoundPool<2> pool;
    pool.play(loadBufferOnce("cash_dispense.wav"));
}

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
// CashDispenser
// ---------------------------------------------------------------------------
namespace {
float easeOutBack(float x) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.f;
    x = std::clamp(x, 0.f, 1.f);
    float xm1 = x - 1.f;
    return 1.f + c3 * xm1 * xm1 * xm1 + c1 * xm1 * xm1;
}
float easeInQuad(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x;
}

// Filled ribbon along a quadratic bezier — used for the smile, since SFML
// has no built-in stroked-curve primitive.
void drawQuadraticThickCurve(sf::RenderWindow& window, sf::Vector2f p0, sf::Vector2f c,
                              sf::Vector2f p1, float thickness, sf::Color color, int segments = 14) {
    sf::VertexArray strip(sf::TriangleStrip, static_cast<size_t>(segments + 1) * 2);
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float mt = 1.f - t;
        sf::Vector2f pt = p0 * (mt * mt) + c * (2.f * mt * t) + p1 * (t * t);
        sf::Vector2f d = (c - p0) * (2.f * mt) + (p1 - c) * (2.f * t);
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        sf::Vector2f nrm = len > 0.0001f ? sf::Vector2f(-d.y / len, d.x / len) : sf::Vector2f(0.f, 1.f);
        strip[static_cast<size_t>(i) * 2].position = pt + nrm * (thickness / 2.f);
        strip[static_cast<size_t>(i) * 2].color = color;
        strip[static_cast<size_t>(i) * 2 + 1].position = pt - nrm * (thickness / 2.f);
        strip[static_cast<size_t>(i) * 2 + 1].color = color;
    }
    window.draw(strip);
}
}  // namespace

CashDispenser::CashDispenser(sf::Font& font) : font_(font) {}

void CashDispenser::trigger() {
    state_ = State::Processing;
    t_ = 0.f;
}

void CashDispenser::draw(sf::RenderWindow& window) {
    constexpr float kDt = 1.f / 60.f;  // matches window.setFramerateLimit(60)
    constexpr float kProcessingDur = 0.5f;
    constexpr float kDispenseDur = 0.45f;
    constexpr float kRestDur = 3.0f;
    constexpr float kRetractDur = 0.35f;

    idleT_ += kDt;  // blink clock runs regardless of state

    if (state_ != State::Idle) {
        t_ += kDt;
        switch (state_) {
            case State::Processing:
                if (t_ >= kProcessingDur) { state_ = State::Dispensing; t_ = 0.f; playCashSound(); }
                break;
            case State::Dispensing:
                if (t_ >= kDispenseDur) { state_ = State::Resting; t_ = 0.f; }
                break;
            case State::Resting:
                if (t_ >= kRestDur) { state_ = State::Retracting; t_ = 0.f; }
                break;
            case State::Retracting:
                if (t_ >= kRetractDur) { state_ = State::Idle; t_ = 0.f; }
                break;
            default:
                break;
        }
    }

    // How far the bill has dropped out of the slot, 0 (hidden) .. 1 (fully out).
    float drop = 0.f;
    if (state_ == State::Dispensing) drop = easeOutBack(t_ / kDispenseDur);
    else if (state_ == State::Resting) drop = 1.f;
    else if (state_ == State::Retracting) drop = 1.f - easeInQuad(t_ / kRetractDur);

    // --- Layout: design coordinates in a fixed 300x260 reference frame,
    // scaled/centered to whatever size this widget was actually given. ---
    constexpr float designW = 300.f, designH = 260.f;
    float s = std::min(bounds_.width / designW, bounds_.height / designH);
    float offX = bounds_.left + (bounds_.width - designW * s) / 2.f;
    float offY = bounds_.top + (bounds_.height - designH * s) / 2.f;
    auto P = [&](float x, float y) { return sf::Vector2f(offX + x * s, offY + y * s); };
    auto SZ = [&](float w, float h) { return sf::Vector2f(w * s, h * s); };
    auto RECT = [&](float x, float y, float w, float h) {
        sf::Vector2f p = P(x, y);
        return sf::FloatRect(p.x, p.y, w * s, h * s);
    };

    const sf::Color rose(209, 126, 166);
    const sf::Color babyPink(251, 213, 229);
    const sf::Color screenPlum(58, 26, 46);
    const sf::Color keyPink(245, 184, 210);
    const sf::Color slotColor(122, 63, 90);
    const sf::Color billGreen(63, 168, 96);
    const sf::Color billGreenDark(31, 122, 66);

    // Header ribbon
    drawRoundedRect(window, RECT(70.f, 6.f, 160.f, 24.f), 8.f * s, rose);
    {
        sf::Text label("A T M", font_, static_cast<unsigned>(std::max(10.f, 13.f * s)));
        label.setStyle(sf::Text::Bold);
        label.setFillColor(sf::Color::White);
        auto tb = label.getLocalBounds();
        sf::Vector2f c = P(150.f, 18.f);
        label.setPosition(std::round(c.x - tb.width / 2.f - tb.left), std::round(c.y - tb.height / 2.f - tb.top));
        window.draw(label);
    }

    // Body
    drawRoundedRect(window, RECT(75.f, 24.f, 150.f, 140.f), 18.f * s, babyPink, rose, 3.f * s);

    // Screen
    drawRoundedRect(window, RECT(85.f, 40.f, 90.f, 44.f), 8.f * s, screenPlum);

    // Eyes (blink is a periodic vertical squish, independent of trigger state)
    float cycle = std::fmod(idleT_, 4.f);
    float blinkScale = 1.f;
    if (cycle > 3.68f) {
        float local = (cycle - 3.68f) / 0.32f;
        float tri = 1.f - std::fabs(local - 0.5f) * 2.f;
        blinkScale = 1.f - tri * 0.9f;
    }
    for (float ex : {100.f, 140.f}) {
        sf::CircleShape eye(4.f * s);
        eye.setOrigin(4.f * s, 4.f * s);
        eye.setScale(1.f, blinkScale);
        eye.setPosition(P(ex, 58.f));
        eye.setFillColor(sf::Color::White);
        window.draw(eye);
    }

    // Smile
    drawQuadraticThickCurve(window, P(98.f, 70.f), P(120.f, 82.f), P(142.f, 70.f), 4.f * s, sf::Color::White);

    // Keypad
    drawRoundedRect(window, RECT(90.f, 92.f, 70.f, 40.f), 6.f * s, keyPink, rose, 2.f * s);
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            sf::RectangleShape key(SZ(18.f, 10.f));
            key.setPosition(P(96.f + col * 25.f, 100.f + row * 16.f));
            key.setFillColor(sf::Color::White);
            window.draw(key);
        }
    }

    // Slot
    drawRoundedRect(window, RECT(100.f, 150.f, 50.f, 10.f), 3.f * s, slotColor);

    // --- Bill: clipped to the region below the slot so it appears to hang
    // out from underneath it, sliding down as `drop` goes 0 -> 1. ---
    {
        sf::View oldView = window.getView();
        sf::Vector2u winSize = window.getSize();
        sf::FloatRect clipRect(bounds_.left, offY + 158.f * s, bounds_.width, offY + designH * s - (offY + 158.f * s));
        sf::View clipView(clipRect);
        clipView.setViewport(sf::FloatRect(clipRect.left / winSize.x, clipRect.top / winSize.y,
                                            clipRect.width / winSize.x, clipRect.height / winSize.y));
        window.setView(clipView);

        float billTopHidden = 80.f;   // fully tucked position (behind the slot, with margin for the rotation overhang)
        float billTopShown = 160.f;   // fully hanging-out position
        float billY = billTopHidden + (billTopShown - billTopHidden) * drop;

        sf::RectangleShape bill(SZ(50.f, 72.f));
        bill.setOrigin(25.f * s, 36.f * s);
        bill.setPosition(P(150.f, billY + 36.f));
        bill.setRotation(3.f);
        bill.setFillColor(billGreen);
        bill.setOutlineThickness(2.f * s);
        bill.setOutlineColor(billGreenDark);
        window.draw(bill);

        sf::CircleShape badge(12.f * s);
        badge.setOrigin(12.f * s, 12.f * s);
        badge.setPosition(P(150.f, billY + 36.f));
        badge.setRotation(3.f);
        badge.setFillColor(sf::Color(234, 250, 240));
        window.draw(badge);

        sf::Text dollar("$", font_, static_cast<unsigned>(std::max(9.f, 15.f * s)));
        dollar.setStyle(sf::Text::Bold);
        dollar.setFillColor(billGreenDark);
        auto tb = dollar.getLocalBounds();
        sf::Vector2f c = P(150.f, billY + 36.f);
        dollar.setPosition(std::round(c.x - tb.width / 2.f - tb.left), std::round(c.y - tb.height / 2.f - tb.top));
        window.draw(dollar);

        window.setView(oldView);
    }
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