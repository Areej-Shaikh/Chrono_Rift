#include "player_threads.h"
#include "shared_memory.h"
#include <SFML/Graphics.hpp>
#include "player_actions.h"
#include "input_buffer.h"
#include <iostream>
#include <unistd.h>

using namespace std;
int selectPartySize(sf::RenderWindow& window, sf::Font& font) {
    int selected = 0;

    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                return 1;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Up) {
                    selected--;

                    if (selected < 0) {
                        selected = 3;
                    }
                }

                else if (event.key.code == sf::Keyboard::Down) {
                    selected++;

                    if (selected > 3) {
                        selected = 0;
                    }
                }

                else if (event.key.code == sf::Keyboard::Enter) {
                    return selected + 1;
                }

                else if (event.key.code == sf::Keyboard::Num1) {
                    return 1;
                }

                else if (event.key.code == sf::Keyboard::Num2) {
                    return 2;
                }

                else if (event.key.code == sf::Keyboard::Num3) {
                    return 3;
                }

                else if (event.key.code == sf::Keyboard::Num4) {
                    return 4;
                }
            }
        }

        window.clear(sf::Color(20, 20, 30));

        sf::Text title;
        title.setFont(font);
        title.setString("Chrono Rift");
        title.setCharacterSize(38);
        title.setFillColor(sf::Color::White);
        title.setPosition(185, 35);
        window.draw(title);

        sf::Text subtitle;
        subtitle.setFont(font);
        subtitle.setString("Select Player Party Size");
        subtitle.setCharacterSize(24);
        subtitle.setFillColor(sf::Color::White);
        subtitle.setPosition(160, 90);
        window.draw(subtitle);

        for (int i = 0; i < 4; i++) {
            sf::RectangleShape box;
            box.setSize(sf::Vector2f(280, 45));
            box.setPosition(160, 150 + i * 60);

            if (i == selected) {
                box.setFillColor(sf::Color(80, 120, 220));
            }
            else {
                box.setFillColor(sf::Color(55, 55, 75));
            }

            window.draw(box);

            sf::Text option;
            option.setFont(font);

            string text = to_string(i + 1) + " Player";

            if (i + 1 > 1) {
                text += "s";
            }

            option.setString(text);
            option.setCharacterSize(22);
            option.setFillColor(sf::Color::White);
            option.setPosition(245, 158 + i * 60);

            window.draw(option);
        }

        sf::Text help;
        help.setFont(font);
        help.setString("Use Up/Down and Enter");
        help.setCharacterSize(16);
        help.setFillColor(sf::Color(180, 180, 180));
        help.setPosition(205, 405);
        window.draw(help);

        window.display();
    }

    return 1;
}

