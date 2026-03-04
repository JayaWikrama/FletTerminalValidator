#ifndef __SOUND_HELPER_HPP__
#define __SOUND_HELPER_HPP__

class SoundHelper
{
private:
    static bool beepInitialized;

public:
    static void setVolume(int value);
    static void beep(int repeat);
    static void transactionSuccess();
    static void transactionFailed();
};

#endif
