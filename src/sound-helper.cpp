#include <unistd.h>
#include <cstring>
#include <cstdio>

#include "sound-helper.hpp"
#include "controller.hpp"
#include "reader/include/DataType.h"
#include "reader/include/LinuxDriverError.h"
#include "reader/include/LinuxHardwareDriver.h"

bool SoundHelper::beepInitialized = false;

void SoundHelper::setVolume(int value)
{
    Volune_Set(value);
}

void SoundHelper::beep(int repeat)
{
    if (SoundHelper::beepInitialized == false)
    {
        SoundHelper::beepInitialized = true;
        beep_init();
    }
    bp_beep(repeat);
}

void SoundHelper::transactionSuccess()
{
    wav_pthread_start(SOUND_TSC_SUCCESS);
}

void SoundHelper::transactionFailed()
{
    wav_pthread_start(SOUND_TSC_FAILED);
}
