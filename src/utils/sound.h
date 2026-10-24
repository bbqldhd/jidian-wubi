#ifndef SOUND_H
#define SOUND_H

enum SoundType
{
    SOUND_LETTER,
    SOUND_ENTER,
    SOUND_BACK,
    SOUND_RECODE,
    SOUND_SAPCE,
    SOUND_EMPTY,
};

/** 播放场景：打字音效 typeEffect；界面音效 uiAudioEffect。 */
enum class SoundScene
{
    Typing,
    Ui,
};

class Sound
{
public:
    static void play(SoundType soundType, SoundScene scene = SoundScene::Ui);

private:
    static const char *s_soundData[6];
};

#endif // SOUND_H
