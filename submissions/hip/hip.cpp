#include "player_threads.h"
#include "shared_memory.h"
#include <SFML/Graphics.hpp>
#include "player_actions.h"
#include "input_buffer.h"
#include <iostream>
#include <unistd.h>
#include <csignal>
#include <algorithm>
#include <vector>
#include <pthread.h>

using namespace std;

struct RenderThreadData
{
    sf::RenderWindow *window;
    sf::Font *font;
    SharedState *state;
    int running;
    int renderPaused;
    int playerHitFrames[MAX_PLAYERS];
    int prevPlayerHp[MAX_PLAYERS];
};

RenderThreadData renderData;
pthread_t renderThread;
void pauseRendering()
{
    renderData.renderPaused = 1;
    usleep(100000);
}
void resumeRendering()
{
    renderData.renderPaused = 0;
}

void requestQuit(SharedState *state)
{
    sem_wait(&state->stateLock);
    int arbiterPid = state->arbiterPid;
    state->gameStatus = GAME_QUIT;
    sem_post(&state->stateLock);

    sem_post(&state->actionReady);

    if (arbiterPid > 0)
    {
        kill(arbiterPid, SIGTERM);
    }
}

void centerText(sf::Text &text, float x, float y, float w, float h)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(
        x + (w - bounds.width) / 2 - bounds.left,
        y + (h - bounds.height) / 2 - bounds.top);
}

void drawBox(sf::RenderWindow &window, float x, float y, float w, float h, string title, sf::Font &font)
{
    sf::RectangleShape box;
    box.setSize(sf::Vector2f(w, h));
    box.setPosition(x, y);
    box.setFillColor(sf::Color::Black);
    box.setOutlineThickness(1);
    box.setOutlineColor(sf::Color(210, 210, 210));
    window.draw(box);

    sf::Text titleText;
    titleText.setFont(font);
    titleText.setString("[ " + title + " ]");
    titleText.setCharacterSize(15);
    titleText.setFillColor(sf::Color::White);
    titleText.setPosition(x + 10, y + 8);
    window.draw(titleText);
}

sf::RectangleShape makeBar(float x, float y, float w, float h, int value, int maxValue, sf::Color color)
{
    if (maxValue <= 0)
    {
        maxValue = 1;
    }

    float percent = (float)value / maxValue;

    if (percent < 0)
    {
        percent = 0;
    }

    if (percent > 1)
    {
        percent = 1;
    }

    sf::RectangleShape bar;
    bar.setSize(sf::Vector2f(w * percent, h));
    bar.setPosition(x, y);
    bar.setFillColor(color);

    return bar;
}

string wrapLine(const string &source, int maxChars)
{
    if (maxChars <= 0)
    {
        return source;
    }

    string result;
    int currentChars = 0;
    int lastSpace = -1;

    for (char c : source)
    {
        result.push_back(c);

        if (c == ' ')
        {
            lastSpace = (int)result.size() - 1;
        }

        currentChars++;

        if (currentChars >= maxChars && lastSpace != -1)
        {
            result[lastSpace] = '\n';
            currentChars = (int)result.size() - lastSpace - 1;
            lastSpace = -1;
        }
    }

    return result;
}

