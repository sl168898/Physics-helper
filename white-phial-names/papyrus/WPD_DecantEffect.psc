Scriptname WPD_DecantEffect extends ActiveMagicEffect

WPD_DecantQuest Property Controller Auto

Event OnEffectStart(Actor akTarget, Actor akCaster)
    If akTarget == Game.GetPlayer() && Controller
        Controller.Decant(akTarget)
    EndIf
EndEvent
