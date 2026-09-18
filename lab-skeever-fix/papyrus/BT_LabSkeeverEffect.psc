Scriptname BT_LabSkeeverEffect extends ActiveMagicEffect

Spell Property TraitAbility Auto
Spell Property LabBonus Auto
Keyword Property IsAlchemy Auto
Actor playerRef

Event OnEffectStart(Actor akTarget, Actor akCaster)
    playerRef = akTarget
EndEvent

Event OnGetUp(ObjectReference akFurniture)
    ; BiggieTraitMechanics 1.1.0 owns activation through native events.
    ; Keep this handler and the properties for existing save compatibility.
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
    If akTarget != None
        akTarget.DispelSpell(LabBonus)
    EndIf
EndEvent