int selectPartySize(sf::RenderWindow &window, sf::Font &font, SharedState *state)
{
    int selected = 0;

    while (window.isOpen())
    {
        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
                return 1;
            }

            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Q)
                {
                    requestQuit(state);
                    window.close();
                    return 1;
                }
                if (event.key.code == sf::Keyboard::Up)
                {
                    selected--;

                    if (selected < 0)
                    {
                        selected = 3;
                    }
                }
                else if (event.key.code == sf::Keyboard::Down)
                {
                    selected++;

                    if (selected > 3)
                    {
                        selected = 0;
                    }
                }
                else if (event.key.code == sf::Keyboard::Enter)
                {
                    return selected + 1;
                }
                else if (event.key.code == sf::Keyboard::Num1)
                {
                    return 1;
                }
                else if (event.key.code == sf::Keyboard::Num2)
                {
                    return 2;
                }
                else if (event.key.code == sf::Keyboard::Num3)
                {
                    return 3;
                }
                else if (event.key.code == sf::Keyboard::Num4)
                {
                    return 4;
                }
            }
        }

        window.clear(sf::Color::Black);

        float winW = (float)window.getSize().x;
        float panelW = 520;
        float panelH = 430;
        float panelX = (winW - panelW) / 2;
        float panelY = 120;

        drawBox(window, panelX, panelY, panelW, panelH, "CHRONO RIFT", font);

        sf::Text title;
        title.setFont(font);
        title.setString("CHRONO RIFT");
        title.setCharacterSize(32);
        title.setFillColor(sf::Color(80, 210, 255));
        centerText(title, panelX, panelY + 35, panelW, 45);
        window.draw(title);

        sf::Text subtitle;
        subtitle.setFont(font);
        subtitle.setString("SELECT PLAYER PARTY SIZE");
        subtitle.setCharacterSize(19);
        subtitle.setFillColor(sf::Color::White);
        centerText(subtitle, panelX, panelY + 85, panelW, 35);
        window.draw(subtitle);

        for (int i = 0; i < 4; i++)
        {
            float boxW = 320;
            float boxH = 45;
            float boxX = panelX + (panelW - boxW) / 2;
            float boxY = panelY + 150 + i * 60;

            sf::RectangleShape optionBox;
            optionBox.setSize(sf::Vector2f(boxW, boxH));
            optionBox.setPosition(boxX, boxY);
            optionBox.setOutlineThickness(1);

            if (i == selected)
            {
                optionBox.setFillColor(sf::Color(35, 75, 95));
                optionBox.setOutlineColor(sf::Color(80, 210, 255));
            }
            else
            {
                optionBox.setFillColor(sf::Color(15, 15, 20));
                optionBox.setOutlineColor(sf::Color(120, 120, 120));
            }

            window.draw(optionBox);

            sf::Text option;
            option.setFont(font);

            string text = to_string(i + 1) + " PLAYER";

            if (i + 1 > 1)
            {
                text += "S";
            }

            option.setString(text);
            option.setCharacterSize(18);
            option.setFillColor(sf::Color::White);
            centerText(option, boxX, boxY, boxW, boxH);
            window.draw(option);
        }

        sf::Text help;
        help.setFont(font);
        help.setString("Use UP / DOWN and ENTER");
        help.setCharacterSize(15);
        help.setFillColor(sf::Color(180, 180, 180));
        centerText(help, panelX, panelY + 375, panelW, 30);
        window.draw(help);

        window.display();
    }

    return 1;
}
void drawInventoryPanel(sf::RenderWindow &window,
                        sf::Font &font,
                        SharedState *state,
                        int selectedPlayer,
                        const vector<string> *menuLines = nullptr)
{
    float winW = (float)window.getSize().x;
    float winH = (float)window.getSize().y;
    const float margin = 20.0f;
    const float contentTop = 80.0f;
    const float headerHeight = 35.0f;
    const float logHeight = std::min(180.0f, winH * 0.22f);
    float availableHeight = winH - contentTop - headerHeight - logHeight - margin * 4.0f;
    float topPanelHeight = std::max(170.0f, availableHeight);
    float bottomY = contentTop + topPanelHeight + margin;
    float bottomH = std::max(120.0f, winH - bottomY - margin);
    float logWidth = std::min(420.0f, (winW - 60.0f) * 0.35f);
    float panelX = 20.0f + logWidth + 20.0f;
    float panelY = bottomY;
    float panelW = winW - panelX - 20.0f;
    float panelH = bottomH;

    if (panelW < 260.0f)
    {
        panelW = 260.0f;
        panelX = winW - panelW - 20.0f;
    }

    drawBox(window, panelX - 5.0f, panelY - 5.0f, panelW + 10.0f, panelH + 10.0f, "INVENTORY", font);

    float contentX = panelX + 10.0f;
    float contentY = panelY + 20.0f;
    float lineHeight = 22.0f;

    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(16);
    text.setFillColor(sf::Color::White);

    text.setPosition(contentX, contentY);
    text.setString("[ INVENTORY ]");
    window.draw(text);

    contentY += 26.0f;

    if (menuLines != nullptr)
    {
        text.setCharacterSize(14);

        for (const string &menuLine : *menuLines)
        {
            text.setPosition(contentX, contentY);
            text.setString(menuLine);
            window.draw(text);
            contentY += lineHeight;
        }

        contentY += 10.0f;
        text.setCharacterSize(16);
    }

    char invInfo[80];
    sprintf(invInfo, "Weapons: %d | Storage: %d",
            state->players[selectedPlayer].inventory.weaponCount,
            state->players[selectedPlayer].inventory.storageCount);

    text.setPosition(contentX, contentY + 20);
    text.setString(invInfo);
    window.draw(text);
    contentY += 50;

    PlayerInventory &inv = state->players[selectedPlayer].inventory;

    const float slotSize = 18.0f;
    const float slotGap = 6.0f;
    const float availableWidth = panelW - 10.0f;
    int slotsPerRow = (int)((availableWidth + slotGap) / (slotSize + slotGap));

    if (slotsPerRow < 1)
    {
        slotsPerRow = 1;
    }

    int slotRows = (INVENTORY_SIZE + slotsPerRow - 1) / slotsPerRow;

    for (int i = 0; i < INVENTORY_SIZE; i++)
    {
        int row = i / slotsPerRow;
        int col = i % slotsPerRow;

        sf::RectangleShape slot(sf::Vector2f(slotSize, slotSize));
        slot.setPosition(contentX + col * (slotSize + slotGap), contentY + row * (slotSize + slotGap));

        if (inv.slots[i] == -1)
        {
            slot.setFillColor(sf::Color(40, 40, 40));
        }
        else
        {
            slot.setFillColor(sf::Color(80, 160, 80));
        }

        slot.setOutlineThickness(1);
        slot.setOutlineColor(sf::Color::White);

        window.draw(slot);
    }

    contentY += slotRows * (slotSize + slotGap) + 30;

    text.setPosition(contentX, contentY);
    text.setString("Active Weapons:");
    window.draw(text);

    contentY += 25;

    for (int i = 0; i < inv.weaponCount; i++)
    {
        if (inv.weapons[i].active == 1)
        {
            char line[140];
            sprintf(line,
                    "%d. %s | DMG:%d | Slots:%d",
                    i + 1,
                    inv.weapons[i].weapon.name,
                    inv.weapons[i].weapon.damage,
                    inv.weapons[i].weapon.slotSize);

            text.setPosition(contentX, contentY);
            text.setString(line);
            window.draw(text);
            contentY += 22;
        }
    }

    contentY += 10;

    text.setPosition(contentX, contentY);
    text.setString("Storage:");
    window.draw(text);

    contentY += 25;

    for (int i = 0; i < inv.storageCount; i++)
    {
        char line[140];
        sprintf(line,
                "%d. %s | DMG:%d",
                i + 1,
                inv.longTermStorage[i].name,
                inv.longTermStorage[i].damage);

        text.setPosition(contentX, contentY);
        text.setString(line);
        window.draw(text);
        contentY += 22;
    }
}
const int HIT_FLASH_TICKS = 12;

