/* ROM browser — no longer used for game launching (partition-switch to retro-go).
 * Kept as stub because it's still compiled. */
#include "rom_browser.h"
#include "app_manager.h"
#include "bsp/key_driver.h"

void rom_browser_enter(void) {
    /* NOP — games are launched via partition switch to ota_0 */
}

void rom_browser_key(key_id_t key, bool pressed) {
    (void)key; (void)pressed;
    /* NOP — games are handled by retro-go on ota_0 */
}

bool rom_browser_has_roms(void) {
    return false;
}
