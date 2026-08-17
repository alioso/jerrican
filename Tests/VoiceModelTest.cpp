#include <cassert>
#include <iostream>

#include "VoiceModel.h"

int main() {
    VoiceModel voice("Voice 1", true, 0.82f, 0.2f, 0.6f, 0.5f, 0.4f, 0.3f, 0.6f, 7);

    assert(voice.getName() == "Voice 1");
    assert(voice.getRootSemitoneOffset() == 7);
    voice.setRootSemitoneOffset(3);
    assert(voice.getRootSemitoneOffset() == 3);
    assert(voice.isEnabled());
    assert(voice.getVolume() > 0.8f);
    assert(voice.getPitchRangeLow() == 0.2f);
    assert(voice.getPitchRangeHigh() == 0.6f);
    assert(voice.getTimbre() == 0.5f);
    assert(voice.getGroove() == 0.4f);
    assert(voice.getWander() == 0.3f);
    assert(voice.getDissonance() == 0.6f);
    assert(voice.getBusy() == 0.5f);   // default
    assert(voice.getSustain() == 0.5f);  // default
    assert(voice.getCleanliness() == 0.5f);  // default
    assert(voice.getAttack() == 0.5f);  // default

    voice.setEnabled(false);
    voice.setVolume(1.8f);
    voice.setPitchRange(0.9f, 0.1f);  // low > high should be corrected
    voice.setTimbre(-0.2f);
    voice.setGroove(2.0f);
    voice.setWander(-1.0f);
    voice.setDissonance(-1.0f);
    voice.setBusy(2.0f);
    voice.setSustain(-1.0f);
    voice.setCleanliness(2.0f);
    voice.setAttack(-1.0f);

    assert(!voice.isEnabled());
    assert(voice.getVolume() == 1.0f);
    assert(voice.getPitchRangeLow() <= voice.getPitchRangeHigh());
    assert(voice.getTimbre() == 0.0f);
    assert(voice.getGroove() == 1.0f);
    assert(voice.getWander() == 0.0f);
    assert(voice.getDissonance() == 0.0f);
    assert(voice.getBusy() == 1.0f);
    assert(voice.getSustain() == 0.0f);
    assert(voice.getCleanliness() == 1.0f);
    assert(voice.getAttack() == 0.0f);

    std::cout << voice.getSummary() << std::endl;
    return 0;
}