void drawBattleView(sf::RenderWindow& window, sf::Font& font, SharedState* state) {
    sem_wait(&state->stateLock);

    // Title
    sf::Text title;
    title.setFont(font);
    title.setString("Chrono Rift Battle View");
    title.setCharacterSize(28);
    title.setFillColor(sf::Color::White);
    title.setPosition(210, 20);
    window.draw(title);

    // Player heading
    sf::Text playerHeading;
    playerHeading.setFont(font);
    playerHeading.setString("Players");
    playerHeading.setCharacterSize(22);
    playerHeading.setFillColor(sf::Color::White);
    playerHeading.setPosition(80, 70);
    window.draw(playerHeading);

    // Enemy heading
    sf::Text enemyHeading;
    enemyHeading.setFont(font);
    enemyHeading.setString("Enemies");
    enemyHeading.setCharacterSize(22);
    enemyHeading.setFillColor(sf::Color::White);
    enemyHeading.setPosition(510, 70);
    window.draw(enemyHeading);

    // Draw players on left
    for (int i = 0; i < state->playerCount; i++) {
        float x = 60;
        float y = 120 + i * 75;

        sf::RectangleShape playerBox;
        playerBox.setSize(sf::Vector2f(170, 55));
        playerBox.setPosition(x, y);

        if (state->players[i].alive == 1) {
            playerBox.setFillColor(sf::Color(60, 130, 220)); // blue
        }
        else {
            playerBox.setFillColor(sf::Color(70, 70, 70)); // dead gray
        }

        if (state->currentTurnType == ENTITY_PLAYER && state->currentTurnId == i) {
            playerBox.setOutlineThickness(4);
            playerBox.setOutlineColor(sf::Color::Yellow);
        }
        else {
            playerBox.setOutlineThickness(2);
            playerBox.setOutlineColor(sf::Color::White);
        }

        window.draw(playerBox);

        sf::Text playerText;
        playerText.setFont(font);
        playerText.setCharacterSize(15);
        playerText.setFillColor(sf::Color::White);

        string text = "Player " + to_string(i)
            + "\nHP: " + to_string(state->players[i].hp)
            + "/" + to_string(state->players[i].maxHp)
            + "  ST: " + to_string(state->players[i].stamina);

        playerText.setString(text);
        playerText.setPosition(x + 10, y + 8);
        window.draw(playerText);
    }

    // Draw enemies on right
    for (int i = 0; i < state->enemyCount; i++) {
        float x = 470;
        float y = 120 + i * 55;

        sf::RectangleShape enemyBox;
        enemyBox.setSize(sf::Vector2f(170, 45));
        enemyBox.setPosition(x, y);

        if (state->enemies[i].alive == 1) {
            enemyBox.setFillColor(sf::Color(190, 70, 70)); // red
        }
        else {
            enemyBox.setFillColor(sf::Color(70, 70, 70)); // dead gray
        }

        if (state->currentTurnType == ENTITY_ENEMY && state->currentTurnId == i) {
            enemyBox.setOutlineThickness(4);
            enemyBox.setOutlineColor(sf::Color::Yellow);
        }
        else {
            enemyBox.setOutlineThickness(2);
            enemyBox.setOutlineColor(sf::Color::White);
        }

        window.draw(enemyBox);

        sf::Text enemyText;
        enemyText.setFont(font);
        enemyText.setCharacterSize(14);
        enemyText.setFillColor(sf::Color::White);

        string text = "Enemy " + to_string(i)
            + "  HP: " + to_string(state->enemies[i].hp)
            + "/" + to_string(state->enemies[i].maxHp)
            + "\nST: " + to_string(state->enemies[i].stamina);

        enemyText.setString(text);
        enemyText.setPosition(x + 10, y + 6);
        window.draw(enemyText);
    }

    // Current turn text
    sf::Text turnText;
    turnText.setFont(font);
    turnText.setCharacterSize(18);
    turnText.setFillColor(sf::Color::White);
    turnText.setPosition(250, 390);

    if (state->currentTurnType == ENTITY_PLAYER) {
        turnText.setString("Current Turn: Player " + to_string(state->currentTurnId));
    }
    else if (state->currentTurnType == ENTITY_ENEMY) {
        turnText.setString("Current Turn: Enemy " + to_string(state->currentTurnId));
    }
    else {
        turnText.setString("Current Turn: None");
    }

    window.draw(turnText);

    sem_post(&state->stateLock);
}

int showActionMenuOnBattleScreen(sf::RenderWindow& window, sf::Font& font, SharedState* state, int playerId) {
    int selected = 0;
    const int optionCount = 2;

    string options[optionCount] = {
        "Strike",
        "Skip"
    };

    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();

                sem_wait(&state->stateLock);
                state->gameStatus = GAME_QUIT;
                sem_post(&state->stateLock);

                return ACTION_SKIP;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Up ||
                    event.key.code == sf::Keyboard::Down) {
                    selected = 1 - selected;
                }
                else if (event.key.code == sf::Keyboard::Enter) {
                    if (selected == 0) {
                        return ACTION_STRIKE;
                    }

                    return ACTION_SKIP;
                }
                else if (event.key.code == sf::Keyboard::Num1) {
                    return ACTION_STRIKE;
                }
                else if (event.key.code == sf::Keyboard::Num2) {
                    return ACTION_SKIP;
                }
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    int mouseX = event.mouseButton.x;
                    int mouseY = event.mouseButton.y;

                    for (int i = 0; i < optionCount; i++) {
                        int boxX = 260;
                        int boxY = 430 + i * 45;
                        int boxW = 180;
                        int boxH = 35;

                        if (mouseX >= boxX && mouseX <= boxX + boxW &&
                            mouseY >= boxY && mouseY <= boxY + boxH) {

                            if (i == 0) {
                                return ACTION_STRIKE;
                            }

                            return ACTION_SKIP;
                        }
                    }
                }
            }
        }

        window.clear(sf::Color(15, 15, 25));

        drawBattleView(window, font, state);

        sf::RectangleShape panel;
        panel.setSize(sf::Vector2f(260, 150));
        panel.setPosition(220, 380);
        panel.setFillColor(sf::Color(35, 35, 55));
        panel.setOutlineThickness(2);
        panel.setOutlineColor(sf::Color::White);
        window.draw(panel);

        sf::Text title;
        title.setFont(font);
        title.setString("Player " + to_string(playerId) + " Action");
        title.setCharacterSize(18);
        title.setFillColor(sf::Color::White);
        title.setPosition(260, 395);
        window.draw(title);

        for (int i = 0; i < optionCount; i++) {
            sf::RectangleShape box;
            box.setSize(sf::Vector2f(180, 35));
            box.setPosition(260, 430 + i * 45);

            if (i == selected) {
                box.setFillColor(sf::Color(80, 120, 220));
            }
            else {
                box.setFillColor(sf::Color(70, 70, 90));
            }

            window.draw(box);

            sf::Text option;
            option.setFont(font);
            option.setString(to_string(i + 1) + ". " + options[i]);
            option.setCharacterSize(16);
            option.setFillColor(sf::Color::White);
            option.setPosition(285, 437 + i * 45);
            window.draw(option);
        }

        window.display();
    }

    return ACTION_SKIP;
}

