#include <cassert>
#include <iostream>

#include "VoiceModel.h"

int main() {
    VoiceModel voice("Voice 1", "Drum", true, 0.82f, 0.63f);

    assert(voice.getName() == "Voice 1");
    assert(voice.getInstrument() == "Drum");
    assert(voice.isEnabled());
    assert(voice.getVolume() > 0.8f);
    assert(voice.getPitch() > 0.6f);

    voice.setEnabled(false);
    voice.setVolume(1.8f);
    voice.setPitch(-0.2f);

    assert(!voice.isEnabled());
    assert(voice.getVolume() == 1.0f);
    assert(voice.getPitch() == 0.0f);

    std::cout << voice.getSummary() << std::endl;
    return 0;
}
