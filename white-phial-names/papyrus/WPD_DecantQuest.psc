Scriptname WPD_DecantQuest extends Quest

Quest Property PhialQuest Auto
GlobalVariable Property RefillHours Auto
GlobalVariable Property ReassignBusy Auto
Potion[] Property DefaultPhials Auto
Potion[] Property DefaultBottles Auto

Bool busy = False

Function Decant(Actor player)
    If busy
        Return
    EndIf
    busy = True
    TryDecant(player)
    busy = False
EndFunction

Bool Function HasFullPhial(Actor player)
    MS12PostQuestScript original = PhialQuest as MS12PostQuestScript
    If !player || !original || !PhialQuest.IsRunning()
        Return False
    EndIf
    Potion full = original.Replicated
    Return full && original.TWPTE_WhitePhialList && original.TWPTE_WhitePhialList.HasForm(full) && player.GetItemCount(full) > 0
EndFunction

Bool Function AutoDecant(Actor player)
    If busy
        Return False
    EndIf
    busy = True
    Bool result = TryDecant(player, True)
    busy = False
    Return result
EndFunction

Function Notice(String message, Bool quiet)
    WPD_Storage.Trace(message)
    If !quiet
        Debug.Notification(message)
    EndIf
EndFunction

Bool Function TryDecant(Actor player, Bool quiet = False)
    WPD_Storage.Trace("TryDecant start (2.1.0); quiet=" + quiet)
    If SKSE.GetPluginVersion("WhitePhialNames") < 33554432
        Notice("White Phial safeguards are unavailable. Check the installed DLL.", quiet)
        Return False
    EndIf
    If !WPD_Storage.IsReady()
        Notice("White Phial safeguards are not ready. Check WhitePhialNames.log.", quiet)
        Return False
    EndIf
    If !player || player != Game.GetPlayer() || player.IsDead()
        Return False
    EndIf
    MS12PostQuestScript original = PhialQuest as MS12PostQuestScript
    If !original || !PhialQuest.IsRunning() || !original.PhialAlias
        Notice("The White Phial is not ready to refill yet.", quiet)
        Return False
    EndIf
    If !RefillHours || !ReassignBusy || RefillHours.GetValue() <= 0.0
        Notice("Set a positive White Phial refill time before decanting.", quiet)
        Return False
    EndIf
    If ReassignBusy.GetValue() != 0.0 || Utility.IsInMenuMode()
        Notice("Wait until the White Phial has finished changing its contents.", quiet)
        Return False
    EndIf

    Potion full = original.Replicated
    MiscObject empty = original.EmptyPhial
    FormList phials = original.TWPTE_WhitePhialList
    If !full || !empty || !phials || !phials.HasForm(full) || player.GetItemCount(full) < 1
        Notice("You need a full White Phial in your inventory.", quiet)
        Return False
    EndIf
    If WPD_Blacklist.IsBlocked(full)
        Notice("This liquid is blacklisted. The phial was not emptied.", quiet)
        Return False
    EndIf
    ; The original refill function removes duplicate phials. Do not let it
    ; unexpectedly consume extra phials or a second empty quest item here.
    If player.GetItemCount(phials) != 1
        Notice("Carry only one White Phial before decanting it.", quiet)
        Return False
    EndIf
    MS12WhitePhialScript phial = original.PhialAlias as MS12WhitePhialScript
    ObjectReference tracked = original.PhialAlias.GetReference()
    If !phial || !tracked || tracked.GetBaseObject() != full
        Notice("The White Phial is updating. Try again in a moment.", quiet)
        Return False
    EndIf

    If full == original.CustomPotion || full == original.CustomPotionPoison
        WPD_Storage.Trace("Checking stored custom liquid before decanting")
        If !ProtectCurrentLiquid()
            Notice("The chosen liquid could not be safeguarded. The phial was not emptied.", quiet)
            Return False
        EndIf
    EndIf
    Potion bottled = ResolveBottle(original, full)
    If !bottled || phials.HasForm(bottled)
        Notice("The White Phial's chosen liquid could not be read.", quiet)
        Return False
    EndIf
    ; No silent strength changes: compare actual runtime effects, not names
    ; or a guessed corresponding vanilla potion.
    If !SameLiquid(original, full, bottled)
        Notice("The White Phial's effects do not match its saved liquid. Reassign the liquid, then try again after it refills.", quiet)
        Return False
    EndIf

    ; Recheck after the effect reads in case another hotkey consumed it.
    If original.Replicated != full || ReassignBusy.GetValue() != 0.0 || player.GetItemCount(full) != 1 || player.GetItemCount(phials) != 1
        Return False
    EndIf
    If full == original.CustomPotionPoison
        ; Its OnItemRemoved compares a cached global poison-use statistic.
        ; An unrelated poison used since the last phial dose can leave that
        ; counter stale. The original OnInit ONLY refreshes the counter.
        ; Calling the method synchronizes it without replacing its script,
        ; altering statistics or triggering the original repair/load logic.
        MS12PostQuestPlayerAliasScript poisonTracker = PhialQuest.GetAlias(2) as MS12PostQuestPlayerAliasScript
        If !poisonTracker
            Notice("The White Phial's poison tracker is not ready.", quiet)
            Return False
        EndIf
        poisonTracker.OnInit()
    EndIf
    ; Use the original alias swap and game-hour timer. No EquipItem, potion
    ; consumption, spell cast, poison application or custom refill timer.
    TraceRefill("Before SetForRefill", player, full, empty, tracked)
    phial.SetForRefill(player)
    ObjectReference nowTracked = original.PhialAlias.GetReference()
    TraceRefill("After SetForRefill", player, full, empty, nowTracked)
    If player.GetItemCount(full) == 0 && player.GetItemCount(empty) == 1 && nowTracked && nowTracked.GetBaseObject() == empty
        phial.CurrentContainer = player
        GiveBottle(player, bottled, quiet)
        WPD_Storage.Trace("Decant completed; bottle=" + bottled)
        Return True
    Else
        Debug.Trace("[White Phial Decanting] Original SetForRefill did not produce the expected empty phial; no bottle was granted.")
        Notice("The White Phial could not finish decanting.", quiet)
    EndIf
    Return False