void drawBattleView(sf::RenderWindow &window, sf::Font &font, SharedState *state, int playerHitFrames[MAX_PLAYERS])
{
    sem_wait(&state->stateLock);

    float winW = (float)window.getSize().x;
    float winH = (float)window.getSize().y;

    sf::Text header;
    header.setFont(font);
    header.setString("CHRONO RIFT  |  Q=Quit  |  KILLS: " + to_string(state->enemiesKilled) + "/10");
    header.setCharacterSize(15);
    header.setFillColor(sf::Color(80, 210, 255));

    const float margin = 20.0f;
    const float headerHeight = 35.0f;
    const float logHeight = std::min(180.0f, winH * 0.22f);
    const float contentTop = 80.0f;
    const float availableHeight = winH - contentTop - headerHeight - logHeight - margin * 4.0f;
    const float panelHeight = std::max(170.0f, availableHeight);

    sf::RectangleShape headerBox;
    headerBox.setSize(sf::Vector2f(winW - 40, headerHeight));
    headerBox.setPosition(margin, 15);
    headerBox.setFillColor(sf::Color::Black);
    headerBox.setOutlineThickness(1);
    headerBox.setOutlineColor(sf::Color(80, 210, 255));
    window.draw(headerBox);

    header.setPosition(margin + 25, 24);
    window.draw(header);

    float partyX = margin;
    float partyY = contentTop;
    float partyW = std::max(260.0f, (winW - margin * 3.0f) / 2.0f);
    float partyH = panelHeight;
    float enemyX = partyX + partyW + margin;
    float enemyY = partyY;
    float enemyW = partyW;
    float enemyH = panelHeight;

    drawBox(window, partyX, partyY, partyW, partyH, "PARTY", font);
    drawBox(window, enemyX, enemyY, enemyW, enemyH, "VOID WRAITHS", font);

    float playerRowHeight = std::max(38.0f, (partyH - 60.0f) / (float)std::max(1, state->playerCount));
    float playerHpWidth = std::min(220.0f, partyW * 0.36f);
    float playerStaminaWidth = std::min(150.0f, partyW - playerHpWidth - 120.0f);
    playerStaminaWidth = std::max(70.0f, playerStaminaWidth);
    float playerHpX = partyX + 95.0f;
    float playerStaminaX = partyX + partyW - playerStaminaWidth - 15.0f;

    for (int i = 0; i < state->playerCount; i++)
    {
        float y = partyY + 45.0f + i * playerRowHeight;

        sf::Text label;
        label.setFont(font);
        label.setCharacterSize(15);
        label.setFillColor(sf::Color::White);
        label.setString("P" + to_string(i + 1));
        label.setPosition(partyX + 18, y);
        window.draw(label);

        sf::RectangleShape hpBack;
        hpBack.setSize(sf::Vector2f(playerHpWidth, 12));
        hpBack.setPosition(playerHpX, y + 4);
        hpBack.setFillColor(sf::Color(25, 25, 25));
        hpBack.setOutlineThickness(1);
        hpBack.setOutlineColor(sf::Color(80, 180, 120));
        window.draw(hpBack);

        sf::Color hpColor = sf::Color(50, 210, 120);
        if (playerHitFrames != nullptr && playerHitFrames[i] > 0)
        {
            hpColor = sf::Color(210, 60, 60);
        }

        sf::RectangleShape hpBar = makeBar(
            playerHpX + 1, y + 5, playerHpWidth - 2, 10,
            state->players[i].hp,
            state->players[i].maxHp,
            hpColor);
        window.draw(hpBar);

        sf::Text hpText;
        hpText.setFont(font);
        hpText.setCharacterSize(13);
        hpText.setFillColor(sf::Color(200, 240, 220));
        hpText.setString(to_string(state->players[i].hp) + "/" + to_string(state->players[i].maxHp));
        hpText.setPosition(playerHpX + 6, y + 16);
        window.draw(hpText);

        sf::RectangleShape staminaBack;
        staminaBack.setSize(sf::Vector2f(playerStaminaWidth, 8));
        staminaBack.setPosition(playerStaminaX, y + 5);
        staminaBack.setFillColor(sf::Color(25, 25, 25));
        staminaBack.setOutlineThickness(1);
        staminaBack.setOutlineColor(sf::Color(220, 200, 90));
        window.draw(staminaBack);

        sf::RectangleShape staminaBar = makeBar(
            playerStaminaX + 1, y + 5, playerStaminaWidth - 2, 8,
            state->players[i].stamina,
            PLAYER_MAX_STAMINA,
            sf::Color(230, 210, 100));
        window.draw(staminaBar);

        sf::Text staminaText;
        staminaText.setFont(font);
        staminaText.setCharacterSize(12);
        staminaText.setFillColor(sf::Color(230, 210, 100));
        staminaText.setString(to_string(state->players[i].stamina));
        staminaText.setPosition(playerStaminaX + 5, y + 16);
        window.draw(staminaText);

        if (state->currentTurnType == ENTITY_PLAYER && state->currentTurnId == i)
        {
            sf::RectangleShape active;
            active.setSize(sf::Vector2f(partyW - 40.0f, playerRowHeight + 2.0f));
            active.setPosition(partyX + 20.0f, y - 6.0f);
            active.setFillColor(sf::Color::Transparent);
            active.setOutlineThickness(1);
            active.setOutlineColor(sf::Color::Yellow);
            window.draw(active);
        }

        if (playerHitFrames != nullptr && playerHitFrames[i] > 0)
        {
            sf::Text hitText;
            hitText.setFont(font);
            hitText.setCharacterSize(14);
            hitText.setFillColor(sf::Color::Red);
            hitText.setString("HIT!");
            hitText.setPosition(playerHpX + playerHpWidth + 10.0f, y - 2.0f);
            window.draw(hitText);
        }
    }

    float enemyRowHeight = std::max(30.0f, (enemyH - 50.0f) / (float)std::max(1, state->enemyCount));
    float enemyHpWidth = std::min(220.0f, enemyW * 0.40f);
    float enemyStaminaWidth = std::min(160.0f, enemyW - enemyHpWidth - 110.0f);
    enemyStaminaWidth = std::max(70.0f, enemyStaminaWidth);
    float enemyHpX = enemyX + 95.0f;
    float enemyStaminaX = enemyX + enemyW - enemyStaminaWidth - 15.0f;

    for (int i = 0; i < state->enemyCount; i++)
    {
        float y = enemyY + 45.0f + i * enemyRowHeight;

        sf::Text enemyLabel;
        enemyLabel.setFont(font);
        enemyLabel.setCharacterSize(14);
        enemyLabel.setFillColor(sf::Color::White);
        enemyLabel.setString("NPC" + to_string(i + 1));
        enemyLabel.setPosition(enemyX + 18, y);
        window.draw(enemyLabel);

        sf::RectangleShape hpBack;
        hpBack.setSize(sf::Vector2f(enemyHpWidth, 12));
        hpBack.setPosition(enemyHpX, y + 4);
        hpBack.setFillColor(sf::Color(25, 25, 25));
        hpBack.setOutlineThickness(1);
        hpBack.setOutlineColor(sf::Color(230, 90, 90));
        window.draw(hpBack);

        sf::RectangleShape hpBar = makeBar(
            enemyHpX + 1, y + 5, enemyHpWidth - 2, 10,
            state->enemies[i].hp,
            state->enemies[i].maxHp,
            sf::Color(230, 80, 80));
        window.draw(hpBar);

        sf::Text hpText;
        hpText.setFont(font);
        hpText.setCharacterSize(13);
        hpText.setFillColor(sf::Color::White);
        hpText.setString(to_string(state->enemies[i].hp) + "/" + to_string(state->enemies[i].maxHp));
        hpText.setPosition(enemyHpX + 6, y + 16);
        window.draw(hpText);

        sf::RectangleShape staminaBack;
        staminaBack.setSize(sf::Vector2f(enemyStaminaWidth, 8));
        staminaBack.setPosition(enemyStaminaX, y + 5);
        staminaBack.setFillColor(sf::Color(25, 25, 25));
        staminaBack.setOutlineThickness(1);
        staminaBack.setOutlineColor(sf::Color(220, 180, 70));
        window.draw(staminaBack);

        sf::RectangleShape staminaBar = makeBar(
            enemyStaminaX + 1, y + 5, enemyStaminaWidth - 2, 8,
            state->enemies[i].stamina,
            ENEMY_MAX_STAMINA,
            sf::Color(230, 210, 100));
        window.draw(staminaBar);

        sf::Text staminaText;
        staminaText.setFont(font);
        staminaText.setCharacterSize(12);
        staminaText.setFillColor(sf::Color(230, 210, 100));
        staminaText.setString(to_string(state->enemies[i].stamina));
        staminaText.setPosition(enemyStaminaX + 5, y + 16);
        window.draw(staminaText);

        if (state->currentTurnType == ENTITY_ENEMY && state->currentTurnId == i)
        {
            sf::RectangleShape active;
            active.setSize(sf::Vector2f(enemyW - 40.0f, enemyRowHeight + 2.0f));
            active.setPosition(enemyX + 20.0f, y - 5.0f);
            active.setFillColor(sf::Color::Transparent);
            active.setOutlineThickness(1);
            active.setOutlineColor(sf::Color::Yellow);
            window.draw(active);
        }
    }

    float bottomY = contentTop + panelHeight + margin;
    float bottomH = std::max(120.0f, winH - bottomY - margin);
    float logW = std::min(420.0f, (winW - margin * 3.0f) * 0.35f);
    float logX = margin;
    float logY = bottomY;
    float logH = bottomH;

    drawBox(window, logX, logY, logW, logH, "ACTION LOG / WEAPON DROPS", font);

    int visibleEntries = (state->actionLogCount < 10) ? state->actionLogCount : 10;
    int logFontSize = (winW < 1000.0f) ? 12 : 14;
    int maxChars = (int)((logW - 40.0f) / 8.0f);
    maxChars = std::max(30, maxChars);
    float lineHeight = logFontSize + 4.0f;
    int maxLines = std::max(1, (int)((logH - 40.0f) / lineHeight));

    vector<string> wrappedLines;
    for (int i = visibleEntries - 1; i >= 0 && (int)wrappedLines.size() < maxLines; i--)
    {
        string line = "> " + string(state->actionLog[i]);
        string wrapped = wrapLine(line, maxChars);
        string current;

        for (char c : wrapped)
        {
            if (c == '\n')
            {
                wrappedLines.push_back(current);
                current.clear();
                if ((int)wrappedLines.size() >= maxLines)
                {
                    break;
                }
            }
            else
            {
                current.push_back(c);
            }
        }

        if ((int)wrappedLines.size() >= maxLines)
        {
            break;
        }

        if (!current.empty())
        {
            wrappedLines.push_back(current);
        }
    }

    sf::Text logText;
    logText.setFont(font);
    logText.setCharacterSize(logFontSize);
    logText.setFillColor(sf::Color::White);

    float lineY = logY + 30.0f;
    for (int i = 0; i < (int)wrappedLines.size(); i++)
    {
        logText.setString(wrappedLines[i]);
        logText.setPosition(logX + 15, lineY + i * lineHeight);
        window.draw(logText);
    }

    sf::Text turnText;
    turnText.setFont(font);
    turnText.setCharacterSize(15);
    turnText.setFillColor(sf::Color::White);

    if (state->currentTurnType == ENTITY_PLAYER)
    {
        turnText.setString("== P" + to_string(state->currentTurnId + 1) + " TURN ==");
    }
    else if (state->currentTurnType == ENTITY_ENEMY)
    {
        turnText.setString("== NPC" + to_string(state->currentTurnId + 1) + " TURN ==");
    }
    else
    {
        turnText.setString("== WAITING ==");
    }

    turnText.setPosition(margin, logY + logH + 10.0f);
    window.draw(turnText);

    sem_post(&state->stateLock);
}

