#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <deque>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <string>
#include "Game.h"

using namespace sf;

// Глобальные переменные для размеров окна
int GLOBAL_WIDTH;
int GLOBAL_HEIGHT;
int GLOBAL_GRID_SIZE; // Теперь размер сетки будет вычисляться динамически

const int BONUS_GROW = 3;
const int ANTIBONUS_SHRINK = 3;
const float BONUS_INTERVAL = 30.0f;
const float ANTI_BONUS_INTERVAL = 30.0f; // Добавлено
const float BONUS_DELAY = 30.0f;
const float EASY_SPEED = 0.2f;
const float NORMAL_SPEED = 0.1f;
const float HARD_SPEED = 0.05f;
// Где-то рядом с другими глобальными константами
const std::vector<std::string> DIFFICULTY_OPTIONS = { "Легкий", "Нормальный", "Сложный" };

enum Direction { UP, DOWN, LEFT, RIGHT };
enum GameState { LOGIN, REGISTER, MENU, PLAYING, PAUSED, GAME_OVER, SETTINGS, LEADERBOARD };
enum Difficulty { EASY, NORMAL, HARD };

// Мягкая цветовая палитра
const Color BACKGROUND_COLOR(240, 240, 245);
const Color PRIMARY_COLOR(100, 180, 180);
const Color SECONDARY_COLOR(150, 200, 200);
const Color ACCENT_COLOR(220, 120, 120);
const Color TEXT_COLOR(60, 60, 70);
const Color LIGHT_TEXT_COLOR(240, 240, 245); 
const Color GRID_COLOR(220, 220, 225, 50); 
const Color SNAKE_COLOR(100, 180, 180);
const Color FOOD_COLOR(220, 120, 120);
const Color BONUS_COLOR(240, 200, 80);
const Color ANTIBONUS_COLOR(180, 80, 200); 

struct Settings {
    Difficulty difficulty = NORMAL;
    bool musicEnabled = true;
    bool soundEffectsEnabled = true;  // Новая переменная

    void saveToFile() {
        std::ofstream file("settings.cfg");
        if (file.is_open()) {
            file << static_cast<int>(difficulty) << "\n";
            file << musicEnabled << "\n";
            file << soundEffectsEnabled << "\n";  // Сохраняем состояние звуковых эффектов
            file.close();
        }
    }

    void loadFromFile() {
        std::ifstream file("settings.cfg");
        if (file.is_open()) {
            int diff;
            file >> diff;
            difficulty = static_cast<Difficulty>(diff);
            file >> musicEnabled;
            file >> soundEffectsEnabled;  // Загружаем состояние звуковых эффектов
            file.close();
        }
    }
} settings;

class UserManager {
private:
    std::unordered_map<std::string, std::string> users;
    std::string currentUser;
    const std::string userFile = "users.txt";

public:
    UserManager() {
        loadUsers();
    }

    void loadUsers() {
        std::ifstream file(userFile);
        if (file.is_open()) {
            std::string username, password;
            while (file >> username >> password) {
                users[username] = password;
            }
            file.close();
        }
    }

    void saveUsers() {
        std::ofstream file(userFile);
        if (file.is_open()) {
            for (const auto& [username, password] : users) {
                file << username << " " << password << "\n";
            }
            file.close();
        }
    }

    bool registerUser(const std::string& username, const std::string& password) {
        if (users.find(username) != users.end()) {
            return false; // User already exists
        }
        users[username] = password;
        saveUsers();
        return true;
    }

    bool loginUser(const std::string& username, const std::string& password) {
        auto it = users.find(username);
        if (it != users.end() && it->second == password) {
            currentUser = username;
            return true;
        }
        return false;
    }

    std::string getCurrentUser() const {
        return currentUser;
    }

    void logout() {
        currentUser.clear();
    }
};

class MusicManager {
private:
    Music backgroundMusic;
    Music gameMusic;
    Music gameOverMusic;
    Music settingsMusic;

public:
    void loadMusic() {
        if (!backgroundMusic.openFromFile("assets/sounds/background.ogg") ||
            !gameMusic.openFromFile("assets/sounds/GameMode.ogg") ||
            !gameOverMusic.openFromFile("assets/sounds/GameOver.ogg") ||
            !settingsMusic.openFromFile("assets/sounds/Settings.ogg")) {
            std::cerr << "Failed to load music files!" << std::endl;
        }
        backgroundMusic.setLoop(true);
        gameMusic.setLoop(true);
        gameOverMusic.setLoop(false);
        settingsMusic.setLoop(true);
    }

    bool isPlaying(const std::string& type) const {
        if (type == "menu") return backgroundMusic.getStatus() == Music::Playing;
        if (type == "game") return gameMusic.getStatus() == Music::Playing;
        if (type == "settings") return settingsMusic.getStatus() == Music::Playing;
        if (type == "gameover") return gameOverMusic.getStatus() == Music::Playing;
        return false;
    }

    void play(const std::string& type) {
        if (!settings.musicEnabled) return;

        if ((type == "menu" && backgroundMusic.getStatus() == Music::Playing) ||
            (type == "game" && gameMusic.getStatus() == Music::Playing) ||
            (type == "gameover" && gameOverMusic.getStatus() == Music::Playing) ||
            (type == "settings" && settingsMusic.getStatus() == Music::Playing)) {
            return;
        }

        stop();
        if (type == "menu") {
            backgroundMusic.play();
        }
        else if (type == "game") {
            gameMusic.play();
        }
        else if (type == "settings") {
            settingsMusic.play();
        }
        else if (type == "gameover") {
            gameOverMusic.play();
        }
    }

    void stop() {
        backgroundMusic.stop();
        gameMusic.stop();
        settingsMusic.stop();
        gameOverMusic.stop();
    }

    void toggleMusic() {
        settings.musicEnabled = !settings.musicEnabled;
        if (settings.musicEnabled) {
            // Если включаем, то продолжаем играть текущую фоновую музыку (или начинаем меню, если ничего не играло)
            if (!backgroundMusic.getStatus() == Music::Playing && !gameMusic.getStatus() == Music::Playing && !settingsMusic.getStatus() == Music::Playing) {
                play("menu");
            }
        }
        else {
            stop();
        }
    }
};

class SoundManager {
private:
    static const int MAX_CHANNELS = 16;
    sf::Sound sounds[MAX_CHANNELS];
    sf::SoundBuffer buffers[MAX_CHANNELS];
    bool channelInUse[MAX_CHANNELS] = { false };
    std::string soundFile;
    float soundVolume;

    Sound ButtonClickSound;
    Sound BonusSound;
    Sound AntiBonusSound;

public:
    SoundManager(const std::string& filename, float volume = 40.f)
        : soundFile(filename), soundVolume(volume) {
    }

    void toggleEffects() {
        soundVolume = settings.soundEffectsEnabled ? 40.f : 0.f;
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (channelInUse[i]) {
                sounds[i].setVolume(soundVolume);
            }
        }
    }

    void play(bool loop = false) {
        int freeChannel = -1;
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (!channelInUse[i] || sounds[i].getStatus() == sf::Sound::Stopped) {
                freeChannel = i;
                break;
            }
        }

        if (freeChannel == -1) return;

        if (buffers[freeChannel].loadFromFile(soundFile)) {
            sounds[freeChannel].setBuffer(buffers[freeChannel]);
            sounds[freeChannel].setVolume(soundVolume);
            sounds[freeChannel].setLoop(loop);
            sounds[freeChannel].play();
            channelInUse[freeChannel] = true;
        }
    }

    void stopAll() {
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            sounds[i].stop();
            channelInUse[i] = false;
        }
    }

    void setVolume(float volume) {
        soundVolume = volume;
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (channelInUse[i]) {
                sounds[i].setVolume(volume);
            }
        }
    }
};

