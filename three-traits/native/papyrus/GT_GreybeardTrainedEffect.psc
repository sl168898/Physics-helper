Scriptname GT_GreybeardTrainedEffect extends ActiveMagicEffect

; Retained properties and variable keep existing script instances compatible.
; Voice of Authority is now a Speech perk. This retired script grants nothing.
Spell Property TraitAbility Auto
Message Property ShoutChoice Auto
GlobalVariable Property ChosenShout Auto
Shout Property AnimalAllegiance Auto
Shout Property FireBreath Auto
Shout Property FrostBreath Auto
Shout Property KynesPeace Auto
Bool choosing = False

Event OnEffectStart(Actor akTarget, Actor akCaster)
EndEvent

Event OnUpdate()
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
    UnregisterForUpdate()
EndEvent