void drawEndScreen(sf::RenderWindow &window, sf::Font &font, int status)
{
    string title;
    string subtitle;
    string description;

    if (status == GAME_WIN)
    {
        title = "VICTORY";
        subtitle = "All enemies have been defeated.";
        description = "You killed 10 enemies and escaped the Chrono Rift.";
    }
    else if (status == GAME_LOSE)
    {
        title = "DEFEAT";
        subtitle = "All player characters have fallen.";
        description = "The party was overwhelmed by the Void Wraiths.";
    }
    else if (status == GAME_QUIT)
    {
        title = "QUIT";
        subtitle = "Human Interfacing Process requested termination.";
        description = "The Arbiter received SIGTERM and the game is ending.";
    }
    else
    {
        title = "GAME OVER";
        subtitle = "The game has ended.";
        description = " ";
    }

    drawBox(window, 170, 150, 760, 420, "GAME OVER", font);

    sf::Text titleText;
    titleText.setFont(font);
    titleText.setString(title);
    titleText.setCharacterSize(46);
    titleText.setFillColor(sf::Color(210, 210, 255));
    centerText(titleText, 170, 190, 760, 60);
    window.draw(titleText);

    sf::Text subtitleText;
    subtitleText.setFont(font);
    subtitleText.setString(subtitle);
    subtitleText.setCharacterSize(24);
    subtitleText.setFillColor(sf::Color::White);
    centerText(subtitleText, 170, 265, 760, 40);
    window.draw(subtitleText);

    sf::Text descriptionText;
    descriptionText.setFont(font);
    descriptionText.setString(description);
    descriptionText.setCharacterSize(18);
    descriptionText.setFillColor(sf::Color(180, 180, 180));
    centerText(descriptionText, 170, 330, 760, 40);
    window.draw(descriptionText);

    sf::Text promptText;
    promptText.setFont(font);
    promptText.setString(" ");
    promptText.setCharacterSize(16);
    promptText.setFillColor(sf::Color(160, 160, 160));
    centerText(promptText, 170, 380, 760, 30);
    window.draw(promptText);
}