class Dropdown {
private:
    std::vector<std::string> options;
    Font font;
    unsigned int characterSize;
    Text mainButtonText;
    RectangleShape mainButtonBackground;
    std::vector<Text> itemTexts;
    std::vector<RectangleShape> itemBackgrounds;
    int selectedIndex = 0;
    bool expanded = false;
    int hoveredItemIndex = -1;

    // Helper to center text within a rect
    void centerText(Text& text, const FloatRect& rect) {
        FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        text.setPosition(rect.left + rect.width / 2.0f, rect.top + rect.height / 2.0f);
    }

public:
    Dropdown(const std::vector<std::string>& options, const Font& font, unsigned int characterSize,
        const Vector2f& position, const Color& buttonColor)
        : options(options), font(font), characterSize(characterSize) {

        // Настройка основной кнопки
        mainButtonText.setFont(font);
        mainButtonText.setString(options[0]);
        mainButtonText.setCharacterSize(characterSize);
        mainButtonText.setFillColor(TEXT_COLOR);

        // Сделаем размер кнопкиDropdown таким же, как у обычной кнопки
        // Высота кнопки должна быть согласована
        float buttonHeight = characterSize * 1.5f;
        FloatRect currentTextRect = mainButtonText.getLocalBounds();
        Vector2f buttonSize = Vector2f(currentTextRect.width + characterSize * 1.5f, buttonHeight);

        mainButtonBackground.setSize(buttonSize);
        mainButtonBackground.setOrigin(buttonSize.x / 2.0f, buttonSize.y / 2.0f);
        mainButtonBackground.setPosition(position);
        mainButtonBackground.setFillColor(buttonColor);
        mainButtonBackground.setOutlineThickness(1);
        mainButtonBackground.setOutlineColor(PRIMARY_COLOR);

        centerText(mainButtonText, mainButtonBackground.getGlobalBounds());


        // Настройка элементов выпадающего списка
        for (size_t i = 0; i < options.size(); ++i) {
            Text item;
            item.setFont(font);
            item.setString(options[i]);
            item.setCharacterSize(characterSize);
            item.setFillColor(TEXT_COLOR);

            RectangleShape itemBg(buttonSize); // Используем тот же размер
            itemBg.setOrigin(buttonSize.x / 2.0f, buttonSize.y / 2.0f);

            // Позиция каждого элемента ниже основной кнопки
            // Учитываем высоту кнопки и интервал между элементами
            float itemYPos = position.y + (i + 1) * buttonSize.y;
            itemBg.setPosition(position.x, itemYPos);
            itemBg.setFillColor(buttonColor);
            itemBg.setOutlineThickness(1);
            itemBg.setOutlineColor(PRIMARY_COLOR);

            centerText(item, itemBg.getGlobalBounds());
            itemTexts.push_back(item);
            itemBackgrounds.push_back(itemBg);
        }
    }

    void draw(RenderWindow& window) {
        window.draw(mainButtonBackground);
        window.draw(mainButtonText);
    }

    // Новая функция для отрисовки раскрытого списка поверх всего
    void drawExpanded(RenderWindow& window) {
        if (expanded) {
            for (size_t i = 0; i < itemBackgrounds.size(); ++i) {
                if (static_cast<int>(i) == hoveredItemIndex) {
                    itemBackgrounds[i].setFillColor(PRIMARY_COLOR); // Цвет при наведении
                    itemTexts[i].setFillColor(LIGHT_TEXT_COLOR);
                }
                else {
                    itemBackgrounds[i].setFillColor(SECONDARY_COLOR); // Обычный цвет
                    itemTexts[i].setFillColor(TEXT_COLOR);
                }
                window.draw(itemBackgrounds[i]);
                window.draw(itemTexts[i]);
            }
        }
    }

    bool handleEvent(const Event& event, const RenderWindow& window) {
        Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));
        hoveredItemIndex = -1; // Сбрасываем при каждом событии

        // Обработка наведения для основной кнопки (когда список не раскрыт)
        if (!expanded) {
            if (mainButtonBackground.getGlobalBounds().contains(mousePos)) {
                mainButtonBackground.setOutlineColor(ACCENT_COLOR); // Цвет обводки при наведении
                mainButtonText.setFillColor(PRIMARY_COLOR);
            }
            else {
                mainButtonBackground.setOutlineColor(PRIMARY_COLOR); // Исходный цвет обводки
                mainButtonText.setFillColor(TEXT_COLOR);
            }
        }
        else { // Если список раскрыт, обрабатываем наведение на элементы списка
            for (size_t i = 0; i < itemBackgrounds.size(); ++i) {
                if (itemBackgrounds[i].getGlobalBounds().contains(mousePos)) {
                    hoveredItemIndex = i;
                    break;
                }
            }
        }


        if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
            if (mainButtonBackground.getGlobalBounds().contains(mousePos)) {
                expanded = !expanded;
                return true; // Событие обработано
            }

            if (expanded) { // Если список был раскрыт
                for (size_t i = 0; i < itemBackgrounds.size(); ++i) {
                    if (itemBackgrounds[i].getGlobalBounds().contains(mousePos)) {
                        selectedIndex = i;
                        mainButtonText.setString(options[i]);
                        centerText(mainButtonText, mainButtonBackground.getGlobalBounds()); // Перецентрируем текст после изменения
                        expanded = false; // Закрываем список после выбора
                        return true; // Событие обработано
                    }
                }
                // Если клик был за пределами элементов списка, когда он раскрыт, закрываем его
                // Проверяем, что клик был не по основной кнопке (мы ее уже обработали)
                if (!mainButtonBackground.getGlobalBounds().contains(mousePos)) {
                    expanded = false;
                    return true; // Считаем, что событие обработано (клик вне списка при его раскрытии)
                }
            }
        }
        return false; // Событие не обработано этим виджетом
    }

    int getSelectedIndex() const { return selectedIndex; }
    bool isExpanded() const { return expanded; }
    void setSelectedIndex(int index) {
        if (index >= 0 && index < static_cast<int>(options.size())) {
            selectedIndex = index;
            mainButtonText.setString(options[index]);
            centerText(mainButtonText, mainButtonBackground.getGlobalBounds()); // Перецентрируем текст после изменения
        }
    }
    // Метод для получения глобальных границ основной кнопки для позиционирования других элементов
    FloatRect getGlobalBounds() const {
        return mainButtonBackground.getGlobalBounds();
    }
};

class Button {
private:
    Text buttonText;
    RectangleShape background;
    bool isHovered = false;
    Clock clickCooldown; // Можно убрать, если не используется
    unsigned int charSize; // Добавлено для хранения размера символов
    std::string currentString; // Добавлено для хранения текущей строки

    // Helper to center text within a rect
    void centerText(Text& text, const FloatRect& rect) {
        FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        text.setPosition(rect.left + rect.width / 2.0f, rect.top + rect.height / 2.0f);
    }

public:
    Button(const std::string& text, const Font& font, unsigned int characterSize,
        const Vector2f& position, const Color& color) : charSize(characterSize), currentString(text) { // Инициализация

        buttonText.setFont(font);
        buttonText.setString(text);
        buttonText.setCharacterSize(characterSize);
        buttonText.setFillColor(TEXT_COLOR); // Исходный цвет текста

        FloatRect textRect = buttonText.getLocalBounds();
        // Устанавливаем высоту кнопки, равную 1.5 от размера символа, для согласованности
        float buttonHeight = characterSize * 1.5f;
        background.setSize(Vector2f(textRect.width + characterSize * 1.5f, buttonHeight));
        background.setOrigin(background.getSize().x / 2.0f, background.getSize().y / 2.0f);
        background.setPosition(position);
        background.setFillColor(color); // Исходный цвет фона
        background.setOutlineThickness(1);
        background.setOutlineColor(PRIMARY_COLOR); // Исходный цвет обводки

        centerText(buttonText, background.getGlobalBounds());
    }

