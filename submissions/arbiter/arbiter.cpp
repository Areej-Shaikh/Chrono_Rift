#include <SFML/Graphics.hpp>

int main() {
    sf::RenderWindow window(sf::VideoMode(600, 400), "Chrono Rift - Arbiter");

    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        window.clear(sf::Color::Black);

        sf::CircleShape circle(80);
        circle.setFillColor(sf::Color::Green);
        circle.setPosition(250, 150);

        window.draw(circle);
        window.display();
    }

    return 0;
}