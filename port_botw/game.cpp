// BOTW (Switch) game target for MIT SwitchGamesRecomp. No Nintendo-owned code.
#include <nx/game.h>
#include <nx/runtime.h>
namespace nx {
const game::Info game::kInfo = {
    "botw", "The Legend of Zelda: Breath of the Wild",
    0x01007EF00011E000ull, "org.example.botwlab", "private/botw",
    0 // NPDM resource size unknown until examining a legitimately obtained copy.
};
void game::after_init() {}
void game::renderer_start() {}
void game::first_frame() {}
} // namespace nx
