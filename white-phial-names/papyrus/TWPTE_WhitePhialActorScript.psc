Scriptname TWPTE_WhitePhialActorScript extends Actor

Actor Property PlayerRef Auto

GlobalVariable Property TWPTE_Failsafe Auto

Potion Property TheWhitePhialFull Auto
Potion Property TheWhitePhialFullPoison Auto

MagicEffect Property MS12WhitePhialEffect Auto
MagicEffect Property MS12WhitePhialEffectPoison Auto
MagicEffect Property TWPTE_DummyEffect Auto

Message Property TWPTE_ReassignedMessage Auto ; "The liquid evaporates and the Phial starts refilling."

String[] ConditionListDummy

Quest Property MS12PostQuest Auto

ReferenceAlias Property WhitePhialPotion Auto

MiscObject Property MS12WhitePhial Auto

GlobalVariable Property TWPTE_ResetHours Auto

Potion Property InitialLastPotion Auto

GlobalVariable Property TWPTE_PotionNumberOfEffects Auto

FormList Property TWPTE_SavedPotionList Auto

Potion PotionToTrack
Potion LastPotion

Import Utility
Import Input
Import PO3_SKSEFunctions
Import Game

Event OnInit()
	LastPotion = InitialLastPotion
EndEvent

Event OnItemAdded(Form akBaseItem, int aiItemCount, ObjectReference akItemReference, ObjectReference akSourceContainer)
	
	If UI.IsMenuOpen("GiftMenu")
		TWPTE_Failsafe.SetValue(1)
		HoldKey(1)
		Wait(0.1)
		ReleaseKey(1)
	EndIf

	PotionToTrack = (akBaseItem as Potion)

	If TWPTE_Failsafe.GetValue() == 1	
		If aiItemCount > 1			
			Int ReturnAmountPotions = aiItemCount - 1
			RemoveItem(PotionToTrack, ReturnAmountPotions, true, PlayerRef)
		EndIf
		
        If WPD_Blacklist.IsBlocked(PotionToTrack)
            RemoveItem(PotionToTrack, 1, True, PlayerRef)
            PotionToTrack = LastPotion
            TWPTE_Failsafe.SetValue(0)
            Debug.Notification("This liquid is blacklisted. Your sample was returned.")
            Return
        EndIf
		If !PotionToTrack.IsPoison()
			AssignEffects(TheWhitePhialFull, MS12WhitePhialEffect, PotionToTrack, "custom")
		Else
			AssignEffects(TheWhitePhialFullPoison, MS12WhitePhialEffectPoison, PotionToTrack, "custompoison")
		EndIf

		LastPotion = PotionToTrack
	EndIf

	TWPTE_Failsafe.SetValue(0)
EndEvent

Function AssignEffects(Potion WhitePhial, MagicEffect EffectToIgnore, Potion PotionToUse, string AssignString)
    If WPD_Blacklist.IsBlocked(PotionToUse)
        RemoveItem(PotionToUse, 1, True, PlayerRef)
        Debug.Notification("This liquid is blacklisted. Your sample was returned.")
        Return
    EndIf
    ; Snapshot while the selected sample still exists in this actor's inventory.
    Potion originalSample = PotionToUse
    Potion protectedLiquid = WPD_Storage.Protect(originalSample)
    If !protectedLiquid
        RemoveItem(originalSample, 1, True, PlayerRef)
        Debug.Notification("This liquid could not be safeguarded. Your sample was returned.")
        Return
    EndIf
    PotionToUse = protectedLiquid
    PotionToTrack = protectedLiquid
    LastPotion = protectedLiquid
    
	TWPTE_SavedPotionList.Revert()
	TWPTE_SavedPotionList.AddForm(PotionToUse)
	TWPTE_PotionNumberOfEffects.SetValue(PotionToUse.GetNumEffects())
	
	CopyPasteEffects(WhitePhial, EffectToIgnore, PotionToUse)

	RemoveItem(originalSample, 1, true)

	MS12PostQuest.UnregisterForUpdateGameTime()
	(MS12PostQuest as MS12PostQuestScript).SetReward(AssignString)

	Wait(0.1)
	
	PlayerRef.RemoveItem(MS12WhitePhial, 1, true)
	(MS12PostQuest As MS12PostQuestScript).RemoveDuplicateWhitePhial(PlayerRef)

	ObjectReference empty = PlayerRef.PlaceAtMe(MS12WhitePhial, 1)
	WhitePhialPotion.ForceRefTo(empty)
	PlayerRef.AddItem(empty, 1, true)
	(MS12PostQuest as MS12PostQuestScript).RegisterForSingleUpdateGameTime(TWPTE_ResetHours.GetValue())

	TWPTE_ReassignedMessage.Show()
EndFunction

Function CopyPasteEffects(Potion WhitePhial, MagicEffect EffectToIgnore, Potion PotionToUse)
    If !WhitePhial || !EffectToIgnore || !PotionToUse || SKSE.GetPluginVersion("WhitePhialNames") < 33554432
        Return
    EndIf
    If !WPD_Storage.IsReady()
        Return
    EndIf
	AddMagicEffectToPotion(WhitePhial, TWPTE_DummyEffect, 0.0, 0, 0, 0, ConditionListDummy)			

	; removes existing effects.
	int indexWP = WhitePhial.GetNumEffects() - 1				; For example, if potion has 3 effects, those are 0,1,2 -> last value is 2
	While indexWP >= 0
		MagicEffect effectWP = WhitePhial.GetNthEffectMagicEffect(indexWP)
		If effectWP != TWPTE_DummyEffect
;			RemoveEffectItemFromPotion(WhitePhial, LastPotion, indexWP)
			RemoveMagicEffectFromPotion(WhitePhial, effectWP, WhitePhial.GetNthEffectMagnitude(indexWP) As Float, WhitePhial.GetNthEffectArea(indexWP) As Int, WhitePhial.GetNthEffectDuration(indexWP) As Int)
		EndIf			
		indexWP -= 1
	EndWhile

	; add new effects.
	int numEffectsNew = PotionToUse.GetNumEffects()
	int indexNew = 0
	While indexNew < numEffectsNew
		MagicEffect effectNew = PotionToUse.GetNthEffectMagicEffect(indexNew)
;		AddEffectItemToPotion(WhitePhial, PotionToUse, indexNew)
		AddMagicEffectToPotion(WhitePhial, effectNew, PotionToUse.GetNthEffectMagnitude(indexNew) As Float, PotionToUse.GetNthEffectArea(indexNew) As Int, PotionToUse.GetNthEffectDuration(indexNew) As Int, 0.0, ConditionListDummy)
		indexNew += 1
	EndWhile			
	
	RemoveMagicEffectFromPotion(WhitePhial, TWPTE_DummyEffect, 0.0, 0, 0, 0)
	AddMagicEffectToPotion(WhitePhial, EffectToIgnore, 0.0, 0, 1, 0.0, ConditionListDummy)
EndFunction

Bool Function ProtectStoredLiquid()
    If !TWPTE_SavedPotionList || TWPTE_SavedPotionList.GetSize() < 1
        Return False
    EndIf
    Potion originalLiquid = TWPTE_SavedPotionList.GetAt(0) as Potion
    Potion protectedLiquid = WPD_Storage.Protect(originalLiquid)
    If !protectedLiquid
        Return False
    EndIf
    If protectedLiquid != originalLiquid
        TWPTE_SavedPotionList.Revert()
        TWPTE_SavedPotionList.AddForm(protectedLiquid)
    EndIf
    PotionToTrack = protectedLiquid
    LastPotion = protectedLiquid
    Return True
EndFunction