    void draw(RenderWindow& window) {
        isMouseOver(window); // Обновляем состояние наведения
        setHighlight(isHovered); // Применяем эффект наведения
        window.draw(background);
        window.draw(buttonText);
    }

    bool isMouseOver(const RenderWindow& window) {
        Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));
        isHovered = background.getGlobalBounds().contains(mousePos);
        return isHovered;
    }

    bool handleClick(const RenderWindow& window, const Event& event, SoundManager& sfx) {
        if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
            sfx.play();
            Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));
    
            if (background.getGlobalBounds().contains(mousePos)) {
                return true;
            }
        }
        return false;
    }

    void setHighlight(bool highlight) {
        if (highlight) {
            background.setOutlineColor(ACCENT_COLOR);
            buttonText.setFillColor(PRIMARY_COLOR);
        }
        else {
            background.setOutlineColor(PRIMARY_COLOR);
            buttonText.setFillColor(TEXT_COLOR);
        }
    }

    void setTextColor(const Color& color) { // Если потребуется изменить цвет текста извне
        buttonText.setFillColor(color);
    }

    void setString(const std::string& text) {
        buttonText.setString(text);
        currentString = text; // Обновляем сохраненную строку
        // Пересчет размеров фона при изменении текста, сохраняя текущую позицию
        FloatRect textRect = buttonText.getLocalBounds();
        float buttonHeight = buttonText.getCharacterSize() * 1.5f;
        background.setSize(Vector2f(textRect.width + buttonText.getCharacterSize() * 1.5f, buttonHeight));
        background.setOrigin(background.getSize().x / 2.0f, background.getSize().y / 2.0f);
        centerText(buttonText, background.getGlobalBounds()); // Перецентрируем текст
    }

    // Добавленные методы
    unsigned int getCharacterSize() const {
        return charSize;
    }

    std::string getString() const {
        return currentString;
    }

    // Добавленный метод для получения границ (для позиционирования других элементов)
    FloatRect getGlobalBounds() const {
        return background.getGlobalBounds();
    }
};

class TextBox {
private:
    RectangleShape box;
    Text text;
    std::string input;
    bool isActive = false;
    bool isPassword = false;
    int maxLength;
    unsigned int characterSize;

    // Helper to center text vertically within the box
    void centerTextVertically() {
        FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left, textRect.top + textRect.height / 2.0f);
        text.setPosition(box.getPosition().x - box.getSize().x / 2.0f + 10, box.getPosition().y);
    }

public:
    TextBox(const Font& font, unsigned int characterSize, const Vector2f& position,
        int maxLength = 20, bool isPassword = false) : maxLength(maxLength), isPassword(isPassword), characterSize(characterSize) {
        box.setSize(Vector2f(GLOBAL_WIDTH * 0.3f, characterSize * 1.5f)); // Consist with button height
        box.setOrigin(box.getSize().x / 2.0f, box.getSize().y / 2.0f);
        box.setPosition(position);
        box.setFillColor(LIGHT_TEXT_COLOR);
        box.setOutlineThickness(1);
        box.setOutlineColor(PRIMARY_COLOR);

        text.setFont(font);
        text.setCharacterSize(characterSize);
        text.setFillColor(TEXT_COLOR);
        centerTextVertically(); // Initial centering
    }

    void handleEvent(const Event& event, const RenderWindow& window) {
        if (event.type == Event::MouseButtonPressed) {
            Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));
            FloatRect bounds = box.getGlobalBounds();
            isActive = bounds.contains(mousePos);
            box.setOutlineColor(isActive ? PRIMARY_COLOR : SECONDARY_COLOR);
        }

        if (isActive && event.type == Event::TextEntered) {
            if (event.text.unicode == '\b') {
                if (!input.empty()) {
                    input.pop_back();
                }
            }
            else if (event.text.unicode < 128 && input.size() < maxLength) {
                if (event.text.unicode >= 32) {
                    input += static_cast<char>(event.text.unicode);
                }
            }
            text.setString(input);
            centerTextVertically(); // Re-center after text change
        }
    }

    void draw(RenderWindow& window) {
        window.draw(box);

        Text displayText = text;
        if (isPassword && !input.empty()) {
            displayText.setString(std::string(input.size(), '*'));
        }
        else {
            displayText.setString(input);
        }
        window.draw(displayText);
    }

    std::string getText() const { return input; }
    void clear() { input.clear(); }
    bool getActive() const { return isActive; }
    void setActive(bool active) {
        isActive = active;
        box.setOutlineColor(isActive ? PRIMARY_COLOR : SECONDARY_COLOR);
    }
};

class Snake {
private:
    std::deque<Vector2i> body;
    Direction direction;
    Vector2i food;
    Vector2i bonus;
    float currentSpeed = NORMAL_SPEED;
    Vector2i antiBonus;
    bool bonusActive = false;
    bool antiBonusActive = false;
    Clock bonusClock;
    Clock antiBonusClock;
    Clock gameClock;
    bool gameStarted = false;
    Font scoreFont;

    void spawnFood() {
        food = getRandomPosition();
    }

    void spawnBonus() {
        bonus = getRandomPosition();
        bonusActive = true;
        bonusClock.restart();
    }

    void spawnAntiBonus() {
        antiBonus = getRandomPosition();
        antiBonusActive = true;
        antiBonusClock.restart();
    }

    Vector2i getRandomPosition() {
        Vector2i position;
        bool validPosition;

        do {
            position = { (rand() % (GLOBAL_WIDTH / GLOBAL_GRID_SIZE)) * GLOBAL_GRID_SIZE,
                         (rand() % (GLOBAL_HEIGHT / GLOBAL_GRID_SIZE)) * GLOBAL_GRID_SIZE };

            validPosition = true;
            for (const auto& segment : body) {
                if (segment == position) {
                    validPosition = false;
                    break;
                }
            }
            if (position == food || (bonusActive && position == bonus) || (antiBonusActive && position == antiBonus)) {
                validPosition = false;
            }
        } while (!validPosition);

        return position;
    }

    bool checkCollision(const Vector2i& head) {
        if (head.x < 0 || head.x >= GLOBAL_WIDTH || head.y < 0 || head.y >= GLOBAL_HEIGHT)
            return true;

        for (auto it = body.begin() + 1; it != body.end(); ++it) {
            if (*it == head)
                return true;
        }
        return false;
    }

    void grow(int size) {
        for (int i = 0; i < size; ++i) {
            body.push_back(body.back());
        }
    }

    void shrink(int size) {
        for (int i = 0; i < size && body.size() > 3; ++i) {
            body.pop_back();
        }
    }

    void drawObject(RenderWindow& window, Vector2i position, Color color) {
        RectangleShape shape(Vector2f(static_cast<float>(GLOBAL_GRID_SIZE - 1), static_cast<float>(GLOBAL_GRID_SIZE - 1)));
        shape.setPosition(static_cast<float>(position.x) + 0.5f, static_cast<float>(position.y) + 0.5f);
        shape.setFillColor(color);
        window.draw(shape);
    }

    void drawGrid(RenderWindow& window) {
        for (int x = 0; x < GLOBAL_WIDTH; x += GLOBAL_GRID_SIZE) {
            // Создаём sf::Vector2f явно перед передачей в sf::Vertex
            Vector2f startPoint(static_cast<float>(x), 0.f);
            Vector2f endPoint(static_cast<float>(x), static_cast<float>(GLOBAL_HEIGHT));

            Vertex line[] = {
                Vertex(startPoint, GRID_COLOR),
                Vertex(endPoint, GRID_COLOR)
            };
            window.draw(line, 2, Lines);
        }
        for (int y = 0; y < GLOBAL_HEIGHT; y += GLOBAL_GRID_SIZE) {
            // Создаём sf::Vector2f явно перед передачей в sf::Vertex
            Vector2f startPoint(0.f, static_cast<float>(y));
            Vector2f endPoint(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(y));

            Vertex line[] = {
                Vertex(startPoint, GRID_COLOR),
                Vertex(endPoint, GRID_COLOR)
            };
            window.draw(line, 2, Lines);
        }
    }

