# p4 runtime testing

Requires original Flailin Around assets, the supplied Lorerim GildFlail.esp,
Skyrim 1.6.1170, FSMP 3.2.1 dependencies, and the p3 all-flails companion ESP.
The native-only CI artifact is not a complete install package.

Replace p3 with the complete p4 package. Keep the companion after other weapon
overrides and let the new hdtsmp64.dll win over FSMP. Disable older Papyrus/armor
workarounds. Existing saved script instances may require a clean test save.

Test in third person:
- Equip/draw/sheath Iron War Flail, then Steel War Flail.
- Unequip into IED, then re-equip each.
- Sit and leave furniture; save and reload.
- Verify the previously working single/dual one-handed flails still work.
- Where configured in IED, verify displayed flails remain independent of held flails.

No HUD notifications. In hdtSMP64.log, expect prototype p4 and binding messages
with 2 bones for Iron War Flail, 4 for Steel War Flail, and 5 for one-handed rigs.
The shared original XML lists five bones, so FSMP can log skipped absent bones
and constraints for shorter rigs. These warnings are expected; successful
binding to the actual chain is the relevant check.

If physics fails, send hdtSMP64.log and the weapon/equip sequence. A legacy armor
warning means another armor-based driver is active. This build has automated
scanner coverage but has not been tested in Skyrim by the authoring environment.
