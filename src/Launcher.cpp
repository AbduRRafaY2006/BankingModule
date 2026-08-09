#include "Launcher.h"

#include "ATM.h"
#include "Bank.h"
#include "UI.h"
#include "Widgets.h"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <vector>

using ui::Button;
using ui::Label;
using ui::Widget;

namespace {

constexpr float WIN_W = 760.f;
constexpr float WIN_H = 690.f;
constexpr float PI = 3.14159265358979f;

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

// ------------------------------------------------------------------------
// Small self-contained drawing helpers (pure SFML, no dependency on
// Widgets.h) used to build the rounded cards, dividers, and hand-drawn
// icons seen in the reference mockup.
// ------------------------------------------------------------------------

sf::ConvexShape makeRoundedRect(float x, float y, float w, float h, float radius, int segs = 8) {
    radius = std::min(radius, std::min(w, h) / 2.f);
    std::vector<sf::Vector2f> pts;
    auto arc = [&](float cx, float cy, float startDeg) {
        for (int i = 0; i <= segs; ++i) {
            float deg = startDeg + 90.f * static_cast<float>(i) / segs;
            float rad = deg * PI / 180.f;
            pts.push_back({cx + radius * std::cos(rad), cy + radius * std::sin(rad)});
        }
    };
    arc(x + radius, y + radius, 180.f);
    arc(x + w - radius, y + radius, 270.f);
    arc(x + w - radius, y + h - radius, 0.f);
    arc(x + radius, y + h - radius, 90.f);

    sf::ConvexShape shape(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) shape.setPoint(i, pts[i]);
    return shape;
}

void drawRoundedRect(sf::RenderWindow& window, float x, float y, float w, float h, float radius,
                      sf::Color fill, sf::Color outline = sf::Color::Transparent, float outlineThickness = 0.f) {
    auto shape = makeRoundedRect(x, y, w, h, radius);
    shape.setFillColor(fill);
    if (outlineThickness > 0.f) {
        shape.setOutlineColor(outline);
        shape.setOutlineThickness(outlineThickness);
    }
    window.draw(shape);
}

sf::RectangleShape thickLine(sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    sf::RectangleShape line({len, thickness});
    line.setFillColor(color);
    line.setOrigin(0.f, thickness / 2.f);
    line.setPosition(a);
    line.setRotation(std::atan2(d.y, d.x) * 180.f / PI);
    return line;
}

void drawChevron(sf::RenderWindow& window, sf::Vector2f tip, float size, sf::Color color, float thickness = 2.2f) {
    window.draw(thickLine({tip.x - size, tip.y - size}, tip, thickness, color));
    window.draw(thickLine({tip.x - size, tip.y + size}, tip, thickness, color));
}

void drawShield(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    sf::ConvexShape shield(6);
    shield.setPoint(0, {c.x - s, c.y - s * 0.7f});
    shield.setPoint(1, {c.x, c.y - s});
    shield.setPoint(2, {c.x + s, c.y - s * 0.7f});
    shield.setPoint(3, {c.x + s, c.y + s * 0.2f});
    shield.setPoint(4, {c.x, c.y + s * 1.1f});
    shield.setPoint(5, {c.x - s, c.y + s * 0.2f});
    shield.setFillColor(sf::Color::Transparent);
    shield.setOutlineColor(color);
    shield.setOutlineThickness(2.f);
    window.draw(shield);

    sf::CircleShape badge(s * 0.28f, 5);
    badge.setOrigin(s * 0.28f, s * 0.28f);
    badge.setPosition(c.x, c.y - s * 0.1f);
    badge.setFillColor(color);
    window.draw(badge);
}

void drawLock(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    sf::RectangleShape body({s * 1.4f, s * 1.0f});
    body.setOrigin(s * 0.7f, s * 0.5f);
    body.setPosition(c.x, c.y + s * 0.25f);
    body.setFillColor(sf::Color::Transparent);
    body.setOutlineColor(color);
    body.setOutlineThickness(2.f);
    window.draw(body);

    sf::CircleShape shackle(s * 0.5f);
    shackle.setOrigin(s * 0.5f, s * 0.5f);
    shackle.setPosition(c.x, c.y - s * 0.25f);
    shackle.setFillColor(sf::Color::Transparent);
    shackle.setOutlineColor(color);
    shackle.setOutlineThickness(2.f);
    window.draw(shackle);
}

void drawClock(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    sf::CircleShape face(s);
    face.setOrigin(s, s);
    face.setPosition(c);
    face.setFillColor(sf::Color::Transparent);
    face.setOutlineColor(color);
    face.setOutlineThickness(2.f);
    window.draw(face);
    window.draw(thickLine(c, {c.x, c.y - s * 0.6f}, 2.f, color));
    window.draw(thickLine(c, {c.x + s * 0.4f, c.y + s * 0.15f}, 2.f, color));
}

void drawCalendar(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    sf::RectangleShape body({s * 1.6f, s * 1.4f});
    body.setOrigin(s * 0.8f, s * 0.7f);
    body.setPosition(c);
    body.setFillColor(sf::Color::Transparent);
    body.setOutlineColor(color);
    body.setOutlineThickness(2.f);
    window.draw(body);
    window.draw(thickLine({c.x - s * 0.8f, c.y - s * 0.3f}, {c.x + s * 0.8f, c.y - s * 0.3f}, 2.f, color));
    window.draw(thickLine({c.x - s * 0.45f, c.y - s * 0.7f}, {c.x - s * 0.45f, c.y - s * 0.95f}, 2.f, color));
    window.draw(thickLine({c.x + s * 0.45f, c.y - s * 0.7f}, {c.x + s * 0.45f, c.y - s * 0.95f}, 2.f, color));
}

void drawPower(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color, sf::Color bgFill) {
    sf::CircleShape ring(s, 24);
    ring.setOrigin(s, s);
    ring.setPosition(c);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(color);
    ring.setOutlineThickness(2.2f);
    window.draw(ring);
    sf::RectangleShape gap({s * 0.9f, s * 0.6f});
    gap.setOrigin(s * 0.45f, s * 0.6f);
    gap.setPosition(c);
    gap.setFillColor(bgFill);
    window.draw(gap);
    window.draw(thickLine({c.x, c.y - s * 1.15f}, {c.x, c.y - s * 0.15f}, 2.4f, color));
}

void drawPersonGear(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    // Head.
    sf::CircleShape head(s * 0.30f);
    head.setOrigin(s * 0.30f, s * 0.30f);
    head.setPosition(c.x - s * 0.12f, c.y - s * 0.32f);
    head.setFillColor(color);
    window.draw(head);

    // Shoulders: a flat-bottomed dome (upper-half ellipse), not a full circle,
    // so it reads as a torso instead of merging into a figure-eight with the head.
    const int segs = 16;
    sf::ConvexShape dome(segs + 1);
    const float rx = s * 0.5f, ry = s * 0.42f;
    const sf::Vector2f cc(c.x - s * 0.12f, c.y + s * 0.30f);
    for (int i = 0; i <= segs; ++i) {
        float t = PI * (1.f - static_cast<float>(i) / segs);
        dome.setPoint(i, {cc.x + rx * std::cos(t), cc.y - ry * std::sin(t)});
    }
    dome.setFillColor(color);
    window.draw(dome);

    // Small gear accent, clearly separated to the side (reads as "admin/settings").
    const sf::Vector2f gearC(c.x + s * 0.52f, c.y - s * 0.05f);
    const float gr = s * 0.20f;
    sf::CircleShape gearHub(gr);
    gearHub.setOrigin(gr, gr);
    gearHub.setPosition(gearC);
    gearHub.setFillColor(sf::Color::Transparent);
    gearHub.setOutlineColor(color);
    gearHub.setOutlineThickness(1.8f);
    window.draw(gearHub);
    for (int i = 0; i < 6; ++i) {
        float angle = i * 60.f * PI / 180.f;
        sf::Vector2f p1(gearC.x + std::cos(angle) * gr, gearC.y + std::sin(angle) * gr);
        sf::Vector2f p2(gearC.x + std::cos(angle) * (gr * 1.5f), gearC.y + std::sin(angle) * (gr * 1.5f));
        window.draw(thickLine(p1, p2, 1.6f, color));
    }
}

void drawCardIcon(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    sf::RectangleShape body({s * 1.7f, s * 1.1f});
    body.setOrigin(s * 0.85f, s * 0.55f);
    body.setPosition(c);
    body.setFillColor(sf::Color::Transparent);
    body.setOutlineColor(color);
    body.setOutlineThickness(2.2f);
    window.draw(body);
    sf::RectangleShape slot({s * 1.7f, s * 0.22f});
    slot.setOrigin(s * 0.85f, s * 0.11f);
    slot.setPosition(c.x, c.y - s * 0.3f);
    slot.setFillColor(color);
    window.draw(slot);
    sf::RectangleShape chip({s * 0.4f, s * 0.3f});
    chip.setOrigin(s * 0.2f, s * 0.15f);
    chip.setPosition(c.x - s * 0.5f, c.y + s * 0.25f);
    chip.setFillColor(color);
    window.draw(chip);
}

void drawBankBuilding(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color color) {
    // Roof (pediment).
    sf::ConvexShape roof(3);
    roof.setPoint(0, {c.x - s * 1.05f, c.y - s * 0.1f});
    roof.setPoint(1, {c.x, c.y - s});
    roof.setPoint(2, {c.x + s * 1.05f, c.y - s * 0.1f});
    roof.setFillColor(color);
    window.draw(roof);

    // Ledge directly under the roof.
    sf::RectangleShape ledge({s * 2.2f, s * 0.18f});
    ledge.setOrigin(s * 1.1f, 0.f);
    ledge.setPosition(c.x, c.y - s * 0.1f);
    ledge.setFillColor(color);
    window.draw(ledge);

    // Columns — each with a flared capital and base, so they read as classic
    // columns instead of plain bars.
    const int numCols = 4;
    const float colTop = c.y + s * 0.08f;
    const float colHeight = s * 0.75f;
    const float colW = s * 0.14f;
    const float capW = s * 0.26f;
    const float capH = s * 0.07f;
    const float leftX = c.x - s * 0.78f;
    const float rightX = c.x + s * 0.78f;
    for (int i = 0; i < numCols; ++i) {
        float cxCol = leftX + (rightX - leftX) * static_cast<float>(i) / (numCols - 1);

        sf::RectangleShape shaft({colW, colHeight});
        shaft.setOrigin(colW / 2.f, 0.f);
        shaft.setPosition(cxCol, colTop);
        shaft.setFillColor(color);
        window.draw(shaft);

        sf::RectangleShape cap({capW, capH});
        cap.setOrigin(capW / 2.f, 0.f);
        cap.setPosition(cxCol, colTop);
        cap.setFillColor(color);
        window.draw(cap);

        sf::RectangleShape baseCap({capW, capH});
        baseCap.setOrigin(capW / 2.f, capH);
        baseCap.setPosition(cxCol, colTop + colHeight);
        baseCap.setFillColor(color);
        window.draw(baseCap);
    }

    // Base plinth.
    sf::RectangleShape plinth({s * 2.3f, s * 0.16f});
    plinth.setOrigin(s * 1.15f, 0.f);
    plinth.setPosition(c.x, colTop + colHeight);
    plinth.setFillColor(color);
    window.draw(plinth);
}

} // namespace