EndFunction

Function TraceRefill(String phase, Actor player, Potion full, MiscObject empty, ObjectReference tracked)
    Form trackedBase = None
    If tracked
        trackedBase = tracked.GetBaseObject()
    EndIf
    WPD_Storage.Trace(phase + ": full=" + full + "; full count=" + player.GetItemCount(full) + "; empty=" + empty + "; empty count=" + player.GetItemCount(empty) + "; tracked=" + tracked + "; tracked base=" + trackedBase)
EndFunction

Function GiveBottle(Actor player, Potion bottled, Bool quiet)
    String chosenName = ""
    If SKSE.GetPluginVersion("WhitePhialNames") >= 0
        chosenName = WPD_Names.GetBottleName(bottled)
    EndIf
    If chosenName != ""
        ; A base-form AddItem cannot carry a bottle's ExtraTextDisplayData.
        ; Name this single disabled reference, then move it into inventory.
        ; Its extra data follows the item; the shared potion form is untouched.
        ObjectReference dose = player.PlaceAtMe(bottled, 1, False, True)
        If dose
            If dose.SetDisplayName(chosenName, True)
                player.AddItem(dose, 1, quiet)
                Return
            EndIf
            dose.Delete()
        EndIf
        Debug.Trace("[White Phial Decanting] Could not name the dose; granting the ordinary bottle.")
    EndIf
    player.AddItem(bottled, 1, quiet)
EndFunction

Potion Function ResolveBottle(MS12PostQuestScript original, Potion full)
    If full == original.CustomPotion || full == original.CustomPotionPoison
        If original.TWPTE_SavedPotionList && original.TWPTE_SavedPotionList.GetSize() > 0
            Return original.TWPTE_SavedPotionList.GetAt(0) as Potion
        EndIf
        Return None
    EndIf
    If !DefaultPhials || !DefaultBottles || DefaultPhials.Length != DefaultBottles.Length
        Return None
    EndIf
    Int i = 0
    While i < DefaultPhials.Length
        If full == DefaultPhials[i]
            Return DefaultBottles[i]
        EndIf
        i += 1
    EndWhile
    Return None
EndFunction

Bool Function SameLiquid(MS12PostQuestScript original, Potion full, Potion bottled)
    If full.IsPoison() != bottled.IsPoison() || bottled.GetNumEffects() < 1
        Return False
    EndIf
    Int i = 0
    Int j = 0
    Int markerCount = 0
    Int bottleEffects = bottled.GetNumEffects()
    While i < full.GetNumEffects()
        MagicEffect effect = full.GetNthEffectMagicEffect(i)
        If effect == original.MS12WhitePhialEffect || effect == original.MS12WhitePhialEffectPoison
            markerCount += 1
        Else
            If !effect || j >= bottleEffects
                Return False
            EndIf
            If effect != bottled.GetNthEffectMagicEffect(j)
                Return False
            EndIf
            If full.GetNthEffectMagnitude(i) != bottled.GetNthEffectMagnitude(j)
                Return False
            EndIf
            If full.GetNthEffectArea(i) != bottled.GetNthEffectArea(j) || full.GetNthEffectDuration(i) != bottled.GetNthEffectDuration(j)
                Return False
            EndIf
            j += 1
        EndIf
        i += 1
    EndWhile
    Return markerCount == 1 && j == bottleEffects
EndFunction


Bool Function ProtectCurrentLiquid(Bool restoreEffects = False)
    If SKSE.GetPluginVersion("WhitePhialNames") < 33554432
        Return False
    EndIf
    If !WPD_Storage.IsReady() || !ReassignBusy || ReassignBusy.GetValue() != 0.0
        Return False
    EndIf
    MS12PostQuestScript original = PhialQuest as MS12PostQuestScript
    If !original || !PhialQuest.IsRunning()
        Return False
    EndIf
    If original.Replicated != original.CustomPotion && original.Replicated != original.CustomPotionPoison
        Return True
    EndIf
    TWPTE_WhitePhialActorScript selector = original.TWPTE_WhitePhialActorRef as TWPTE_WhitePhialActorScript
    If !selector
        Return False
    EndIf
    If !selector.ProtectStoredLiquid()
        Return False
    EndIf
    ; The original OnPlayerLoadGame event may precede the native post-load
    ; validation. Finish restoring the phial once gameplay has resumed.
    If restoreEffects
        Potion full = original.Replicated
        Potion liquid = original.TWPTE_SavedPotionList.GetAt(0) as Potion
        If !SameLiquid(original, full, liquid)
            If full == original.CustomPotionPoison
                selector.CopyPasteEffects(full, original.MS12WhitePhialEffectPoison, liquid)
            Else
                selector.CopyPasteEffects(full, original.MS12WhitePhialEffect, liquid)
            EndIf
        EndIf
    EndIf
    Return True
EndFunction
