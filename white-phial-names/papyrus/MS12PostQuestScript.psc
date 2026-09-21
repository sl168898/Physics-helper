Scriptname MS12PostQuestScript extends Quest  

MiscObject Property EmptyPhial auto

Potion Property Replicated auto

Potion Property HealPotion auto
Potion Property ResistMagicPotion auto
Potion Property ResistDamagePotion auto
Potion Property ImproveMagicPotion auto
Potion Property ImproveDamagePotion auto
Potion Property ImproveSneakPotion auto

Potion Property CustomPotion auto
Potion Property CustomPotionPoison auto

Sound property TWPTE_UIAlchemyCreatePoisonSound auto

Message Property MS12RefillMessage auto

ReferenceAlias Property PhialAlias auto

FormList Property TWPTE_WhitePhialList Auto

GlobalVariable Property TWPTE_PotionNumberOfEffects	Auto

FormList Property TWPTE_SavedPotionList Auto

MagicEffect Property MS12WhitePhialEffect Auto
MagicEffect Property MS12WhitePhialEffectPoison Auto

Actor Property TWPTE_WhitePhialActorRef Auto

Actor Property PlayerRef Auto

Function SetReward(string rewardType)
    Potion candidate = Replicated
	if (rewardType == "heal")
		candidate = HealPotion
	elseif (rewardType == "resist magic")		
		candidate = ResistMagicPotion
	elseif (rewardType == "resist damage")		
		candidate = ResistDamagePotion
	elseif (rewardType == "improve magic")		
		candidate = ImproveMagicPotion
	elseif (rewardType == "improve damage")		
		candidate = ImproveDamagePotion
	elseif (rewardType == "improve sneak")		
		candidate = ImproveSneakPotion

	elseif (rewardType == "custom")	
		candidate = CustomPotion	
	elseif (rewardType == "custompoison")
		candidate = CustomPotionPoison

;	else
; 		Debug.Trace("MS12: Trying to align phial to unknown type.", 2)
	endif

    If WPD_Blacklist.IsBlocked(candidate)
        Debug.Notification("This liquid is blacklisted. The White Phial was not changed.")
        Return
    EndIf
    Replicated = candidate
	TWPTE_UIAlchemyCreatePoisonSound.Play(PlayerRef)

	RemoveDuplicateWhitePhial(PlayerRef)	
	ObjectReference rep = PlayerRef.PlaceAtMe(Replicated, 1)
	PhialAlias.ForceRefTo(rep)
	PlayerRef.AddItem(rep, 1, true)
	If PlayerRef.GetItemCount(EmptyPhial) > 0					; edit
		PlayerRef.RemoveItem(Replicated, 1, true)				; edit
	EndIf														; edit
EndFunction

Event OnUpdateGameTime()
	RewardCheck()
EndEvent

Function RewardCheck(bool quiet = false)
    If WPD_Blacklist.IsBlocked(Replicated) || !WPD_Storage.IsReady()
        Return
    EndIf
; 	Debug.Trace("MS12: White phial attempting to refill...")
	if (PlayerRef.GetItemCount(EmptyPhial) > 0)
		MS12RefillMessage.Show()
	endif

	If Replicated == CustomPotion
		If (CustomPotion.GetNumEffects() - 1) != (TWPTE_PotionNumberOfEffects.GetValue() As Int)
;			debug.notification("White Phial potion mismatch occured. Redoing effects.")
			(TWPTE_WhitePhialActorRef as TWPTE_WhitePhialActorScript).CopyPasteEffects(CustomPotion, MS12WhitePhialEffect, (TWPTE_SavedPotionList.GetAt(0) As Potion))
;			debug.notification("Redoing effects done.")
		EndIf
	ElseIf Replicated == CustomPotionPoison
		If (CustomPotionPoison.GetNumEffects() - 1) != (TWPTE_PotionNumberOfEffects.GetValue() As Int)
;			debug.notification("White Phial poison mismatch occured. Redoing effects.")
			(TWPTE_WhitePhialActorRef as TWPTE_WhitePhialActorScript).CopyPasteEffects(CustomPotionPoison, MS12WhitePhialEffectPoison, (TWPTE_SavedPotionList.GetAt(0) As Potion))
;			debug.notification("Redoing effects done.")
		EndIf
	EndIf

	(PhialAlias as MS12WhitePhialScript).Refill(Replicated)
EndFunction

Function RemoveDuplicateWhitePhial(ObjectReference akContainer)
	If akContainer != None
		If akContainer.GetItemCount(TWPTE_WhitePhialList) > 0
			int AmountOfForms = TWPTE_WhitePhialList.GetSize() - 1			; if there are 5 forms, those are 0, 1, 2, 3, 4. -> last value is 4.
			While AmountOfForms >= 0
				akContainer.RemoveItem(TWPTE_WhitePhialList.GetAt(AmountOfForms), 999, true)
				AmountOfForms -= 1
			EndWhile
		EndIf
	EndIf
EndFunction

Function LimitGiftToOne()
	RegisterForMenu("GiftMenu")
EndFunction

Event OnMenuOpen(String MenuName)
	If MenuName == "GiftMenu"
		UnregisterForMenu("GiftMenu")
		Utility.WaitMenuMode(0.1)
		UI.SetInt("GiftMenu", "_root.Menu_mc._quantityMinCount", -12878323)

		While UI.IsMenuOpen("GiftMenu")
			Utility.Wait(0.1)
		EndWhile
	EndIf
EndEvent

; Called once when the menu removes the currently selected liquid's rule.
Function WPD_ResumeAfterBlacklistChange()
    If !Replicated || WPD_Blacklist.IsBlocked(Replicated) || !WPD_Storage.IsReady()
        Return
    EndIf
    ObjectReference tracked = PhialAlias.GetReference()
    If tracked && tracked.GetBaseObject() == EmptyPhial
        GlobalVariable hours = Game.GetFormFromFile(0x808, "The White Phial - Tweaks and Enhancements.esp") As GlobalVariable
        If hours && hours.GetValue() > 0.0
            UnregisterForUpdateGameTime()
            RegisterForSingleUpdateGameTime(hours.GetValue())
        EndIf
    EndIf
EndFunction
