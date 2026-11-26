# Level-local wardrobe variation

The first accepted air-station seating (tap or drag) activates a cosmetic rumpled-outfit variant. It persists in the current level and resets on a new load or retry. The variant adds ordinary creases at the outer waist and a subtly irregular waistband; the existing character geometry and clothing coverage are unchanged. The effect follows the front, seated, profile/flight and rotating body render paths, and never leaks into offline art generation or menu art. Special exposed outfits are excluded from the wrinkle overlay. A sealed, flat-topped mechanical housing with a side indicator replaces the exposed station nozzle. No attachment, collision, inflation, input, progress or saved-data changes are introduced.

The regression checks exercise first use, idempotence, both input paths, level persistence, reset and drawing scope. Chromium coverage compares the visible variant on Ultra Low and High across five modes. Native sprite art is unchanged.