    void drawScore(RenderWindow& window) {
        if (!scoreFont.loadFromFile("font1.ttf")) {
            std::cerr << "Error loading font for score!" << std::endl;
            return;
        }
        Text scoreText("Счет: " + std::to_string(score), scoreFont, GLOBAL_HEIGHT / 35);
        scoreText.setFillColor(LIGHT_TEXT_COLOR);
        scoreText.setPosition(GLOBAL_WIDTH * 0.01f, GLOBAL_HEIGHT * 0.01f);
        window.draw(scoreText);
    }

public:
    bool gameOver = false;
    int score = 0;
    float getSpeed() const { return currentSpeed; }

    Snake() {
        // Инициализация направления
        direction = RIGHT;

        // Инициализация флагов бонусов
        bonusActive = false;
        antiBonusActive = false;

        // Инициализация состояния игры
        gameOver = false;
        gameStarted = false;
        score = 0;

        // Инициализация скорости
        updateSpeed();

        // Инициализация часов
        bonusClock.restart();
        antiBonusClock.restart();
        gameClock.restart();

        // Загрузка шрифта для счета
        if (!scoreFont.loadFromFile("font1.ttf")) {
            std::cerr << "Error loading font for score!" << std::endl;
        }

        // Инициализация змейки
        reset();

        // Спавн первой еды
        spawnFood();
    }

    void updateSpeed() {
        switch (settings.difficulty) {
        case EASY: currentSpeed = EASY_SPEED; break;
        case NORMAL: currentSpeed = NORMAL_SPEED; break;
        case HARD: currentSpeed = HARD_SPEED; break;
        }
    }

    void reset() {
        body.clear();
        // Всегда добавляем минимум 3 сегмента
        int centerX = (GLOBAL_WIDTH / 2 / GLOBAL_GRID_SIZE) * GLOBAL_GRID_SIZE;
        int centerY = (GLOBAL_HEIGHT / 2 / GLOBAL_GRID_SIZE) * GLOBAL_GRID_SIZE;

        body.push_back({ centerX, centerY });
        body.push_back({ centerX - GLOBAL_GRID_SIZE, centerY });
        body.push_back({ centerX - 2 * GLOBAL_GRID_SIZE, centerY });

        direction = RIGHT;
        spawnFood();
        bonusClock.restart();
        antiBonusClock.restart();
        gameClock.restart();
        bonusActive = false;
        antiBonusActive = false;
        gameOver = false;
        score = 0;
        gameStarted = false;
        updateSpeed();
    }

    void move(SoundManager& bonusSfx, SoundManager& antiBonusSfx, SoundManager& appleSfx) {
        if (gameOver || body.empty()) return; // Защита от пустого тела

        if (!gameStarted) {
            gameStarted = true;
            gameClock.restart();
        }

        Vector2i head = body.front();
        switch (direction) {
        case UP:head.y -= GLOBAL_GRID_SIZE; break;
        case DOWN:head.y += GLOBAL_GRID_SIZE; break;
        case LEFT:head.x -= GLOBAL_GRID_SIZE; break;
        case RIGHT: head.x += GLOBAL_GRID_SIZE; break;
        }

        if (checkCollision(head)) {
            gameOver = true;
            return;
        }

        body.push_front(head);

        if (head == food) {
            spawnFood();
            score += 1;
            appleSfx.play();
        }
        else if (bonusActive && head == bonus) {
            grow(BONUS_GROW);
            bonusActive = false;
            bonusClock.restart();
            score += 3;
            bonusSfx.play();
        }
        else if (antiBonusActive && head == antiBonus) {
            shrink(ANTIBONUS_SHRINK);
            antiBonusActive = false;
            antiBonusClock.restart();
            score = std::max(0, score - 3);
            antiBonusSfx.play();
        }
        else {
            body.pop_back();
        }

        float elapsedTime = gameClock.getElapsedTime().asSeconds();
        if (elapsedTime > BONUS_DELAY) {
            if (!bonusActive && bonusClock.getElapsedTime().asSeconds() > BONUS_INTERVAL) {
                spawnBonus();
            }
            if (!antiBonusActive && antiBonusClock.getElapsedTime().asSeconds() > ANTI_BONUS_INTERVAL) {
                spawnAntiBonus();
            }
        }
    }

    void changeDirection(Direction newDirection) {
        if (body.empty()) return; // Защита от пустого тела

        if ((direction == UP && newDirection != DOWN) ||
            (direction == DOWN && newDirection != UP) ||
            (direction == LEFT && newDirection != RIGHT) ||
            (direction == RIGHT && newDirection != LEFT)) {
            direction = newDirection;
        }
    }

    void draw(RenderWindow& window, const Sprite& background) {
        window.draw(background);
        drawGrid(window);

        // Проверяем, что тело не пустое
        if (body.empty()) return;

        for (auto& segment : body) {
            RectangleShape rect(Vector2f(static_cast<float>(GLOBAL_GRID_SIZE - 1),
                static_cast<float>(GLOBAL_GRID_SIZE - 1)));
            rect.setPosition(static_cast<float>(segment.x) + 0.5f,
                static_cast<float>(segment.y) + 0.5f);
            rect.setFillColor(SNAKE_COLOR);
            window.draw(rect);
        }

        drawObject(window, food, FOOD_COLOR);
        if (bonusActive) drawObject(window, bonus, BONUS_COLOR);
        if (antiBonusActive) drawObject(window, antiBonus, ANTIBONUS_COLOR);

        drawScore(window);
    }

    void drawGameOver(RenderWindow& window, Font& font, const Sprite& background,
        Button& restartButton, Button& menuButton) {
        window.clear();
        window.draw(background);

        RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
        overlay.setFillColor(Color(0, 0, 0, 150));
        window.draw(overlay);

        Text text("Игра Окончена", font, GLOBAL_HEIGHT / 10);
        text.setFillColor(ACCENT_COLOR);
        FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        text.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.3f);
        window.draw(text);

        Text finalScoreText("Результат: " + std::to_string(score), font, GLOBAL_HEIGHT / 25);
        FloatRect scoreRect = finalScoreText.getLocalBounds();
        finalScoreText.setOrigin(scoreRect.left + scoreRect.width / 2.0f, scoreRect.top + scoreRect.height / 2.0f);
        finalScoreText.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.45f);
        finalScoreText.setFillColor(PRIMARY_COLOR);
        window.draw(finalScoreText);

        restartButton.isMouseOver(window);
        menuButton.isMouseOver(window);

        restartButton.draw(window);
        menuButton.draw(window);
    }

};

struct ScoreEntry {
    std::string name;
    int score;
};

class Leaderboard {
private:
    std::vector<ScoreEntry> scores;
    Font font;

    void loadScores() {
        std::ifstream file("scores.txt");
        if (file.is_open()) {
            scores.clear();
            std::string name;
            int score;
            while (file >> name >> score) {
                scores.push_back({ name, score });
            }
            file.close();
        }
    }

    void saveScores() {
        std::ofstream file("scores.txt");
        if (file.is_open()) {
            for (const auto& entry : scores) {
                file << entry.name << " " << entry.score << "\n";
            }
            file.close();
        }
    }

public:
    Leaderboard(const Font& font) : font(font) {
        loadScores();
    }

