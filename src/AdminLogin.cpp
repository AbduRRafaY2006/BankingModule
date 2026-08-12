#include "AdminLogin.h"

#include "AdminAuth.h"
#include "UI.h"
#include "Widgets.h"

#include <SFML/Graphics.hpp>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

using ui::Button;
using ui::Label;
using ui::TextBox;
using ui::Widget;

namespace {

// Same window family as the rest of the app (Launcher is 760x690, the
// Admin Dashboard is 1200x740) -- this one is a focused, single-purpose
// gate screen, so it stays compact.
constexpr float WIN_W = 480.f;
constexpr float WIN_H = 600.f;

// Mirrors UI.cpp's PAGE_* / FIELD_* constants so spacing reads the same
// as the dashboard the user lands on right after this screen.
constexpr float FIELD_GAP = 58.f;

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

}  // namespace

void RunAdminLoginUI(Bank& bank) {
    const std::string dataDir = "data";
    AdminAuth::ensureCredentialsFile(dataDir);

    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                             "Admin Login", sf::Style::Titlebar | sf::Style::Close, settings);
    window.setFramerateLimit(60);

    // Same DejaVu fonts used by the Launcher and (in practice) the Admin
    // Dashboard, so glyphs render identically across every screen.
    sf::Font regular = loadFontOrThrow("DejaVuSans.ttf");
    sf::Font bold = loadFontOrThrow("DejaVuSans-Bold.ttf");

    std::vector<std::unique_ptr<Widget>> owner;
    std::vector<Widget*> widgets;

    // ------------------------------------------------------------------
    // Header band -- exact same navy + gold "ADMIN" plaque + subtitle as
    // the Admin Dashboard's sidebar header, so this reads as the front
    // door of the same screen rather than a different app.
    // ------------------------------------------------------------------
    constexpr float HEADER_H = 130.f;
    constexpr float PLAQUE_W = 200.f, PLAQUE_H = 46.f;
    const sf::FloatRect plaque((WIN_W - PLAQUE_W) / 2.f, 28.f, PLAQUE_W, PLAQUE_H);

    // ------------------------------------------------------------------
    // Login card
    // ------------------------------------------------------------------
    const sf::FloatRect card(40.f, HEADER_H + 34.f, WIN_W - 80.f, 400.f);
    const float fieldX = card.left + 24.f;
    const float fieldW = card.width - 48.f;
    const float fieldY0 = card.top + 62.f;

    auto userBox = add<TextBox>(owner, widgets, regular, "Username");
    userBox->setPosition(fieldX, fieldY0);
    userBox->setSize(fieldW, 42.f);

    auto passBox = add<TextBox>(owner, widgets, regular, "Password", false, true);
    passBox->setPosition(fieldX, fieldY0 + FIELD_GAP);
    passBox->setSize(fieldW, 42.f);

    auto errorLbl = add<Label>(owner, widgets, regular, "", 13, ui::theme.danger);
    errorLbl->setPosition(fieldX, fieldY0 + FIELD_GAP * 2 + 6.f);

    bool loggedIn = false;

    auto attemptLogin = [&] {
        if (userBox->value().empty() || passBox->value().empty()) {
            errorLbl->setText("Enter both username and password.");
            return;
        }
        if (AdminAuth::verify(dataDir, userBox->value(), passBox->value())) {
            loggedIn = true;
            window.close();
        } else {
            errorLbl->setText("Invalid username or password.");
            passBox->clear();
        }
    };

    auto loginBtn = add<Button>(owner, widgets, bold, "LOGIN", attemptLogin,
                                 ui::theme.accent, ui::theme.textDark);
    loginBtn->setPosition(fieldX, fieldY0 + FIELD_GAP * 2 + 32.f);
    loginBtn->setSize(fieldW, 46.f);

    auto cancelBtn = add<Button>(owner, widgets, regular, "Cancel", [&] { window.close(); },
                                  ui::theme.rowAlt, ui::theme.textDim);
    cancelBtn->setPosition(fieldX, fieldY0 + FIELD_GAP * 2 + 32.f + 58.f);
    cancelBtn->setSize(fieldW, 40.f);

    auto hintLbl = add<Label>(owner, widgets, regular,
                               "First run defaults to admin / admin123 -- change them",
                               11, ui::theme.textDim);
    hintLbl->setPosition(fieldX, card.top + card.height - 34.f);
    auto hintLbl2 = add<Label>(owner, widgets, regular,
                                "in data/admin_credentials.dat.",
                                11, ui::theme.textDim);
    hintLbl2->setPosition(fieldX, card.top + card.height - 18.f);

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) window.close();
                if (event.key.code == sf::Keyboard::Enter) attemptLogin();
            }
            for (auto* w : widgets) w->handleEvent(event, window);
        }

        window.clear(ui::theme.contentBg);

        // ---- Header ----
        {
            sf::RectangleShape header({WIN_W, HEADER_H});
            header.setFillColor(ui::theme.sidebarBg);
            window.draw(header);

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
            subtitle.setPosition(std::round((WIN_W - sb.width) / 2.f), plaque.top + PLAQUE_H + 8.f);
            window.draw(subtitle);
        }

        // ---- Card ----
        ui::drawSectionCard(window, bold, card, "ADMINISTRATOR LOGIN", ui::theme.accent);

        for (auto* w : widgets) w->draw(window);

        window.display();
    }

    if (loggedIn) {
        RunAdminUI(bank);
    }
}
