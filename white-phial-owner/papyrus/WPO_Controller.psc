Scriptname WPO_Controller extends Quest

Spell Property TraitAbility Auto
GlobalVariable Property Chosen Auto
GlobalVariable Property GiftGranted Auto
Cell Property ShopCell Auto
FormList Property Phials Auto
MiscObject Property EmptyPhial Auto
Quest Property PhialQuest Auto
GlobalVariable Property RefillHours Auto

Bool updating = False
Bool warned = False
Float nextScan = 0.0
Quest acquisitionQuest
Quest repairQuest

Event OnInit()
    Resume()
EndEvent

Function Resume()
    updating = False
    nextScan = 0.0
    RegisterForCrosshairRef()
    RegisterForSingleUpdate(0.1)
EndFunction

Function ScanSoon()
    nextScan = 0.0
    RegisterForSingleUpdate(0.1)
EndFunction

Bool Function IsRestricted()
    Return Chosen && Chosen.GetValue() == 1.0
EndFunction

Event OnUpdate()
    If updating
        RegisterForSingleUpdate(1.0)
        Return
    EndIf
    updating = True
    Actor player = Game.GetPlayer()
    If player && TraitAbility && Chosen && GiftGranted && ShopCell && Phials && EmptyPhial && PhialQuest && RefillHours
        If player.HasSpell(TraitAbility)
            ; Permanent, saved choice. Removing the item or trait never reopens the shop.
            If !IsRestricted()
                Chosen.SetValue(1.0)
                nextScan = 0.0
                Debug.Trace("[White Phial Owner] Selected: shop restriction committed.")
            EndIf
            If GiftGranted.GetValue() != 2.0
                GrantEmptyPhial(player)
            EndIf
        EndIf
        If IsRestricted()
            Float now = Utility.GetCurrentRealTime()
            If now >= nextScan
                CloseQuestAccess()
                ScanCell(ShopCell)
                Cell currentCell = player.GetParentCell()
                If currentCell && currentCell != ShopCell
                    ScanCell(currentCell)
                EndIf
                MaintainDoors()
                nextScan = now + 2.0
            EndIf
        EndIf
    ElseIf !warned
        warned = True
        Debug.Trace("[White Phial Owner] Required property unavailable; will retry.")
    EndIf
    updating = False
    RegisterForSingleUpdate(1.0)
EndEvent

Function GrantEmptyPhial(Actor player)
    If player.GetItemCount(Phials) > 0
        ; Preserve an existing phial and its chosen liquid instead of making duplicates.
        GiftGranted.SetValue(2.0)
        Return
    EndIf
    If !PhialQuest.IsRunning()
        If !PhialQuest.Start()
            Debug.Trace("[White Phial Owner] Refill service could not start; gift pending.")
            Return
        EndIf
    EndIf
    MS12PostQuestScript service = PhialQuest as MS12PostQuestScript
    If !service || !service.PhialAlias
        Debug.Trace("[White Phial Owner] Original phial alias unavailable; gift pending.")
        Return
    EndIf
    MS12WhitePhialScript tracking = service.PhialAlias as MS12WhitePhialScript
    If !tracking
        Debug.Trace("[White Phial Owner] Original refill script unavailable; gift pending.")
        Return
    EndIf
    If !service.Replicated
        ; A newly granted empty phial uses the original healing essence by default.
        ; Original liquid reassignment and enchantment settings are left intact.
        service.Replicated = service.HealPotion
    EndIf
    If !service.Replicated
        Return
    EndIf
    GiftGranted.SetValue(1.0)
    ; This is the original mod's empty-object creation and refill schedule.
    ; Neither story quest is completed and no story reward function is called.
    tracking.SetForRefill(player)
    tracking.CurrentContainer = player
    If player.GetItemCount(EmptyPhial) > 0
        GiftGranted.SetValue(2.0)
        Debug.Trace("[White Phial Owner] One empty phial granted; original refill service active.")
    Else
        GiftGranted.SetValue(0.0)
        Debug.Trace("[White Phial Owner] Empty phial creation failed; will retry.")
    EndIf
EndFunction

Function CloseQuestAccess()
    ; MS12PostQuest is intentionally kept running: it is the refill service.
    If !acquisitionQuest
        acquisitionQuest = Quest.GetQuest("MS12")
    EndIf
    If !repairQuest
        repairQuest = Quest.GetQuest("MS12b")
    EndIf
    If acquisitionQuest && acquisitionQuest.IsRunning() && !acquisitionQuest.IsCompleted()
        acquisitionQuest.Stop()
        Debug.Trace("[White Phial Owner] Acquisition quest stopped without awarding completion.")
    EndIf
    If repairQuest && repairQuest.IsRunning() && !repairQuest.IsCompleted()
        repairQuest.Stop()
        Debug.Trace("[White Phial Owner] Repair story quest stopped without awarding completion.")
    EndIf
EndFunction

Event OnCrosshairRefChange(ObjectReference ref)
    If IsRestricted() && ref
        Form base = ref.GetBaseObject()
        If base && base.GetType() == 29
            ConsiderDoor(ref)
        EndIf
    EndIf
EndEvent

Function ScanCell(Cell targetCell)
    If !targetCell
        Return
    EndIf
    Int i = targetCell.GetNumRefs(29)
    While i > 0
        i -= 1
        ConsiderDoor(targetCell.GetNthRef(i, 29))
    EndWhile
EndFunction

Function ConsiderDoor(ObjectReference candidate)
    If !candidate || !IsRestricted()
        Return
    EndIf
    ObjectReference destination = PO3_SKSEFunctions.GetDoorDestination(candidate)
    If !destination
        Return
    EndIf
    If candidate.GetParentCell() == ShopCell && destination.GetParentCell() != ShopCell
        ; Only close the outside entrance. The inside exit remains usable.
        RememberDoor(destination)
    ElseIf candidate.GetParentCell() != ShopCell && destination.GetParentCell() == ShopCell
        RememberDoor(candidate)
    EndIf
EndFunction

Function RememberDoor(ObjectReference doorRef)
    Int i = 1
    ReferenceAlias vacant
    While i <= 8
        ReferenceAlias slot = GetAlias(i) as ReferenceAlias
        If slot
            If slot.GetReference() == doorRef
                (slot as WPO_DoorAlias).Enforce()
                Return
            ElseIf !vacant && !slot.GetReference()
                vacant = slot
            EndIf
        EndIf
        i += 1
    EndWhile
    If vacant
        vacant.ForceRefTo(doorRef)
        (vacant as WPO_DoorAlias).Enforce()
        Debug.Trace("[White Phial Owner] Shop entrance protected: " + doorRef)
    Else
        ; Preserve the lock even if a large location overhaul adds over eight entrances.
        doorRef.BlockActivation(True)
        doorRef.SetLockLevel(255)
        doorRef.Lock(True)
        Debug.Trace("[White Phial Owner] Additional entrance locked; all eight watch aliases occupied.")
    EndIf
EndFunction

Function MaintainDoors()
    Int i = 1
    While i <= 8
        WPO_DoorAlias slot = GetAlias(i) as WPO_DoorAlias
        If slot && slot.GetReference()
            slot.Enforce()
        EndIf
        i += 1
    EndWhile
EndFunction