int showEnemyTargetMenuOnBattleScreen(sf::RenderWindow& window, sf::Font& font, SharedState* state) {
    int selected = 0;

    while (window.isOpen()) {
        sem_wait(&state->stateLock);

        int enemyCount = state->enemyCount;

        if (enemyCount <= 0) {
            sem_post(&state->stateLock);
            return -1;
        }

        if (selected >= enemyCount) {
            selected = enemyCount - 1;
        }

        sem_post(&state->stateLock);

        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();

                sem_wait(&state->stateLock);
                state->gameStatus = GAME_QUIT;
                sem_post(&state->stateLock);

                return -1;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Up) {
                    selected--;

                    if (selected < 0) {
                        sem_wait(&state->stateLock);
                        selected = state->enemyCount - 1;
                        sem_post(&state->stateLock);
                    }
                }
                else if (event.key.code == sf::Keyboard::Down) {
                    selected++;

                    sem_wait(&state->stateLock);

                    if (selected >= state->enemyCount) {
                        selected = 0;
                    }

                    sem_post(&state->stateLock);
                }
                else if (event.key.code == sf::Keyboard::Enter) {
                    sem_wait(&state->stateLock);

                    int valid = 0;

                    if (selected >= 0 &&
                        selected < state->enemyCount &&
                        state->enemies[selected].alive == 1) {
                        valid = 1;
                    }

                    sem_post(&state->stateLock);

                    if (valid == 1) {
                        return selected;
                    }
                }
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    int mouseX = event.mouseButton.x;
                    int mouseY = event.mouseButton.y;

                    sem_wait(&state->stateLock);

                    for (int i = 0; i < state->enemyCount; i++) {
                        int boxX = 235;
                        int boxY = 330 + i * 32;
                        int boxW = 230;
                        int boxH = 28;

                        if (mouseX >= boxX && mouseX <= boxX + boxW &&
                            mouseY >= boxY && mouseY <= boxY + boxH &&
                            state->enemies[i].alive == 1) {

                            sem_post(&state->stateLock);
                            return i;
                        }
                    }

                    sem_post(&state->stateLock);
                }
            }
        }

        window.clear(sf::Color(15, 15, 25));

        drawBattleView(window, font, state);

        sf::RectangleShape panel;
        panel.setSize(sf::Vector2f(300, 260));
        panel.setPosition(200, 280);
        panel.setFillColor(sf::Color(35, 35, 55));
        panel.setOutlineThickness(2);
        panel.setOutlineColor(sf::Color::White);
        window.draw(panel);

        sf::Text title;
        title.setFont(font);
        title.setString("Select Enemy Target");
        title.setCharacterSize(18);
        title.setFillColor(sf::Color::White);
        title.setPosition(255, 295);
        window.draw(title);

        sem_wait(&state->stateLock);

        for (int i = 0; i < state->enemyCount; i++) {
            sf::RectangleShape box;
            box.setSize(sf::Vector2f(230, 28));
            box.setPosition(235, 330 + i * 32);

            if (state->enemies[i].alive == 0) {
                box.setFillColor(sf::Color(60, 60, 60));
            }
            else if (i == selected) {
                box.setFillColor(sf::Color(180, 80, 80));
            }
            else {
                box.setFillColor(sf::Color(70, 70, 90));
            }

            window.draw(box);

            string line = "Enemy " + to_string(i)
                + " HP: " + to_string(state->enemies[i].hp)
                + "/" + to_string(state->enemies[i].maxHp);

            sf::Text enemyText;
            enemyText.setFont(font);
            enemyText.setString(line);
            enemyText.setCharacterSize(13);
            enemyText.setFillColor(sf::Color::White);
            enemyText.setPosition(245, 335 + i * 32);
            window.draw(enemyText);
        }

        sem_post(&state->stateLock);

        window.display();
    }

    return -1;
}