    void addScore(const std::string& name, int score) {
        scores.push_back({ name, score });
        std::sort(scores.begin(), scores.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
            return a.score > b.score;
            });
        saveScores();
    }

    void draw(RenderWindow& window, const Sprite& menuBackground, Button& backButton) {
        window.draw(menuBackground);

        RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
        overlay.setFillColor(Color(0, 0, 0, 180));
        window.draw(overlay);

        Text title("Таблица лидеров", font, GLOBAL_HEIGHT / 10);
        title.setFillColor(PRIMARY_COLOR);
        title.setStyle(Text::Bold);
        FloatRect titleRect = title.getLocalBounds();
        title.setOrigin(titleRect.left + titleRect.width / 2.0f,
            titleRect.top + titleRect.height / 2.0f);
        title.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.15f);
        window.draw(title);

        for (size_t i = 0; i < std::min(scores.size(), static_cast<size_t>(10)); ++i) {
            Color textColor = (i < 3) ? PRIMARY_COLOR : TEXT_COLOR;
            unsigned int textSize = (i < 3) ? (GLOBAL_HEIGHT / 25) : (GLOBAL_HEIGHT / 30);
            float yPos = static_cast<float>(GLOBAL_HEIGHT) * 0.3f + i * (static_cast<float>(GLOBAL_HEIGHT) / 18);

            Text rankText(std::to_string(i + 1) + ".", font, textSize);
            rankText.setFillColor(textColor);
            if (i < 3) rankText.setStyle(Text::Bold);
            rankText.setPosition(GLOBAL_WIDTH * 0.3f, yPos);
            window.draw(rankText);

            Text nameText(scores[i].name, font, textSize);
            nameText.setFillColor(textColor);
            if (i < 3) nameText.setStyle(Text::Bold);
            nameText.setPosition(GLOBAL_WIDTH * 0.4f, yPos);
            window.draw(nameText);

            Text scoreText(std::to_string(scores[i].score), font, textSize);
            scoreText.setFillColor(textColor);
            if (i < 3) scoreText.setStyle(Text::Bold);
            FloatRect scoreTextRect = scoreText.getLocalBounds();
            scoreText.setOrigin(scoreTextRect.left + scoreTextRect.width, scoreTextRect.top);
            scoreText.setPosition(GLOBAL_WIDTH * 0.7f, yPos);
            window.draw(scoreText);
        }

        backButton.isMouseOver(window);
        backButton.draw(window);
    }
};

void drawLoginScreen(RenderWindow& window, Font& font, const Sprite& background,
    UserManager& userManager, TextBox& usernameBox, TextBox& passwordBox,
    Text& errorText, Button& loginButton, Button& registerButton) {
    window.clear();
    window.draw(background);

    RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
    overlay.setFillColor(Color(0, 0, 0, 150));
    window.draw(overlay);

    Text title("ЗМЕЙКА", font, GLOBAL_HEIGHT / 10);
    title.setFillColor(PRIMARY_COLOR);
    FloatRect titleRect = title.getLocalBounds();
    title.setOrigin(titleRect.left + titleRect.width / 2.0f, titleRect.top + titleRect.height / 2.0f);
    title.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.12f);
    window.draw(title);

    // Username Label and Box
    Text usernameLabel("Имя:", font, GLOBAL_HEIGHT / 30);
    usernameLabel.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect userLabelRect = usernameLabel.getLocalBounds();
    usernameLabel.setOrigin(userLabelRect.left + userLabelRect.width / 2.0f, userLabelRect.top + userLabelRect.height / 2.0f);
    usernameLabel.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.3f);
    window.draw(usernameLabel);
    usernameBox.draw(window);

    // Password Label and Box
    Text passwordLabel("пароль:", font, GLOBAL_HEIGHT / 30);
    passwordLabel.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect passLabelRect = passwordLabel.getLocalBounds();
    passwordLabel.setOrigin(passLabelRect.left + passLabelRect.width / 2.0f, passLabelRect.top + passLabelRect.height / 2.0f);
    passwordLabel.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.45f);
    window.draw(passwordLabel);
    passwordBox.draw(window);

    // Error Text
    errorText.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.58f);
    window.draw(errorText);

    // Buttons
    loginButton.isMouseOver(window);
    registerButton.isMouseOver(window);
    loginButton.draw(window);
    registerButton.draw(window);
}

void drawRegisterScreen(RenderWindow& window, Font& font, const Sprite& background,
    UserManager& userManager, TextBox& usernameBox, TextBox& passwordBox,
    TextBox& confirmBox, Text& errorText, Button& registerButton, Button& backButton) {
    window.clear();
    window.draw(background);

    RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
    overlay.setFillColor(Color(0, 0, 0, 150));
    window.draw(overlay);

    Text title("Регистрация", font, GLOBAL_HEIGHT / 10);
    title.setFillColor(PRIMARY_COLOR);
    FloatRect titleRect = title.getLocalBounds();
    title.setOrigin(titleRect.left + titleRect.width / 2.0f, titleRect.top + titleRect.height / 2.0f);
    title.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.12f);
    window.draw(title);

    // Username Label and Box
    Text usernameLabel("Имя:", font, GLOBAL_HEIGHT / 30);
    usernameLabel.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect userLabelRect = usernameLabel.getLocalBounds();
    usernameLabel.setOrigin(userLabelRect.left + userLabelRect.width / 2.0f, userLabelRect.top + userLabelRect.height / 2.0f);
    usernameLabel.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.27f);
    window.draw(usernameLabel);
    usernameBox.draw(window);

    // Password Label and Box
    Text passwordLabel("Пароль:", font, GLOBAL_HEIGHT / 30);
    passwordLabel.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect passLabelRect = passwordLabel.getLocalBounds();
    passwordLabel.setOrigin(passLabelRect.left + passLabelRect.width / 2.0f, passLabelRect.top + passLabelRect.height / 2.0f);
    passwordLabel.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.42f);
    window.draw(passwordLabel);
    passwordBox.draw(window);

    // Confirm Password Label and Box
    Text confirmLabel("Подтвердить пароль:", font, GLOBAL_HEIGHT / 30);
    confirmLabel.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect confirmLabelRect = confirmLabel.getLocalBounds();
    confirmLabel.setOrigin(confirmLabelRect.left + confirmLabelRect.width / 2.0f, confirmLabelRect.top + confirmLabelRect.height / 2.0f);
    confirmLabel.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.57f);
    window.draw(confirmLabel);
    confirmBox.draw(window);

    // Error Text
    errorText.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.7f);
    window.draw(errorText);

    // Buttons
    registerButton.isMouseOver(window);
    backButton.isMouseOver(window);
    registerButton.draw(window);
    backButton.draw(window);
}

void drawMainMenu(RenderWindow& window, Font& font, const Sprite& background,
    UserManager& userManager, Button& playButton, Button& settingsButton,
    Button& leaderboardButton, Button& exitToDesktopButton) {
    window.clear();
    window.draw(background);

    RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
    overlay.setFillColor(Color(0, 0, 0, 100));
    window.draw(overlay);

    Text title("ЗМЕЙКА", font, GLOBAL_HEIGHT / 10);
    title.setFillColor(PRIMARY_COLOR);
    FloatRect titleRect = title.getLocalBounds();
    title.setOrigin(titleRect.left + titleRect.width / 2.0f, titleRect.top + titleRect.height / 2.0f);
    title.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.12f);
    window.draw(title);

    Text welcomeText("Добро пожаловать, " + userManager.getCurrentUser() + "!", font, GLOBAL_HEIGHT / 25);
    welcomeText.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect welcomeRect = welcomeText.getLocalBounds();
    welcomeText.setOrigin(welcomeRect.left + welcomeRect.width / 2.0f, welcomeRect.top + welcomeRect.height / 2.0f);
    welcomeText.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.25f);
    window.draw(welcomeText);

    // Buttons
    playButton.isMouseOver(window);
    settingsButton.isMouseOver(window);
    leaderboardButton.isMouseOver(window);
    exitToDesktopButton.isMouseOver(window);

    playButton.draw(window);
    settingsButton.draw(window);
    leaderboardButton.draw(window);
    exitToDesktopButton.draw(window);
}