void *renderThreadFunction(void *arg)
{

    RenderThreadData *data = (RenderThreadData *)arg;
    SharedState *state = data->state;
    sf::RenderWindow *window = data->window;
    sf::Font *font = data->font;
    window->setActive(true);
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        data->playerHitFrames[i] = 0;
        data->prevPlayerHp[i] = 0;
    }

    sem_wait(&state->stateLock);
    for (int i = 0; i < state->playerCount && i < MAX_PLAYERS; i++)
    {
        data->prevPlayerHp[i] = state->players[i].hp;
    }
    sem_post(&state->stateLock);

    while (data->running == 1 && window->isOpen())
    {
        if (data->renderPaused == 1)
        {
            window->setActive(false);

            while (data->renderPaused == 1 && data->running == 1)
            {
                usleep(30000);
            }

            window->setActive(true);
            continue;
        }

        sf::Event event;
        while (window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                requestQuit(state);
                window->close();
            }
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Q)
            {
                requestQuit(state);
            }
        }

        sem_wait(&state->stateLock);
        int currentStatus = state->gameStatus;

        for (int i = 0; i < state->playerCount && i < MAX_PLAYERS; i++)
        {
            int hp = state->players[i].hp;

            if (hp < data->prevPlayerHp[i])
            {
                data->playerHitFrames[i] = HIT_FLASH_TICKS;
            }

            data->prevPlayerHp[i] = hp;
        }
        sem_post(&state->stateLock);

        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (data->playerHitFrames[i] > 0)
            {
                data->playerHitFrames[i]--;
            }
        }

        window->clear(sf::Color(15, 15, 25));

        if (currentStatus == GAME_RUNNING)
        {
            drawBattleView(*window, *font, state, data->playerHitFrames);
        }
        else
        {
            drawEndScreen(*window, *font, currentStatus);
        }

        window->display();
        usleep(50000);
    }
    window->setActive(false);
    return NULL;
}

int showActionMenuOnBattleScreen(sf::RenderWindow &window, sf::Font &font, SharedState *state, int playerId)
{
    int selected = 0;
    const int optionCount = 7;
    pauseRendering();
    window.setActive(true);
    while (window.isOpen())
    {
        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();

                requestQuit(state);
                window.setActive(false);
                resumeRendering();
                return ACTION_SKIP;
            }

            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Q)
                {
                    requestQuit(state);
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_SKIP;
                }
                if (event.key.code == sf::Keyboard::Left || event.key.code == sf::Keyboard::Up)
                {
                    selected--;

                    if (selected < 0)
                    {
                        selected = optionCount - 1;
                    }
                }
                else if (event.key.code == sf::Keyboard::Right || event.key.code == sf::Keyboard::Down)
                {
                    selected++;

                    if (selected >= optionCount)
                    {
                        selected = 0;
                    }
                }
                else if (event.key.code == sf::Keyboard::Enter)
                {
                    if (selected == 0)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return ACTION_STRIKE;
                    }
                    if (selected == 1)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return ACTION_EXHAUST;
                    }
                    if (selected == 2)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return ACTION_USE_WEAPON;
                    }
                    if (selected == 3)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return ACTION_SWAP_IN;
                    }
                    if (selected == 4)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return ACTION_HEAL;
                    }
                    if (selected == 5)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return ACTION_ULTIMATE;
                    }

                    window.setActive(false);
                    resumeRendering();
                    return ACTION_SKIP;
                }
                else if (event.key.code == sf::Keyboard::Num1)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_STRIKE;
                }
                else if (event.key.code == sf::Keyboard::Num2)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_EXHAUST;
                }
                else if (event.key.code == sf::Keyboard::Num3)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_USE_WEAPON;
                }
                else if (event.key.code == sf::Keyboard::Num4)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_SWAP_IN;
                }
                else if (event.key.code == sf::Keyboard::Num5)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_HEAL;
                }
                else if (event.key.code == sf::Keyboard::Num6)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_ULTIMATE;
                }
                else if (event.key.code == sf::Keyboard::Num7)
                {
                    window.setActive(false);
                    resumeRendering();
                    return ACTION_SKIP;
                }
            }
        }
        window.clear(sf::Color::Black);
        drawBattleView(window, font, state, nullptr);

        vector<string> menuLines;
        menuLines.push_back((selected == 0 ? "> " : "  ") + string("[1] Strike    ") + (selected == 1 ? "> " : "  ") + string("[2] Exhaust"));
        menuLines.push_back((selected == 2 ? "> " : "  ") + string("[3] Use Weapon    ") + (selected == 3 ? "> " : "  ") + string("[4] Swap In"));
        menuLines.push_back((selected == 4 ? "> " : "  ") + string("[5] Heal    ") + (selected == 5 ? "> " : "  ") + string("[6] Ultimate    ") + (selected == 6 ? "> " : "  ") + string("[7] Skip"));
        drawInventoryPanel(window, font, state, playerId, &menuLines);
        window.display();
    }
    window.setActive(false);
    resumeRendering();
    return ACTION_SKIP;
}

