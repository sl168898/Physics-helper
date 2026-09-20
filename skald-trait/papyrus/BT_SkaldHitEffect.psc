Scriptname BT_SkaldHitEffect extends ActiveMagicEffect
; Compatibility stub for short-lived marks already present in older saves.
; The SKSE helper now owns Skald; no cloak or shout-recovery script remains.
Actor Property PlayerRef Auto
Spell Property TraitAbility Auto
Float Property RecoverySeconds Auto
Spell originalSkald = None

Event OnEffectStart(Actor akTarget, Actor akCaster)
EndEvent

Event OnHit(ObjectReference akAggressor, Form akSource, Projectile akProjectile, Bool abPowerAttack, Bool abSneakAttack, Bool abBashAttack, Bool abHitBlocked)
EndEvent