// Изменена сигнатура функции
void drawSettingsScreen(RenderWindow& window, Font& font, const Sprite& background,
    Dropdown& difficultyDropdown, Button& toggleMusicButton, Button& toggleEffectsButton,
    Button& saveButton, Button& backButton, bool fromPauseMenu) {

    window.clear();
    window.draw(background);

    RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
    overlay.setFillColor(Color(0, 0, 0, 150));
    window.draw(overlay);

    Text title("НАСТРОЙКИ", font, GLOBAL_HEIGHT / 10);
    title.setFillColor(PRIMARY_COLOR);
    FloatRect titleRect = title.getLocalBounds();
    title.setOrigin(titleRect.left + titleRect.width / 2.0f, titleRect.top + titleRect.height / 2.0f);
    title.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.12f);
    window.draw(title);

    // Difficulty Label
    Text difficultyLabel("Сложность:", font, GLOBAL_HEIGHT / 25);
    difficultyLabel.setFillColor(LIGHT_TEXT_COLOR);
    FloatRect diffLabelRect = difficultyLabel.getLocalBounds();
    difficultyLabel.setOrigin(diffLabelRect.left + diffLabelRect.width / 2.0f, diffLabelRect.top + diffLabelRect.height / 2.0f);
    difficultyLabel.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, difficultyDropdown.getGlobalBounds().top - GLOBAL_HEIGHT * 0.05f);
    window.draw(difficultyLabel);

    // Отрисовка dropdown (без создания нового)
    difficultyDropdown.draw(window);

    // Остальные элементы UI...
    unsigned int buttonFontSize = GLOBAL_HEIGHT / 25;
    float verticalSpacing = GLOBAL_HEIGHT * 0.08f;
    float startY = difficultyDropdown.getGlobalBounds().top + difficultyDropdown.getGlobalBounds().height + verticalSpacing;

    // Music Button
    toggleMusicButton = Button(settings.musicEnabled ? "Музыка: ВКЛ" : "Музыка: ВЫКЛ",
        font, buttonFontSize,
        Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, startY),
        SECONDARY_COLOR);
    toggleMusicButton.isMouseOver(window);
    toggleMusicButton.draw(window);

    // Sound Effects Button
    toggleEffectsButton = Button(settings.soundEffectsEnabled ? "Звуковые эффекты: ВКЛ" : "Звуковые эффекты: ВЫКЛ",
        font, buttonFontSize,
        Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, startY + verticalSpacing),
        SECONDARY_COLOR);
    toggleEffectsButton.isMouseOver(window);
    toggleEffectsButton.draw(window);

    // Save Button
    saveButton = Button("Сохранить настройки", font, buttonFontSize,
        Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, startY + verticalSpacing * 2),
        PRIMARY_COLOR);
    saveButton.isMouseOver(window);
    saveButton.draw(window);

    // Back Button
    backButton = Button(fromPauseMenu ? "Вернуться в паузу" : "Вернуться в меню",
        font, buttonFontSize,
        Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, startY + verticalSpacing * 3),
        SECONDARY_COLOR);
    backButton.isMouseOver(window);
    backButton.draw(window);

    // Отрисовка раскрытого списка поверх всего
    difficultyDropdown.drawExpanded(window);
}

void drawPauseScreen(RenderWindow& window, Font& font, const Sprite& background,
    Button& resumeButton, Button& settingsPauseButton, Button& menuButton) {
    window.clear();
    window.draw(background);

    RectangleShape overlay(Vector2f(static_cast<float>(GLOBAL_WIDTH), static_cast<float>(GLOBAL_HEIGHT)));
    overlay.setFillColor(Color(0, 0, 0, 150));
    window.draw(overlay);

    Text pausedText("ПАУЗА", font, GLOBAL_HEIGHT / 10);
    pausedText.setFillColor(PRIMARY_COLOR);
    FloatRect textRect = pausedText.getLocalBounds();
    pausedText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    pausedText.setPosition(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.25f);
    window.draw(pausedText);

    resumeButton.isMouseOver(window);
    settingsPauseButton.isMouseOver(window);
    menuButton.isMouseOver(window);

    resumeButton.draw(window);
    settingsPauseButton.draw(window); // Теперь эта кнопка будет между resume и menu
    menuButton.draw(window);
}

