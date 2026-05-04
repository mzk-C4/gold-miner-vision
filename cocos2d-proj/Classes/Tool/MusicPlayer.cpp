#include "MusicPlayer.hpp"
#include "SimpleAudioEngine.h"
#include <algorithm>
#include <random>

USING_NS_CC;
using namespace CocosDenshion;

MusicPlayer* MusicPlayer::s_instance = nullptr;

static const char* BACK_MUSIC = "music/backMusic.mp3";

// JAY mp3 files (converted from flac)
static const char* kJayTracks[] = {
    "music/JAY/周杰伦 - 爱你没差.mp3",
    "music/JAY/周杰伦 - 爱在西元前.mp3",
    "music/JAY/周杰伦 - 大笨钟.mp3",
    "music/JAY/周杰伦 - 稻香.mp3",
    "music/JAY/周杰伦 - 东风破.mp3",
    "music/JAY/周杰伦 - 斗牛.mp3",
    "music/JAY/周杰伦 - 反方向的钟.mp3",
    "music/JAY/周杰伦 - 分裂.mp3",
    "music/JAY/周杰伦 - 给我一首歌的时间.mp3",
    "music/JAY/周杰伦 - 火车叨位去.mp3",
    "music/JAY/周杰伦 - 霍元甲.mp3",
    "music/JAY/周杰伦 - 将军.mp3",
    "music/JAY/周杰伦 - 开不了口.mp3",
    "music/JAY/周杰伦 - 龙卷风.mp3",
    "music/JAY/周杰伦 - 你听得到.mp3",
    "music/JAY/周杰伦 - 逆鳞.mp3",
    "music/JAY/周杰伦 - 牛仔很忙.mp3",
    "music/JAY/周杰伦 - 飘移.mp3",
    "music/JAY/周杰伦 - 晴天.mp3",
    "music/JAY/周杰伦 - 忍者.mp3",
    "music/JAY/周杰伦 - 三年二班.mp3",
    "music/JAY/周杰伦 - 上海一九四三.mp3",
    "music/JAY/周杰伦 - 时光机.mp3",
    "music/JAY/周杰伦 - 手语.mp3",
    "music/JAY/周杰伦 - 双刀.mp3",
    "music/JAY/周杰伦 - 说走就走.mp3",
    "music/JAY/周杰伦 - 外婆.mp3",
};

// David Tao mp3 files
static const char* kDavidTaoTracks[] = {
    "music/David Tao/陶喆 - Angel.mp3",
    "music/David Tao/陶喆 - Melody.mp3",
    "music/David Tao/陶喆 - 爱，很简单.mp3",
    "music/David Tao/陶喆 - 爱我还是他.mp3",
    "music/David Tao/陶喆 - 暗恋.mp3",
    "music/David Tao/陶喆 - 二十二.mp3",
    "music/David Tao/陶喆 - 飞机场的1030.mp3",
    "music/David Tao/陶喆 - 蝴蝶.mp3",
    "music/David Tao/陶喆 - 寂寞的季节.mp3",
    "music/David Tao/陶喆 - 就是爱你.mp3",
    "music/David Tao/陶喆 - 流沙.mp3",
    "music/David Tao/陶喆 - 普通朋友.mp3",
    "music/David Tao/陶喆 - 讨厌红楼梦.mp3",
    "music/David Tao/陶喆 - 望春风.mp3",
    "music/David Tao/陶喆 - 小镇姑娘.mp3",
    "music/David Tao/陶喆 - 找自己.mp3",
};

MusicPlayer* MusicPlayer::getInstance()
{
    if (!s_instance) {
        s_instance = new MusicPlayer();
    }
    return s_instance;
}

MusicPlayer::~MusicPlayer()
{
    if (_scheduler) {
        _scheduler->unschedule("MusicPlayerTick", this);
    }
}

void MusicPlayer::buildPlaylist()
{
    int jayCount = sizeof(kJayTracks) / sizeof(kJayTracks[0]);
    int dtCount  = sizeof(kDavidTaoTracks) / sizeof(kDavidTaoTracks[0]);
    int total = std::max(jayCount, dtCount) * 2;

    _playlist.reserve(total);
    for (int i = 0; i < std::max(jayCount, dtCount); i++) {
        if (i < jayCount)
            _playlist.push_back(kJayTracks[i]);
        if (i < dtCount)
            _playlist.push_back(kDavidTaoTracks[i]);
    }

    // Shuffle to keep it fresh each session
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(_playlist.begin(), _playlist.end(), g);
}

void MusicPlayer::start()
{
    if (_started) return;
    _started = true;

    buildPlaylist();

    auto* audio = SimpleAudioEngine::getInstance();

    // Play the default background music once — NOT looped.
    audio->playBackgroundMusic(BACK_MUSIC, false);
    _playingBackMusic = true;

    // Poll every 0.5s to detect when a track ends.
    _scheduler = Director::getInstance()->getScheduler();
    _scheduler->schedule(
        std::bind(&MusicPlayer::onTick, this, std::placeholders::_1),
        this, 0.5f, CC_REPEAT_FOREVER, 0.0f, false, "MusicPlayerTick");
}

void MusicPlayer::onTick(float /*dt*/)
{
    auto* audio = SimpleAudioEngine::getInstance();

    if (audio->isBackgroundMusicPlaying())
        return; // still playing, wait

    if (_playingBackMusic) {
        // backMusic just finished — start the playlist.
        _playingBackMusic = false;
        _inPlaylist = true;
        _index = 0;
        playNext();
    } else if (_inPlaylist) {
        // Current playlist track finished, go to next.
        _index = (_index + 1) % _playlist.size();
        playNext();
    }
}

void MusicPlayer::playNext()
{
    if (_playlist.empty()) return;

    auto* audio = SimpleAudioEngine::getInstance();
    const auto& path = _playlist[_index];

    audio->playBackgroundMusic(path.c_str(), false);
    CCLOG("[MusicPlayer] Now playing: %s", path.c_str());
}
