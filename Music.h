
#pragma once

#include <string>
#include <iostream>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

using namespace std;
using namespace sf;

class MusicManager {
private:
    sf::Music music;
    std::string currentTrack;
    bool isPlaying = false;

public:
    void play(const std::string& filename, bool loop = true, float volume = 100.f) {
        if (currentTrack != filename) {
            music.stop();
            if (music.openFromFile(filename)) {
                currentTrack = filename;
                music.setLoop(loop);
                music.setVolume(volume);
                music.play();
                isPlaying = true;
            }
        }
        else if (!isPlaying) {
            music.play();
            isPlaying = true;
        }
    }

    void pause() {
        music.pause();
        isPlaying = false;
    }

    void stop() {
        music.stop();
        currentTrack = "";
        isPlaying = false;
    }

    void setVolume(float volume) {
        music.setVolume(volume);
    }
};