int main() {
    setlocale(LC_ALL, "Rus");
    SoundManager clickSfx("assets/sounds/buttonClick.wav", 70.f);
    SoundManager appleSfx("assets/sounds/appleSound.wav", 70.f);
    SoundManager bonusSfx("assets/sounds/bonus.wav", 70.f);
    SoundManager antiBonusSfx("assets/sounds/antiBonus.wav", 100.f);
    settings.loadFromFile();
    std::srand(static_cast<unsigned int>(std::time(NULL)));

    VideoMode desktopMode = VideoMode::getDesktopMode();
    GLOBAL_WIDTH = desktopMode.width;
    GLOBAL_HEIGHT = desktopMode.height;
    GLOBAL_GRID_SIZE = std::min(GLOBAL_WIDTH, GLOBAL_HEIGHT) / 40;

    RenderWindow window(VideoMode(GLOBAL_WIDTH, GLOBAL_HEIGHT), L"Игра Змейка", Style::Fullscreen);
    window.setFramerateLimit(60);

    Font font;
    if (!font.loadFromFile("font1.ttf")) {
        std::cerr << "Error loading font! Make sure 'font1.ttf' is in the correct directory." << std::endl;
        return -1;
    }

    Texture menuTexture;
    if (!menuTexture.loadFromFile("assets/images/main_menu_background.png")) {
        std::cerr << "Error loading menu background image! Make sure 'main_menu_background.png' is in 'assets/images/'." << std::endl;
        return -1;
    }
    Sprite menuBackground(menuTexture);
    menuBackground.setScale(
        static_cast<float>(GLOBAL_WIDTH) / menuTexture.getSize().x,
        static_cast<float>(GLOBAL_HEIGHT) / menuTexture.getSize().y
    );

    Texture gameTexture;
    if (!gameTexture.loadFromFile("assets/images/game_background.png")) {
        std::cerr << "Error loading game background image! Make sure 'game_background.png' is in 'assets/images/'." << std::endl;
        return -1;
    }
    Sprite gameBackground(gameTexture);
    gameBackground.setScale(
        static_cast<float>(GLOBAL_WIDTH) / gameTexture.getSize().x,
        static_cast<float>(GLOBAL_HEIGHT) / gameTexture.getSize().y
    );

    UserManager userManager;
    MusicManager musicManager;
    musicManager.loadMusic();
    musicManager.play("menu");

    Snake snake;
    Leaderboard leaderboard(font);

    GameState currentGameState = LOGIN;
    GameState previousGameState = MENU; // Добавлена переменная для отслеживания предыдущего состояния

    unsigned int titleFontSize = GLOBAL_HEIGHT / 10;
    unsigned int labelFontSize = GLOBAL_HEIGHT / 30;
    unsigned int buttonFontSize = GLOBAL_HEIGHT / 20;
    unsigned int textBoxFontSize = GLOBAL_HEIGHT / 30;

    // Login UI elements
    // Возвращаем исходные Y-координаты для отступов
    TextBox usernameLoginBox(font, textBoxFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.38f), 20, false);
    TextBox passwordLoginBox(font, textBoxFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.53f), 20, true);
    Text loginErrorText("", font, labelFontSize * 0.8f);
    loginErrorText.setFillColor(ACCENT_COLOR);
    loginErrorText.setOrigin(loginErrorText.getLocalBounds().width / 2.0f, loginErrorText.getLocalBounds().height / 2.0f);
    Button loginButton("клоунизация", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.68f), SECONDARY_COLOR); // SECONDARY_COLOR для фона
    Button registerButton("Регистрация", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.78f), SECONDARY_COLOR);

    // Register UI elements
    TextBox usernameRegisterBox(font, textBoxFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.35f), 20, false);
    TextBox passwordRegisterBox(font, textBoxFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.5f), 20, true);
    TextBox confirmRegisterBox(font, textBoxFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.65f), 20, true);
    Text registerErrorText("", font, labelFontSize * 0.8f);
    registerErrorText.setFillColor(ACCENT_COLOR);
    registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
    Button registerConfirmButton("Регистрация", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.77f), SECONDARY_COLOR);
    Button backToLoginButton("Вернуться в авторизацию", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.87f), SECONDARY_COLOR);

    // Main Menu UI elements
    Button playButton("Играть", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.40f), SECONDARY_COLOR);
    Button settingsButton("Настройки", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.53f), SECONDARY_COLOR);
    Button leaderboardButton("Таблица Лидеров", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.66f), SECONDARY_COLOR);
    Button exitToDesktopButton("Рабочий стол", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.79f), SECONDARY_COLOR);

    // Settings UI elements
    std::vector<std::string> difficultyOptions = { "лёгкая", "нормальная", "сложная" };
    // Позиция для выпадающего списка (учитываем, что метка будет выше)
    Dropdown difficultyDropdown(DIFFICULTY_OPTIONS, font, GLOBAL_HEIGHT / 30,
        Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.3f),
        SECONDARY_COLOR);
    difficultyDropdown.setSelectedIndex(static_cast<int>(settings.difficulty));
    Button saveButton("Сохранить настрйоки", font, buttonFontSize, Vector2f(0, 0), SECONDARY_COLOR);
    Button toggleEffectsButton_placeholder("Звуковые эффекты: ВКЛ", font, buttonFontSize, Vector2f(0, 0), SECONDARY_COLOR);

    // Инициализация кнопок, позиция будет скорректирована в drawSettingsScreen
    Button toggleMusicButton_placeholder("Музыка: ВКЛ", font, buttonFontSize, Vector2f(0, 0), SECONDARY_COLOR);
    Button backToMenuButton_placeholder("Вернуться в меню", font, buttonFontSize, Vector2f(0, 0), SECONDARY_COLOR);


    // Pause Menu UI elements
    // Изменен порядок и позиция для settingsPauseButton
    Button resumeButton("Продолжить", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.45f), SECONDARY_COLOR);
    Button settingsPauseButton("Настройки", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.58f), SECONDARY_COLOR); // Позиция между resume и menu
    Button pauseMenuButton("Главное Меню", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.71f), SECONDARY_COLOR);

    // Game Over UI elements
    Button gameOverRestartButton("Играть снова", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.6f), SECONDARY_COLOR);
    Button gameOverMenuButton("Главное Меню", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) * 0.75f), SECONDARY_COLOR);

    // Leaderboard UI elements
    Button leaderboardBackButton("Главное Меню", font, buttonFontSize, Vector2f(static_cast<float>(GLOBAL_WIDTH) / 2, static_cast<float>(GLOBAL_HEIGHT) - static_cast<float>(GLOBAL_HEIGHT) * 0.1f), SECONDARY_COLOR);

    Clock gameUpdateClock;
    float snakeSpeed = 0.15f; // Normal difficulty by default

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
            }

            // Обработка событий в зависимости от текущего состояния игры
            if (currentGameState == LOGIN) {
                usernameLoginBox.handleEvent(event, window);
                passwordLoginBox.handleEvent(event, window);

                if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (loginButton.handleClick(window, event, clickSfx)) {
                        if (userManager.loginUser(usernameLoginBox.getText(), passwordLoginBox.getText())) {
                            currentGameState = MENU;
                            loginErrorText.setString("");
                            usernameLoginBox.clear();
                            passwordLoginBox.clear();
                            musicManager.play("menu");
                        }
                        else {
                            loginErrorText.setString("Неверное имя или пароль");
                            loginErrorText.setOrigin(loginErrorText.getLocalBounds().width / 2.0f, loginErrorText.getLocalBounds().height / 2.0f);
                        }
                    }
                    else if (registerButton.handleClick(window, event, clickSfx)) {
                        currentGameState = REGISTER;
                        loginErrorText.setString("");
                        usernameLoginBox.clear();
                        passwordLoginBox.clear();
                        usernameRegisterBox.clear();
                        passwordRegisterBox.clear();
                        confirmRegisterBox.clear();
                        registerErrorText.setString("");
                    }
                }
                if (event.type == Event::KeyPressed && event.key.code == Keyboard::Enter) {
                    if (usernameLoginBox.getActive()) {
                        passwordLoginBox.setActive(true);
                        usernameLoginBox.setActive(false);
                    }
                    else if (passwordLoginBox.getActive()) {
                        if (userManager.loginUser(usernameLoginBox.getText(), passwordLoginBox.getText())) {
                            currentGameState = MENU;
                            loginErrorText.setString("");
                            usernameLoginBox.clear();
                            passwordLoginBox.clear();
                            musicManager.play("menu");
                        }
                        else {
                            loginErrorText.setString("Неверное имя или пароль");
                            loginErrorText.setOrigin(loginErrorText.getLocalBounds().width / 2.0f, loginErrorText.getLocalBounds().height / 2.0f);
                        }
                    }
                }
            }
            else if (currentGameState == REGISTER) {
                usernameRegisterBox.handleEvent(event, window);
                passwordRegisterBox.handleEvent(event, window);
                confirmRegisterBox.handleEvent(event, window);

                if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (registerConfirmButton.handleClick(window, event, clickSfx)) {
                        if (usernameRegisterBox.getText().empty() || passwordRegisterBox.getText().empty() || confirmRegisterBox.getText().empty()) {
                            registerErrorText.setString("Все поля должны быть заполнены!");
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                        }
                        else if (passwordRegisterBox.getText() != confirmRegisterBox.getText()) {
                            registerErrorText.setString("Неверный пароль!");
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                        }
                        else if (userManager.registerUser(usernameRegisterBox.getText(), passwordRegisterBox.getText())) {
                            currentGameState = LOGIN;
                            registerErrorText.setString("Успешная регистрация! Пожалуйста войдите.");
                            registerErrorText.setFillColor(PRIMARY_COLOR);
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                            usernameRegisterBox.clear();
                            passwordRegisterBox.clear();
                            confirmRegisterBox.clear();
                        }
                        else {
                            registerErrorText.setString("Имя уже существует!");
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                        }
                    }
                    else if (backToLoginButton.handleClick(window, event, clickSfx)) {
                        currentGameState = LOGIN;
                        registerErrorText.setString("");
                        usernameRegisterBox.clear();
                        passwordRegisterBox.clear();
                        confirmRegisterBox.clear();
                        loginErrorText.setString("");
                    }
                }
                if (event.type == Event::KeyPressed && event.key.code == Keyboard::Enter) {
                    if (usernameRegisterBox.getActive()) {
                        passwordRegisterBox.setActive(true);
                        usernameRegisterBox.setActive(false);
                    }
                    else if (passwordRegisterBox.getActive()) {
                        confirmRegisterBox.setActive(true);
                        passwordRegisterBox.setActive(false);
                    }
                    else if (confirmRegisterBox.getActive()) {
                        if (usernameRegisterBox.getText().empty() || passwordRegisterBox.getText().empty() || confirmRegisterBox.getText().empty()) {
                            registerErrorText.setString("Все поля должны быть заполнены!");
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                        }
                        else if (passwordRegisterBox.getText() != confirmRegisterBox.getText()) {
                            registerErrorText.setString("Неверный пароль!");
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                        }
                        else if (userManager.registerUser(usernameRegisterBox.getText(), passwordRegisterBox.getText())) {
                            currentGameState = LOGIN;
                            registerErrorText.setString("Успешная регистрация! Пожалуйста войдите.");
                            registerErrorText.setFillColor(PRIMARY_COLOR);
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                            usernameRegisterBox.clear();
                            passwordRegisterBox.clear();
                            confirmRegisterBox.clear();
                        }
                        else {
                            registerErrorText.setString("Имя уже занято!");
                            registerErrorText.setOrigin(registerErrorText.getLocalBounds().width / 2.0f, registerErrorText.getLocalBounds().height / 2.0f);
                        }
                    }
                }
            }
            else if (currentGameState == MENU) {
                if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (playButton.handleClick(window, event, clickSfx)) {
                        currentGameState = PLAYING;
                        musicManager.play("game");
                    }
                    else if (settingsButton.handleClick(window, event, clickSfx)) {
                        // Обновленная часть - переход в настройки
                        previousGameState = MENU;
                        currentGameState = SETTINGS;
                        musicManager.play("settings");
                        // Устанавливаем текущую сложность в dropdown
                        difficultyDropdown.setSelectedIndex(static_cast<int>(settings.difficulty));
                        // Обновляем скорость змейки согласно настройкам
                        snake.updateSpeed();
                    }
                    else if (leaderboardButton.handleClick(window, event, clickSfx)) {
                        currentGameState = LEADERBOARD;
                        musicManager.play("settings"); // Можно использовать ту же музыку, что и для настроек
                    }
                    else if (exitToDesktopButton.handleClick(window, event, clickSfx)) {
                        window.close();
                    }
                }
            }
            else if (currentGameState == PLAYING) {
                if (event.type == Event::KeyPressed) {
                    if (event.key.code == Keyboard::Up) snake.changeDirection(UP);
                    else if (event.key.code == Keyboard::Down) snake.changeDirection(DOWN);
                    else if (event.key.code == Keyboard::Left) snake.changeDirection(LEFT);
                    else if (event.key.code == Keyboard::Right) snake.changeDirection(RIGHT);
                    else if (event.key.code == Keyboard::Escape) {
                        currentGameState = PAUSED;
                        musicManager.stop(); // Остановка музыки при паузе
                    }
                }
            }

            else if (currentGameState == PAUSED) {
                if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (resumeButton.handleClick(window, event, clickSfx)) {
                        currentGameState = PLAYING;
                        musicManager.play("game");
                    }
                    else if (settingsPauseButton.handleClick(window, event, clickSfx)) {
                        // Обновленная часть - переход в настройки из паузы
                        previousGameState = PAUSED;
                        currentGameState = SETTINGS;
                        musicManager.play("settings");
                        // Устанавливаем текущую сложность в dropdown
                        difficultyDropdown.setSelectedIndex(static_cast<int>(settings.difficulty));
                        // Обновляем скорость змейки согласно настройкам
                        snake.updateSpeed();
                    }
                    else if (pauseMenuButton.handleClick(window, event, clickSfx)) {
                        currentGameState = MENU;
                        musicManager.play("menu");
                        snake.reset();
                    }
                }
            }
            else if (currentGameState == GAME_OVER) {
                if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (gameOverRestartButton.handleClick(window, event, clickSfx)) {
                        snake.reset();
                        currentGameState = PLAYING;
                        musicManager.play("game");
                    }
                    else if (gameOverMenuButton.handleClick(window, event, clickSfx)) {
                        snake.reset();
                        currentGameState = MENU;
                        musicManager.play("menu");
                    }
                }
            }
            else if (currentGameState == SETTINGS) {
                // Обработка событий для dropdown
                bool dropdownHandled = difficultyDropdown.handleEvent(event, window);

                if (!dropdownHandled && event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (toggleMusicButton_placeholder.handleClick(window, event, clickSfx)) {
                        musicManager.toggleMusic();
                        toggleMusicButton_placeholder.setString(settings.musicEnabled ? "Музыка: ВКЛ" : "Музыка: ВЫКЛ");
                    }
                    else if (toggleEffectsButton_placeholder.handleClick(window, event, clickSfx)) {
                        settings.soundEffectsEnabled = !settings.soundEffectsEnabled;
                        toggleEffectsButton_placeholder.setString(settings.soundEffectsEnabled ? "Звуковые эффекты: ВКЛ" : "Звуковые эффекты: ВЫКЛ");
                        clickSfx.toggleEffects();
                        appleSfx.toggleEffects();
                        bonusSfx.toggleEffects();
                        antiBonusSfx.toggleEffects();
                    }
                    else if (saveButton.handleClick(window, event, clickSfx)) {
                        // Применяем настройки только после нажатия Save
                        int selected = difficultyDropdown.getSelectedIndex();
                        if (selected == 0) settings.difficulty = EASY;
                        else if (selected == 1) settings.difficulty = NORMAL;
                        else if (selected == 2) settings.difficulty = HARD;

                        snake.updateSpeed();
                        settings.saveToFile();
                    }
                    else if (backToMenuButton_placeholder.handleClick(window, event, clickSfx)) {
                        currentGameState = previousGameState;
                        if (currentGameState == MENU) {
                            musicManager.play("menu");
                        }
                        else if (currentGameState == PAUSED) {
                            // Если вернулись в паузу, музыку не проигрываем
                        }
                    }
                    
                }
            }
            else if (currentGameState == LEADERBOARD) {
                if (event.type == Event::MouseButtonReleased && event.mouseButton.button == Mouse::Left) {
                    if (leaderboardBackButton.handleClick(window, event, clickSfx)) {
                        currentGameState = MENU;
                        musicManager.play("menu");
                    }
                }
            }
           window.clear();
        }
        // Отрисовка в зависимости от текущего состояния игры
        if (currentGameState == LOGIN) {
            drawLoginScreen(window, font, menuBackground, userManager, usernameLoginBox, passwordLoginBox, loginErrorText, loginButton, registerButton);
        }
        else if (currentGameState == REGISTER) {
            drawRegisterScreen(window, font, menuBackground, userManager, usernameRegisterBox, passwordRegisterBox, confirmRegisterBox, registerErrorText, registerConfirmButton, backToLoginButton);
        }
        else if (currentGameState == MENU) {
            drawMainMenu(window, font, menuBackground, userManager, playButton, settingsButton, leaderboardButton, exitToDesktopButton);
        }
        else if (currentGameState == PLAYING) {
            // Установка скорости змейки в зависимости от сложности
            if (settings.difficulty == EASY) snakeSpeed = 0.2f;
            else if (settings.difficulty == NORMAL) snakeSpeed = 0.15f;
            else if (settings.difficulty == HARD) snakeSpeed = 0.1f;

            if (gameUpdateClock.getElapsedTime().asSeconds() >= snake.getSpeed()) {
                snake.move(bonusSfx, antiBonusSfx, appleSfx);
                gameUpdateClock.restart();
            }

            if (snake.gameOver) {
                currentGameState = GAME_OVER;
                leaderboard.addScore(userManager.getCurrentUser(), snake.score);
                musicManager.play("gameover");
            }
            else {
                snake.draw(window, gameBackground);
            }
        }
        else if (currentGameState == PAUSED) {
            drawPauseScreen(window, font, gameBackground, resumeButton, settingsPauseButton, pauseMenuButton);
        }
        else if (currentGameState == GAME_OVER) {
            snake.drawGameOver(window, font, gameBackground, gameOverRestartButton, gameOverMenuButton);
        }
        else if (currentGameState == SETTINGS) {
            drawSettingsScreen(window, font, menuBackground, difficultyDropdown,
                toggleMusicButton_placeholder, toggleEffectsButton_placeholder,
                saveButton, backToMenuButton_placeholder, previousGameState == PAUSED);
        }
        else if (currentGameState == LEADERBOARD) {
            leaderboard.draw(window, menuBackground, leaderboardBackButton);
        }

        window.display();
    }
   return 0;
}