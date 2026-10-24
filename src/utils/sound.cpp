#include "sound.h"

#include <cstdio>
#include <cstdlib>

#include "config.h"
#include "log.h"
#include "settings.h"

const char *Sound::s_soundData[6] = {
    "letter.wav", // SOUND_LETTER
    "enter.wav",  // SOUND_ENTER
    "back.wav",   // SOUND_BACK
    "recode.wav", // SOUND_RECODE
    "space.wav",  // SOUND_SAPCE
    "empty.wav"   // SOUND_EMPTY
};

void Sound::play(SoundType soundType, SoundScene scene)
{
    const bool enabled =
        (scene == SoundScene::Typing) ? settings::instance().get_typeEffect() : settings::instance().get_uiAudioEffect();
    if (!enabled)
    {
        return;
    }

#if defined(__LINUX__)
    char cmd[128] = {0};
    sprintf(cmd, "aplay %s%s%s > /dev/null 2>&1 &", FREEWB_INSTALL_PKGDATADIR "/sound", "/", s_soundData[soundType]);
    if (system(cmd) == -1)
    {
        FREEWB_ERROR("Failed to play sound: {}", cmd);
    }
#endif
}
