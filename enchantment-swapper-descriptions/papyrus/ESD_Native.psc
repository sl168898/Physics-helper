Scriptname ESD_Native Hidden

; Call immediately after the receiving item has the enchantment, before the
; original donor is stripped/archived. Does not alter enchantments or charge.
Bool Function RememberTransfer(ObjectReference donor, ObjectReference recipient, Enchantment transferredEnchantment) Global Native
