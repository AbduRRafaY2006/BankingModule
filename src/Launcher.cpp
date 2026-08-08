#include "Launcher.h"

#include "ATM.h"
#include "Bank.h"
#include "UI.h"
#include "Widgets.h"

#include <SFML/Graphics.hpp>
#include <memory>
#include <stdexcept>
#include <vector>

using ui::Button;
using ui::Label;
using ui::Widget;

namespace {

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

} // namespace

void RunLauncherUI() {
    Bank bank("data");

    sf::RenderWindow window(sf::VideoMode(static_cast<unsigned>(WIN_W), static_cast<unsigned>(WIN_H)),
                            "Banking Module", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Font regular = loadFontOrThrow("DejaVuSans.ttf");
    sf::Font bold = loadFontOrThrow("DejaVuSans-Bold.ttf");

    std::vector<std::unique_ptr<Widget>> owner;
    std::vector<Widget*> tab;

    auto title = add<Label>(owner, tab, regular, "Banking Module", 24, ui::theme.textDark, true);
    title->setPosition(60.f, 70.f);

    auto subtitle = add<Label>(owner, tab, regular, "Choose where you want to go", 15, ui::theme.textDim);
    subtitle->setPosition(60.f, 112.f);

    auto adminBtn = add<Button>(owner, tab, regular, "Admin Panel",
        [&bank] { RunAdminUI(bank); }, ui::theme.accent, ui::theme.textLight);
    adminBtn->setPosition(60.f, 170.f);
    adminBtn->setSize(260.f, 48.f);

    auto atmBtn = add<Button>(owner, tab, regular, "ATM",
        [&bank] { RunATMUI(bank); }, ui::theme.success, ui::theme.textLight);
    atmBtn->setPosition(60.f, 232.f);
    atmBtn->setSize(260.f, 48.f);

    auto exitBtn = add<Button>(owner, tab, regular, "Exit",
        [&window] { window.close(); }, ui::theme.textDim, ui::theme.textLight);
    exitBtn->setPosition(60.f, 294.f);
    exitBtn->setSize(260.f, 44.f);

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();
            for (auto* w : tab) w->handleEvent(event, window);
        }

        window.clear(ui::theme.contentBg);

        sf::Text heading("Banking Module", bold, 28);
        heading.setFillColor(ui::theme.textDark);
        heading.setPosition(60.f, 28.f);
        window.draw(heading);

        sf::RectangleShape card({360.f, 330.f});
        card.setPosition(40.f, 22.f);
        card.setFillColor(sf::Color::White);
        card.setOutlineThickness(1.f);
        card.setOutlineColor(ui::theme.border);
        window.draw(card);

        for (auto* w : tab) w->draw(window);

        window.display();
    }
}