int showEnemyTargetMenuOnBattleScreen(sf::RenderWindow &window, sf::Font &font, SharedState *state)
{
    int selected = 0;
    pauseRendering();
    window.setActive(true);

    while (window.isOpen())
    {
        sem_wait(&state->stateLock);

        int enemyCount = state->enemyCount;

        if (enemyCount <= 0)
        {
            sem_post(&state->stateLock);
            window.setActive(false);
            resumeRendering();
            return -1;
        }

        if (selected >= enemyCount)
        {
            selected = enemyCount - 1;
        }

        sem_post(&state->stateLock);

        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();

                requestQuit(state);
                window.setActive(false);
                resumeRendering();
                return -1;
            }

            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Q)
                {
                    requestQuit(state);
                    window.setActive(false);
                    resumeRendering();
                    return -1;
                }
                if (event.key.code == sf::Keyboard::Up)
                {
                    selected--;

                    if (selected < 0)
                    {
                        sem_wait(&state->stateLock);
                        selected = state->enemyCount - 1;
                        sem_post(&state->stateLock);
                    }
                }
                else if (event.key.code == sf::Keyboard::Down)
                {
                    selected++;

                    sem_wait(&state->stateLock);

                    if (selected >= state->enemyCount)
                    {
                        selected = 0;
                    }

                    sem_post(&state->stateLock);
                }
                else if (event.key.code == sf::Keyboard::Enter)
                {
                    sem_wait(&state->stateLock);

                    int valid = 0;

                    if (selected >= 0 &&
                        selected < state->enemyCount &&
                        state->enemies[selected].alive == 1)
                    {
                        valid = 1;
                    }

                    sem_post(&state->stateLock);

                    if (valid == 1)
                    {
                        window.setActive(false);
                        resumeRendering();
                        return selected;
                    }
                }
            }
        }

        window.clear(sf::Color::Black);
        drawBattleView(window, font, state, nullptr);

        float winW = (float)window.getSize().x;
        float winH = (float)window.getSize().y;
        float logSide = std::min(420.0f, std::min((winW - 60.0f) * 0.35f, winH * 0.35f));
        float panelX = 20.0f + logSide + 20.0f;
        float panelY = winH - logSide - 10.0f;
        float panelW = winW - panelX - 20.0f;
        float panelH = logSide;

        if (panelW < 260.0f)
        {
            panelW = 260.0f;
            panelX = winW - panelW - 20.0f;
        }

        drawBox(window, panelX - 5.0f, panelY - 5.0f, panelW + 10.0f, panelH + 10.0f, "TARGET ENEMY", font);

        sf::Text targetText;
        targetText.setFont(font);
        targetText.setCharacterSize(14);
        targetText.setFillColor(sf::Color::White);

        float rowY = panelY + 25.0f;
        float lineHeight = 22.0f;

        sem_wait(&state->stateLock);

        for (int i = 0; i < state->enemyCount && rowY + lineHeight < panelY + panelH - 10.0f; i++)
        {
            string line = (i == selected ? "> " : "  ");
            line += "[NPC" + to_string(i + 1) + " " + to_string(state->enemies[i].hp) + "HP]";

            targetText.setString(line);
            targetText.setPosition(panelX + 10.0f, rowY);
            window.draw(targetText);
            rowY += lineHeight;
        }

        sem_post(&state->stateLock);

        window.display();
    }
    window.setActive(false);
    resumeRendering();
    return -1;
}
int showWeaponSelectMenu(sf::RenderWindow &window, sf::Font &font,
                         SharedState *state, int playerId)
{
    pauseRendering();
    window.setActive(true);
    int selected = 0;

    while (window.isOpen())
    {
        // ── Snapshot inventory under lock ─────────────────────────────────
        sem_wait(&state->stateLock);
        PlayerInventory inv = state->players[playerId].inventory;
        sem_post(&state->stateLock);

        // Build a list of active weapon indices
        int activeIdx[MAX_WEAPONS];
        int activeCount = 0;
        for (int i = 0; i < inv.weaponCount; i++)
        {
            if (inv.weapons[i].active == 1)
                activeIdx[activeCount++] = i;
        }

        if (activeCount == 0)
        {
            // Nothing to pick — treat as cancel
            window.setActive(false);
            resumeRendering();
            return -1;
        }

        if (selected >= activeCount)
            selected = activeCount - 1;
        if (selected < 0)
            selected = 0;

        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                requestQuit(state);
                window.setActive(false);
                resumeRendering();
                return -1;
            }
            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Q)
                {
                    requestQuit(state);
                    window.setActive(false);
                    resumeRendering();
                    return -1;
                }
                if (event.key.code == sf::Keyboard::Up)
                {
                    selected--;
                    if (selected < 0)
                        selected = activeCount - 1;
                }
                else if (event.key.code == sf::Keyboard::Down)
                {
                    selected++;
                    if (selected >= activeCount)
                        selected = 0;
                }
                else if (event.key.code == sf::Keyboard::Enter)
                {
                    window.setActive(false);
                    resumeRendering();
                    return activeIdx[selected]; // weapon index in inventory.weapons[]
                }
                else if (event.key.code == sf::Keyboard::Escape)
                {
                    window.setActive(false);
                    resumeRendering();
                    return -1; // player cancelled
                }
            }
        }

        window.clear(sf::Color::Black);
        drawBattleView(window, font, state, nullptr);

        // ── Draw weapon list ──────────────────────────────────────────────
        float x = 35;
        float y = 670;

        sf::Text header;
        header.setFont(font);
        header.setCharacterSize(14);
        header.setFillColor(sf::Color(80, 210, 255));
        header.setString("SELECT WEAPON (UP/DOWN + ENTER, ESC=cancel):");
        header.setPosition(x, y);
        window.draw(header);
        y += 20;

        for (int i = 0; i < activeCount; i++)
        {
            int wi = activeIdx[i];
            char line[120];
            sprintf(line, "%s%d. %s  DMG:%d  Slots:%d",
                    (i == selected) ? "> " : "  ",
                    i + 1,
                    inv.weapons[wi].weapon.name,
                    inv.weapons[wi].weapon.damage,
                    inv.weapons[wi].weapon.slotSize);

            sf::Text entry;
            entry.setFont(font);
            entry.setCharacterSize(13);
            entry.setFillColor((i == selected) ? sf::Color::Yellow : sf::Color::White);
            entry.setString(line);
            entry.setPosition(x, y);
            window.draw(entry);
            y += 18;
        }

        window.display();
    }

    window.setActive(false);
    resumeRendering();
    return -1;
}
int showStorageSelectMenu(sf::RenderWindow &window, sf::Font &font,
                          SharedState *state, int playerId)
{
    pauseRendering();
    window.setActive(true);
    int selected = 0;

    while (window.isOpen())
    {
        sem_wait(&state->stateLock);
        PlayerInventory inv = state->players[playerId].inventory;
        sem_post(&state->stateLock);

        if (inv.storageCount == 0)
        {
            window.setActive(false);
            resumeRendering();
            return -1; // nothing in storage
        }

        if (selected >= inv.storageCount)
            selected = inv.storageCount - 1;
        if (selected < 0)
            selected = 0;

        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                requestQuit(state);
                window.setActive(false);
                resumeRendering();
                return -1;
            }
            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Q)
                {
                    requestQuit(state);
                    window.setActive(false);
                    resumeRendering();
                    return -1;
                }
                if (event.key.code == sf::Keyboard::Up)
                {
                    selected--;
                    if (selected < 0)
                        selected = inv.storageCount - 1;
                }
                else if (event.key.code == sf::Keyboard::Down)
                {
                    selected++;
                    if (selected >= inv.storageCount)
                        selected = 0;
                }
                else if (event.key.code == sf::Keyboard::Enter)
                {
                    window.setActive(false);
                    resumeRendering();
                    return selected; // storage index
                }
                else if (event.key.code == sf::Keyboard::Escape)
                {
                    window.setActive(false);
                    resumeRendering();
                    return -1;
                }
            }
        }

        window.clear(sf::Color::Black);
        drawBattleView(window, font, state, nullptr);

        float x = 35;
        float y = 670;

        sf::Text header;
        header.setFont(font);
        header.setCharacterSize(14);
        header.setFillColor(sf::Color(80, 210, 255));
        header.setString("SWAP IN FROM STORAGE (UP/DOWN + ENTER, ESC=cancel):");
        header.setPosition(x, y);
        window.draw(header);
        y += 20;

        for (int i = 0; i < inv.storageCount; i++)
        {
            char line[120];
            sprintf(line, "%s%d. %s  DMG:%d  Slots:%d",
                    (i == selected) ? "> " : "  ",
                    i + 1,
                    inv.longTermStorage[i].name,
                    inv.longTermStorage[i].damage,
                    inv.longTermStorage[i].slotSize);

            sf::Text entry;
            entry.setFont(font);
            entry.setCharacterSize(13);
            entry.setFillColor((i == selected) ? sf::Color::Yellow : sf::Color::White);
            entry.setString(line);
            entry.setPosition(x, y);
            window.draw(entry);
            y += 18;
        }

        window.display();
    }

    window.setActive(false);
    resumeRendering();
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// handlePlayerTurnInput  (replaces the old version)
//
// Flow:
//   1. Show action menu → get actionType
//   2a. Strike / Exhaust        → show enemy target menu → targetId
//   2b. Use Weapon              → show weapon select menu → weaponIndex
//                                 then show enemy target menu → targetId
//   2c. Swap In                 → show storage select menu → storageIndex
//   2d. Heal / Ultimate / Skip  → no secondary menu needed
//   3. Write to input buffer
// ─────────────────────────────────────────────────────────────────────────────
void handlePlayerTurnInput(sf::RenderWindow &window, sf::Font &font,
                           SharedState *state, int playerId)
{
    sem_wait(&state->stateLock);
    if (state->inputBuffer.hasInput == 1)
    {
        sem_post(&state->stateLock);
        return;
    }
    sem_post(&state->stateLock);

    int actionType = showActionMenuOnBattleScreen(window, font, state, playerId);

    int targetType = ENTITY_NONE;
    int targetId = -1;

    if (actionType == ACTION_STRIKE || actionType == ACTION_EXHAUST)
    {
        targetId = showEnemyTargetMenuOnBattleScreen(window, font, state);
        targetType = ENTITY_ENEMY;
    }
    else if (actionType == ACTION_USE_WEAPON)
    {
        // Step 1: pick which weapon to use
        int weaponIndex = showWeaponSelectMenu(window, font, state, playerId);

        if (weaponIndex == -1)
        {
            // Player cancelled or no weapons — fall back to skip
            actionType = ACTION_SKIP;
            targetType = ENTITY_NONE;
            targetId = -1;
        }
        else
        {
            // Step 2: pick which enemy to hit
            targetId = showEnemyTargetMenuOnBattleScreen(window, font, state);
            // targetType carries the weapon index so the Arbiter knows
            // which inventory slot to use for damage (matches processAction).
            targetType = weaponIndex;
        }
    }
    else if (actionType == ACTION_SWAP_IN)
    {
        int storageIndex = showStorageSelectMenu(window, font, state, playerId);

        if (storageIndex == -1)
        {
            // Nothing in storage or player cancelled — fall back to skip
            actionType = ACTION_SKIP;
            targetType = ENTITY_NONE;
            targetId = -1;
        }
        else
        {
            targetId = storageIndex;
            targetType = ENTITY_NONE;
        }
    }
    // ACTION_HEAL, ACTION_ULTIMATE, ACTION_SKIP need no secondary menu.

    writeInputBuffer(state, playerId, actionType, targetType, targetId);

    cout << "[HIP] Player " << playerId
         << " action=" << actionType
         << " targetType=" << targetType
         << " targetId=" << targetId << endl;
}
int main()
{
    sleep(1);

    SharedState *state = attachSharedMemory();

    if (state == NULL)
    {
        cout << "HIP failed to attach shared memory." << endl;
        return 1;
    }

    cout << "HIP attached to shared memory." << endl;
    sf::RenderWindow window(sf::VideoMode(1100, 800), "Chrono Rift - HIP");
    window.setActive(false);

    sf::Font font;

    if (!font.loadFromFile("assets/font.ttf"))
    {
        cout << "Font not found: assets/font.ttf" << endl;
        detachSharedMemory(state);
        return 1;
    }
    int partySize = selectPartySize(window, font, state);
    window.setActive(false);
    sem_wait(&state->stateLock);

    state->partySize = partySize;
    state->partySizeSelected = 1;

    sem_post(&state->stateLock);

    cout << "HIP selected party size: " << partySize << endl;

    while (state->gameInitialized == 0)
    {
        usleep(100000);
    }

    sem_wait(&state->stateLock);

    cout << "\nPlayers initialized by Arbiter:" << endl;

    for (int i = 0; i < state->playerCount; i++)
    {
        cout << "Player " << i
             << " HP: " << state->players[i].hp
             << " Damage: " << state->players[i].damage
             << " Speed: " << state->players[i].speed
             << " Stamina: " << state->players[i].stamina
             << endl;
    }

    sem_post(&state->stateLock);
    createPlayerThreads(state);

    renderData.window = &window;
    renderData.font = &font;
    renderData.state = state;
    renderData.running = 1;
    renderData.renderPaused = 0;

    pthread_create(&renderThread, NULL, renderThreadFunction, &renderData);

    int lastHandledTurnType = ENTITY_NONE;
    int lastHandledTurnId = -1;

    while (window.isOpen())
    {
        sem_wait(&state->stateLock);

        int currentStatus = state->gameStatus;
        int isPlayerTurn = 0;
        int activePlayerId = -1;

        if (state->currentTurnType == ENTITY_PLAYER)
        {
            isPlayerTurn = 1;
            activePlayerId = state->currentTurnId;
        }

        sem_post(&state->stateLock);

        if (currentStatus != GAME_RUNNING)
        {
            usleep(2000000);
            break;
        }
        sem_wait(&state->stateLock);

        int dropPending = state->dropPending;
        int waitingChoice = (state->dropChoice == -1);
        Weapon dropped = state->pendingDrop;

        sem_post(&state->stateLock);

        // ── Weapon drop decision ─────────────────────────────
        if (dropPending == 1 &&
            waitingChoice == 1)
        {
            pauseRendering();
            window.setActive(true);

            bool answered = false;

            while (window.isOpen() && answered == false)
            {
                sf::Event event;

                while (window.pollEvent(event))
                {
                    if (event.type == sf::Event::Closed)
                    {
                        requestQuit(state);
                        answered = true;
                        break;
                    }

                    if (event.type == sf::Event::KeyPressed)
                    {
                        if (event.key.code == sf::Keyboard::Y)
                        {
                            sem_wait(&state->stateLock);
                            state->dropChoice = 1;
                            sem_post(&state->stateLock);
                            sem_post(&state->dropAnswered); // wake Arbiter immediately

                            answered = true;
                        }
                        else if (event.key.code == sf::Keyboard::N)
                        {
                            sem_wait(&state->stateLock);
                            state->dropChoice = 0;
                            sem_post(&state->stateLock);
                            sem_post(&state->dropAnswered); // wake Arbiter immediately

                            answered = true;
                        }
                    }
                }

                window.clear(sf::Color::Black);

                drawBattleView(window, font, state, nullptr);

                sf::Text text;
                text.setFont(font);
                text.setCharacterSize(22);
                text.setFillColor(sf::Color::Yellow);

                string msg =
                    "WEAPON DROP: " +
                    string(dropped.name) +
                    "\nPress Y to pick up\nPress N to reject";

                text.setString(msg);
                text.setPosition(300, 300);

                window.draw(text);
                window.display();
            }

            window.setActive(false);
            resumeRendering();

            continue;
        }
        if (isPlayerTurn == 1)
        {
            // ── Normal turn input ───────────────────────────────
            if (lastHandledTurnType != ENTITY_PLAYER ||
                lastHandledTurnId != activePlayerId)
            {
                handlePlayerTurnInput(window, font, state, activePlayerId);

                lastHandledTurnType = ENTITY_PLAYER;
                lastHandledTurnId = activePlayerId;
            }
        }
        else
        {
            lastHandledTurnType = ENTITY_NONE;
            lastHandledTurnId = -1;
        }

        usleep(100000);
    }

    renderData.running = 0;
    pthread_join(renderThread, NULL);
    detachSharedMemory(state);

    cout << "HIP finished party selection test." << endl;

    cout << "Press Enter to exit..." << endl;
    std::cin.get();

    return 0;
}