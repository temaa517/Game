
#pragma once

#include <SFML/Audio.hpp>

class SoundManager {
private:
    static const int MAX_CHANNELS = 16;
    sf::Sound sounds[MAX_CHANNELS];
    sf::SoundBuffer buffers[MAX_CHANNELS];
    bool channelInUse[MAX_CHANNELS] = { false };
    std::string soundFile;
    float soundVolume;

public:
    SoundManager(const std::string& filename, float volume = 40.f)
        : soundFile(filename), soundVolume(volume) {
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