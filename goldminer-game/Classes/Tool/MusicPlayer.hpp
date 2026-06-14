#pragma once
#include "cocos2d.h"

class MusicPlayer
{
public:
    static MusicPlayer* getInstance();

    /// Call once after the first scene loads. Plays the default background
    /// music once (non-looped), then begins alternating Jay/David Tao tracks.
    void start();

    /// Returns true once the initial backMusic has finished and the
    /// playlist has taken over.
    bool isInPlaylist() const { return _inPlaylist; }

private:
    MusicPlayer() = default;
    ~MusicPlayer();

    void buildPlaylist();
    void onTick(float dt);
    void playNext();

    cocos2d::Scheduler* _scheduler = nullptr;
    std::vector<std::string> _playlist;
    size_t _index = 0;
    bool _started = false;
    bool _inPlaylist = false;
    bool _playingBackMusic = false;
    bool _paused = false;

    static MusicPlayer* s_instance;
};