void RunLauncherUI() {
    Bank bank("data");

    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                            "Banking Module", sf::Style::Titlebar | sf::Style::Close, settings);
    window.setFramerateLimit(60);

    sf::Font regular = loadFontOrThrow("DejaVuSans.ttf");
    sf::Font bold = loadFontOrThrow("DejaVuSans-Bold.ttf");

    const sf::Color navy(22, 48, 92), navyDark(12, 31, 61), yellow(244, 196, 48);
    const sf::Color mediumBlue(56, 97, 168);
    const sf::Color lightBlueGray(157, 180, 217);
    const sf::Color white(240, 244, 250);
    const sf::Color skyBlue(90, 170, 240);
    const sf::Color cardFill(10, 22, 45, 210);
    const sf::Color footerFill(9, 19, 38, 200);
    const sf::Color dividerColor(255, 255, 255, 40);

    // Background image — unchanged "cover" scaling + dark scrim from the original.
    sf::Texture bgTexture;
    bool bgLoaded = false;
    for (const std::string& base : {"assets/", "../assets/", "./"}) {
        if (bgTexture.loadFromFile(base + "launcher_bg.jpg")) { bgLoaded = true; break; }
    }
    sf::Sprite bgSprite;
    if (bgLoaded) {
        bgSprite.setTexture(bgTexture);
        sf::Vector2u texSize = bgTexture.getSize();
        float scale = std::max(WIN_W / static_cast<float>(texSize.x), WIN_H / static_cast<float>(texSize.y));
        bgSprite.setScale(scale, scale);
        float spriteW = static_cast<float>(texSize.x) * scale, spriteH = static_cast<float>(texSize.y) * scale;
        bgSprite.setPosition((WIN_W - spriteW) / 2.f, (WIN_H - spriteH) / 2.f);
    }

    std::vector<std::unique_ptr<Widget>> owner;
    std::vector<Widget*> tab;

    auto centeredXIn = [&](const std::string& text, sf::Font& font, unsigned size, bool boldStyle,
                            float regionX, float regionW) {
        sf::Text t(text, font, size);
        if (boldStyle) t.setStyle(sf::Text::Bold);
        auto tb = t.getLocalBounds();
        return regionX + (regionW - tb.width) / 2.f - tb.left;
    };
    auto centeredX = [&](const std::string& text, sf::Font& font, unsigned size, bool boldStyle) {
        return centeredXIn(text, font, size, boldStyle, 0.f, WIN_W);
    };
    auto textWidth = [&](const std::string& text, sf::Font& font, unsigned size, bool boldStyle) {
        sf::Text t(text, font, size);
        if (boldStyle) t.setStyle(sf::Text::Bold);
        return t.getLocalBounds().width;
    };

    // ---- Header: bank icon + three-tone title + subtitle ----
    float w1 = textWidth("BANK ", bold, 30, true);
    float w2 = textWidth("ATM ", bold, 30, true);
    float w3 = textWidth("SYSTEM", bold, 30, true);
    float titleStartX = (WIN_W - (w1 + w2 + w3)) / 2.f;

    auto titleBank = add<Label>(owner, tab, bold, "BANK ", 30, white, true);
    titleBank->setPosition(titleStartX, 106.f);
    auto titleAtm = add<Label>(owner, tab, bold, "ATM ", 30, yellow, true);
    titleAtm->setPosition(titleStartX + w1, 106.f);
    auto titleSys = add<Label>(owner, tab, bold, "SYSTEM", 30, white, true);
    titleSys->setPosition(titleStartX + w1 + w2, 106.f);

    auto subtitle = add<Label>(owner, tab, regular, "Select an option to continue", 14, lightBlueGray);
    subtitle->setPosition(centeredX("Select an option to continue", regular, 14, false), 156.f);

    // ---- Cards ----
    struct CardSpec {
        float x, w;
        sf::Color accent;
        std::string title, line1, line2, cta;
        int iconId; // 0 = admin (person+gear), 1 = atm (card)
    };
    const float cardY = 228.f, cardH = 260.f;
    CardSpec adminSpec{50.f, 310.f, yellow, "ADMIN PANEL",
                       "Manage accounts, transactions", "and reports with ease.", "ACCESS ADMIN PANEL", 0};
    CardSpec atmSpec{400.f, 310.f, skyBlue, "ATM",
                     "Customer self-service", "terminal for quick banking.", "GO TO ATM", 1};

    // Buttons are made fully transparent — they exist purely to keep the
    // original click-handling/hit-testing behavior. All visuals for the
    // cards are drawn manually below so they can match the mockup's rounded,
    // bordered card look. NOTE: this assumes Button tolerates a blank label
    // and honors alpha 0 for its fill color; if Widgets.h enforces something
    // different, share it and this can be adjusted.
    auto adminBtn = add<Button>(owner, tab, regular, " ", [&bank] { RunAdminUI(bank); },
                                 sf::Color(0, 0, 0, 0), sf::Color(0, 0, 0, 0));
    adminBtn->setPosition(adminSpec.x, cardY);
    adminBtn->setSize(adminSpec.w, cardH);

    auto atmBtn = add<Button>(owner, tab, regular, " ", [&bank] { RunATMUI(bank); },
                               sf::Color(0, 0, 0, 0), sf::Color(0, 0, 0, 0));
    atmBtn->setPosition(atmSpec.x, cardY);
    atmBtn->setSize(atmSpec.w, cardH);

    // CTA pill layout constants — text and chevron form one cluster that gets
    // centered as a unit, with fixed padding, instead of a fixed pixel nudge.
    const float ctaPadX = 18.f, ctaGap = 12.f, ctaChevronW = 8.f, ctaH = 28.f;

    for (const CardSpec& cs : {adminSpec, atmSpec}) {
        float cx = cs.x + cs.w / 2.f;
        auto titleLbl = add<Label>(owner, tab, bold, cs.title, 20, cs.accent, true);
        titleLbl->setPosition(centeredXIn(cs.title, bold, 20, true, cs.x, cs.w), cardY + 112.f);
        auto l1 = add<Label>(owner, tab, regular, cs.line1, 12, lightBlueGray);
        l1->setPosition(centeredXIn(cs.line1, regular, 12, false, cs.x, cs.w), cardY + 146.f);
        auto l2 = add<Label>(owner, tab, regular, cs.line2, 12, lightBlueGray);
        l2->setPosition(centeredXIn(cs.line2, regular, 12, false, cs.x, cs.w), cardY + 164.f);

        float ctaTextW = textWidth(cs.cta, bold, 12, true);
        float ctaW = ctaTextW + ctaGap + ctaChevronW + ctaPadX * 2.f;
        float ctaX = cx - ctaW / 2.f;
        auto ctaLbl = add<Label>(owner, tab, bold, cs.cta, 12, cs.accent, true);
        ctaLbl->setPosition(ctaX + ctaPadX, cardY + 218.f);
    }

    // ---- Exit pill ----
    // Sized FROM its content: icon + gap + text + gap + chevron, plus fixed
    // side padding — mirrors the CTA pill approach above. The previous
    // version fixed the pill width at 200px and centered the cluster inside
    // it; with the real font metrics that cluster was ~198px wide, leaving
    // almost no padding, so the icon sat right on the rounded edge.
    const float exitH = 46.f;
    const float exitIconR = 8.f, exitGap1 = 10.f, exitGap2 = 14.f, exitChevronW = 8.f, exitPadX = 26.f;
    const sf::Color exitFill(196, 55, 60);     // solid, opaque — this is what makes it pop
    const sf::Color exitFillHover(168, 42, 47); // slightly darker edge for definition
    const sf::Color exitIconColor(255, 255, 255);
    const std::string exitText = "EXIT APPLICATION";

    float exitTextW = textWidth(exitText, bold, 13, true);
    float exitClusterW = exitIconR * 2.f + exitGap1 + exitTextW + exitGap2 + exitChevronW;
    const float exitW = exitClusterW + exitPadX * 2.f;
    const float exitX = (WIN_W - exitW) / 2.f;
    const float exitY = cardY + cardH + 32.f;

    auto exitBtn = add<Button>(owner, tab, regular, " ", [&window] { window.close(); },
                                sf::Color(0, 0, 0, 0), sf::Color(0, 0, 0, 0));
    exitBtn->setPosition(exitX, exitY);
    exitBtn->setSize(exitW, exitH);

    float exitClusterX = exitX + exitPadX;
    float exitIconCx = exitClusterX + exitIconR;
    float exitTextX = exitClusterX + exitIconR * 2.f + exitGap1;
    float exitChevronTipX = exitTextX + exitTextW + exitGap2 + exitChevronW;

    auto exitLbl = add<Label>(owner, tab, bold, exitText, 13, exitIconColor, true);
    exitLbl->setPosition(exitTextX, exitY + (exitH - 18.f) / 2.f);

    // ---- Footer stat bar ----
    const float footerX = 30.f, footerY = exitY + exitH + 24.f, footerW = WIN_W - 60.f, footerH = 70.f;
    const float colW = footerW / 4.f;
    struct FooterSpec { std::string title, sub; };
    FooterSpec footerItems[4] = {
        {"SECURE", "Your security is our priority"},
        {"RELIABLE", "Always here for you"},
        {"24/7 SERVICE", "Bank anytime, anywhere"},
        {"", ""} // 4th column is the live date/time, drawn fresh every frame below
    };
    for (int i = 0; i < 4; ++i) {
        if (footerItems[i].title.empty()) continue;
        float colX = footerX + i * colW;
        auto t = add<Label>(owner, tab, bold, footerItems[i].title, 12, white, true);
        t->setPosition(colX + 46.f, footerY + 18.f);
        auto s = add<Label>(owner, tab, regular, footerItems[i].sub, 10, lightBlueGray);
        s->setPosition(colX + 46.f, footerY + 36.f);
    }

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();
            for (auto* w : tab) w->handleEvent(event, window);
        }

        window.clear(navy);

        if (bgLoaded) {
            window.draw(bgSprite);
            sf::RectangleShape overlay({WIN_W, WIN_H});
            overlay.setFillColor(sf::Color(navy.r, navy.g, navy.b, 150));
            window.draw(overlay);
        }

        // Bank icon above the title.
        drawBankBuilding(window, {WIN_W / 2.f, 62.f}, 26.f, yellow);

        // Divider + shield under the subtitle.
        {
            float dy = 199.f, half = 90.f;
            window.draw(thickLine({WIN_W / 2.f - half - 26.f, dy}, {WIN_W / 2.f - 14.f, dy}, 1.f, dividerColor));
            window.draw(thickLine({WIN_W / 2.f + 14.f, dy}, {WIN_W / 2.f + half + 26.f, dy}, 1.f, dividerColor));
            drawShield(window, {WIN_W / 2.f, dy}, 9.f, yellow);
        }

        // Cards: rounded, bordered background + icon badge + CTA pill.
        for (const CardSpec& cs : {adminSpec, atmSpec}) {
            drawRoundedRect(window, cs.x, cardY, cs.w, cardH, 16.f, cardFill, cs.accent, 1.6f);
            window.draw(thickLine({cs.x + 24.f, cardY + 190.f}, {cs.x + cs.w - 24.f, cardY + 190.f}, 1.f, dividerColor));

            float cx = cs.x + cs.w / 2.f;
            sf::CircleShape badge(30.f);
            badge.setOrigin(30.f, 30.f);
            badge.setPosition(cx, cardY + 50.f);
            badge.setFillColor(sf::Color(cs.accent.r, cs.accent.g, cs.accent.b, 30));
            badge.setOutlineColor(cs.accent);
            badge.setOutlineThickness(1.6f);
            window.draw(badge);
            if (cs.iconId == 0) drawPersonGear(window, {cx, cardY + 50.f}, 22.f, cs.accent);
            else drawCardIcon(window, {cx, cardY + 50.f}, 20.f, cs.accent);

            // Same measured cluster used for the label above: pill width comes
            // from actual text width + fixed gap + chevron, not a guessed pad.
            float ctaTextW = textWidth(cs.cta, bold, 12, true);
            float ctaW = ctaTextW + ctaGap + ctaChevronW + ctaPadX * 2.f;
            float ctaX = cx - ctaW / 2.f, ctaY = cardY + 212.f;
            drawRoundedRect(window, ctaX, ctaY, ctaW, ctaH, ctaH / 2.f,
                             sf::Color(cs.accent.r, cs.accent.g, cs.accent.b, 24), cs.accent, 1.4f);
            float ctaChevronTipX = ctaX + ctaPadX + ctaTextW + ctaGap + ctaChevronW;
            drawChevron(window, {ctaChevronTipX, ctaY + ctaH / 2.f}, 4.f, cs.accent);
        }

        // Exit pill — icon, text, and chevron use the single measured cluster
        // computed during setup, so they can't collide regardless of text width.
        drawRoundedRect(window, exitX, exitY, exitW, exitH, exitH / 2.f, exitFill, exitFillHover, 2.f);
        drawPower(window, {exitIconCx, exitY + exitH / 2.f}, exitIconR, exitIconColor, exitFill);
        drawChevron(window, {exitChevronTipX, exitY + exitH / 2.f}, 4.f, exitIconColor);

        // Footer bar.
        drawRoundedRect(window, footerX, footerY, footerW, footerH, 14.f, footerFill,
                         sf::Color(255, 255, 255, 25), 1.f);
        for (int i = 1; i < 4; ++i) {
            float dx = footerX + i * colW;
            window.draw(thickLine({dx, footerY + 12.f}, {dx, footerY + footerH - 12.f}, 1.f, dividerColor));
        }
        drawShield(window, {footerX + 22.f, footerY + footerH / 2.f}, 10.f, yellow);
        drawLock(window, {footerX + colW + 22.f, footerY + footerH / 2.f}, 10.f, yellow);
        drawClock(window, {footerX + 2 * colW + 22.f, footerY + footerH / 2.f}, 10.f, yellow);
        drawCalendar(window, {footerX + 3 * colW + 22.f, footerY + footerH / 2.f}, 10.f, yellow);

        // Live date/time in the 4th footer column.
        {
            std::time_t now = std::time(nullptr);
            std::tm local = *std::localtime(&now);
            char dateBuf[32], timeBuf[32];
            std::strftime(dateBuf, sizeof(dateBuf), "%d %b %Y", &local);
            std::strftime(timeBuf, sizeof(timeBuf), "%I:%M %p", &local);

            float colX = footerX + 3 * colW;
            sf::Text dateText(dateBuf, bold, 12);
            dateText.setStyle(sf::Text::Bold);
            dateText.setFillColor(white);
            dateText.setPosition(colX + 46.f, footerY + 18.f);
            window.draw(dateText);

            sf::Text timeText(timeBuf, regular, 11);
            timeText.setFillColor(yellow);
            timeText.setPosition(colX + 46.f, footerY + 36.f);
            window.draw(timeText);
        }

        for (auto* w : tab) w->draw(window);

        window.display();
    }
}