void handlePlayerTurnInput(sf::RenderWindow& window, sf::Font& font, SharedState* state, int playerId) {
    sem_wait(&state->stateLock);

    if (state->inputBuffer.hasInput == 1) {
        sem_post(&state->stateLock);
        return;
    }

    sem_post(&state->stateLock);

    int actionType = showActionMenuOnBattleScreen(window, font, state, playerId);
    int targetType = ENTITY_NONE;
    int targetId = -1;

    if (actionType == ACTION_STRIKE) {
        targetId = showEnemyTargetMenuOnBattleScreen(window, font, state);

        if (targetId == -1) {
            actionType = ACTION_SKIP;
            targetType = ENTITY_NONE;
        }
        else {
            targetType = ENTITY_ENEMY;
        }
    }

    writeInputBuffer(state, playerId, actionType, targetType, targetId);
    cout << "HIP wrote input buffer for Player " << playerId
     << " action: " << actionType
     << " target: " << targetId << endl;
}

int main() {
    sleep(1);

    SharedState* state = attachSharedMemory();

    if (state == NULL) {
        cout << "HIP failed to attach shared memory." << endl;
        return 1;
    }

    cout << "HIP attached to shared memory." << endl;
sf::RenderWindow window(sf::VideoMode(700, 500), "Chrono Rift - HIP");

sf::Font font;

if (!font.loadFromFile("assets/font.ttf")) {
    cout << "Font not found: assets/font.ttf" << endl;
    detachSharedMemory(state);
    return 1;
}
    int partySize = selectPartySize(window, font);

    sem_wait(&state->stateLock);

    state->partySize = partySize;
    state->partySizeSelected = 1;

    sem_post(&state->stateLock);

    cout << "HIP selected party size: " << partySize << endl;

    while (state->gameInitialized == 0) {
        usleep(100000);
    }

    sem_wait(&state->stateLock);

    cout << "\nPlayers initialized by Arbiter:" << endl;

    for (int i = 0; i < state->playerCount; i++) {
        cout << "Player " << i
             << " HP: " << state->players[i].hp
             << " Damage: " << state->players[i].damage
             << " Speed: " << state->players[i].speed
             << " Stamina: " << state->players[i].stamina
             << endl;
    }

   sem_post(&state->stateLock);
createPlayerThreads(state);

int lastHandledTurnType = ENTITY_NONE;
int lastHandledTurnId = -1;

while (window.isOpen() && state->gameStatus == GAME_RUNNING) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();

                sem_wait(&state->stateLock);
                state->gameStatus = GAME_QUIT;
                sem_post(&state->stateLock);
            }
        }

        sem_wait(&state->stateLock);

        int isPlayerTurn = 0;
        int activePlayerId = -1;

        if (state->currentTurnType == ENTITY_PLAYER) {
            isPlayerTurn = 1;
            activePlayerId = state->currentTurnId;
        }

        sem_post(&state->stateLock);
if (isPlayerTurn == 1) {
    if (lastHandledTurnType != ENTITY_PLAYER || lastHandledTurnId != activePlayerId) {
        handlePlayerTurnInput(window, font, state, activePlayerId);

        lastHandledTurnType = ENTITY_PLAYER;
        lastHandledTurnId = activePlayerId;
    }
    else {
        window.clear(sf::Color(15, 15, 25));
        drawBattleView(window, font, state);
        window.display();
    }
}
else {
    lastHandledTurnType = ENTITY_NONE;
    lastHandledTurnId = -1;

    window.clear(sf::Color(15, 15, 25));
    drawBattleView(window, font, state);
    window.display();
}
        usleep(100000);
    }
detachSharedMemory(state);

cout << "HIP finished party selection test." << endl;

    return 0;
}