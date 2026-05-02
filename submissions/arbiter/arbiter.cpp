#include <SFML/Graphics.hpp>
#include <iostream>
#include "shared_memory.h"

using namespace std;

int main() {
    SharedState* state = createSharedMemory();

    if (state == NULL) {
        cout << "Arbiter failed to create shared memory." << endl;
        return 1;
    }

    initializeSharedState(state);

    cout << "Arbiter created shared memory." << endl;

    sf::RenderWindow window(sf::VideoMode(600, 400), "Chrono Rift - Arbiter");

    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        window.clear();

        sf::CircleShape circle(80);
        circle.setFillColor(sf::Color::Green);
        circle.setPosition(220, 120);

        window.draw(circle);
        window.display();
    }

    detachSharedMemory(state);
    destroySharedMemory();

    return 0;
}