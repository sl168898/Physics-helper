; Compile-only interface. Never install this as a replacement game script.
Scriptname MS12PostQuestScript extends Quest
Potion Property Replicated Auto
MiscObject Property EmptyPhial Auto
FormList Property TWPTE_WhitePhialList Auto
FormList Property TWPTE_SavedPotionList Auto
ReferenceAlias Property PhialAlias Auto
Potion Property CustomPotion Auto
Potion Property CustomPotionPoison Auto
MagicEffect Property MS12WhitePhialEffect Auto
MagicEffect Property MS12WhitePhialEffectPoison Auto
Actor Property TWPTE_WhitePhialActorRef Auto
Function SetReward(String RewardType)
EndFunction
Function RemoveDuplicateWhitePhial(ObjectReference akContainer)
EndFunction
